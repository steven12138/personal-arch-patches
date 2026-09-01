#include <libnotify/notify.h>
#include <glib-unix.h>
#include <sqlite3.h>

#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

constexpr std::size_t kChunkSize = 8 * 1024 * 1024;
constexpr std::uint64_t kMaxRegionSize = 512ULL * 1024 * 1024;
constexpr char kVersion[] = "1.1.1";

struct Databases {
    fs::path session;
    fs::path contact;
    fs::path message;
};

struct ChatEntry {
    std::string username;
    std::string title;
    std::string sender;
    std::string content;
    std::string summary;
    int unread = 0;
    std::string lastTimestamp;
    std::string sortTimestamp;
};

struct Notice {
    std::string signature;
    std::string title;
    std::string body;
};

struct ExtractResult {
    bool ok = false;
    std::optional<Notice> notice;
    std::string error;
};

struct SnapshotDir {
    fs::path path;
    SnapshotDir() = default;
    explicit SnapshotDir(fs::path value) : path(std::move(value)) {}
    SnapshotDir(const SnapshotDir &) = delete;
    SnapshotDir &operator=(const SnapshotDir &) = delete;
    SnapshotDir(SnapshotDir &&other) noexcept : path(std::move(other.path)) {
        other.path.clear();
    }
    SnapshotDir &operator=(SnapshotDir &&other) noexcept {
        if (this != &other) {
            std::error_code ec;
            if (!path.empty()) fs::remove_all(path, ec);
            path = std::move(other.path);
            other.path.clear();
        }
        return *this;
    }
    ~SnapshotDir() {
        std::error_code ec;
        if (!path.empty()) fs::remove_all(path, ec);
    }
};

static fs::path homeDir() {
    const char *home = std::getenv("HOME");
    if (!home || !*home) throw std::runtime_error("HOME is not set");
    return home;
}

static fs::path runtimeDir() {
    const char *runtime = std::getenv("XDG_RUNTIME_DIR");
    fs::path base = runtime && *runtime
        ? fs::path(runtime)
        : fs::path("/run/user") / std::to_string(getuid());
    fs::path dir = base / "wechat-universal-notify";
    fs::create_directories(dir);
    chmod(dir.c_str(), 0700);
    return dir;
}

static fs::path stateDir() {
    const char *state = std::getenv("XDG_STATE_HOME");
    fs::path base = state && *state ? fs::path(state) : homeDir() / ".local/state";
    fs::path dir = base / "wechat-universal-notify";
    fs::create_directories(dir);
    chmod(dir.c_str(), 0700);
    return dir;
}

static std::string readText(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

static bool writePrivate(const fs::path &path, const std::string &text) {
    fs::path temporary = path;
    temporary += ".tmp." + std::to_string(getpid());
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << text;
        if (!out) return false;
    }
    chmod(temporary.c_str(), 0600);
    std::error_code ec;
    fs::rename(temporary, path, ec);
    if (ec) fs::remove(temporary, ec);
    return !ec;
}

static std::optional<pid_t> findWechatPid() {
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator("/proc", ec)) {
        std::string name = entry.path().filename();
        if (name.empty() || !std::all_of(name.begin(), name.end(), ::isdigit)) continue;
        std::ifstream cmdline(entry.path() / "cmdline", std::ios::binary);
        std::string command;
        std::getline(cmdline, command, '\0');
        if (command == "/opt/wechat-universal/wechat") {
            try { return static_cast<pid_t>(std::stoi(name)); }
            catch (...) { }
        }
    }
    return std::nullopt;
}

static std::optional<Databases> findDatabases() {
    fs::path root = homeDir() / "Documents/WeChat_Data/xwechat_files";
    std::error_code ec;
    if (!fs::is_directory(root, ec)) return std::nullopt;

    std::optional<Databases> newest;
    fs::file_time_type newestTime{};
    bool have = false;
    for (const auto &entry : fs::directory_iterator(root, ec)) {
        fs::path storage = entry.path() / "db_storage";
        Databases candidate{
            storage / "session/session.db",
            storage / "contact/contact.db",
            storage / "message/message_0.db",
        };
        if (!fs::is_regular_file(candidate.session, ec) ||
            !fs::is_regular_file(candidate.contact, ec)) continue;
        if (!fs::is_regular_file(candidate.message, ec)) candidate.message.clear();
        auto changed = fs::last_write_time(candidate.session, ec);
        if (!have || (!ec && changed > newestTime)) {
            have = true;
            newest = candidate;
            newestTime = changed;
        }
    }
    return newest;
}

struct Fingerprint {
    bool exists = false;
    dev_t device{};
    ino_t inode{};
    off_t size{};
    timespec modified{};
    timespec changed{};
    bool operator==(const Fingerprint &other) const {
        return exists == other.exists && device == other.device && inode == other.inode &&
               size == other.size && modified.tv_sec == other.modified.tv_sec &&
               modified.tv_nsec == other.modified.tv_nsec && changed.tv_sec == other.changed.tv_sec &&
               changed.tv_nsec == other.changed.tv_nsec;
    }
};

static Fingerprint fingerprint(const fs::path &path) {
    struct stat st{};
    if (lstat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) return {};
    return {true, st.st_dev, st.st_ino, st.st_size, st.st_mtim, st.st_ctim};
}

static std::array<Fingerprint, 2> familyFingerprint(const fs::path &database) {
    return {fingerprint(database), fingerprint(database.string() + "-wal")};
}

static bool stableCopy(const fs::path &source, const fs::path &destination) {
    for (int attempt = 0; attempt < 12; ++attempt) {
        auto before = familyFingerprint(source);
        if (!before[0].exists) return false;
        std::this_thread::sleep_for(100ms);
        if (familyFingerprint(source) != before) continue;

        std::error_code ec;
        fs::remove(destination, ec);
        fs::remove(destination.string() + "-wal", ec);
        fs::remove(destination.string() + "-shm", ec);
        fs::copy_file(source, destination, fs::copy_options::overwrite_existing, ec);
        if (ec) continue;
        chmod(destination.c_str(), 0600);
        if (before[1].exists) {
            fs::copy_file(source.string() + "-wal", destination.string() + "-wal",
                          fs::copy_options::overwrite_existing, ec);
            if (ec) continue;
            chmod((destination.string() + "-wal").c_str(), 0600);
        }
        if (familyFingerprint(source) == before) return true;
    }
    return false;
}

static std::optional<std::pair<SnapshotDir, Databases>> snapshot(const Databases &live) {
    std::string pattern = (runtimeDir() / ".snapshot.XXXXXX").string();
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back('\0');
    char *created = mkdtemp(name.data());
    if (!created) return std::nullopt;
    SnapshotDir holder{created};
    chmod(created, 0700);
    Databases copy{holder.path / "session.db", holder.path / "contact.db", {}};
    if (!stableCopy(live.session, copy.session) || !stableCopy(live.contact, copy.contact))
        return std::nullopt;
    return std::make_pair(std::move(holder), copy);
}

static std::string bytesToHex(const unsigned char *data, std::size_t length) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result(length * 2, '0');
    for (std::size_t i = 0; i < length; ++i) {
        result[i * 2] = digits[data[i] >> 4];
        result[i * 2 + 1] = digits[data[i] & 15];
    }
    return result;
}

static std::optional<std::string> databaseSalt(const fs::path &database) {
    std::array<unsigned char, 16> header{};
    std::ifstream in(database, std::ios::binary);
    in.read(reinterpret_cast<char *>(header.data()), header.size());
    if (in.gcount() != static_cast<std::streamsize>(header.size())) return std::nullopt;
    return bytesToHex(header.data(), header.size());
}

static bool isHex(unsigned char value) {
    return (value >= '0' && value <= '9') ||
           (value >= 'a' && value <= 'f') ||
           (value >= 'A' && value <= 'F');
}

static std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

static std::map<std::string, std::string> loadKeyCache() {
    std::map<std::string, std::string> keys;
    std::istringstream input(readText(stateDir() / "keys"));
    std::string format;
    input >> format;
    if (format != "v1") return {};
    std::string salt, key;
    while (input >> salt >> key) keys[salt] = key;
    return keys;
}

static void saveKeyCache(const std::map<std::string, std::string> &keys) {
    std::ostringstream output;
    output << "v1\n";
    for (const auto &[salt, key] : keys) output << salt << ' ' << key << '\n';
    writePrivate(stateDir() / "keys", output.str());
}

static std::map<std::string, std::string> scanKeys(
    pid_t pid, const std::vector<std::string> &salts) {
    std::map<std::string, std::string> found;
    std::ifstream maps("/proc/" + std::to_string(pid) + "/maps");
    int memory = open(("/proc/" + std::to_string(pid) + "/mem").c_str(), O_RDONLY | O_CLOEXEC);
    if (!maps || memory < 0) return found;

    std::string line;
    std::vector<unsigned char> chunk(kChunkSize + 128);
    while (std::getline(maps, line) && found.size() < salts.size()) {
        std::istringstream fields(line);
        std::string range, permissions, offset, device, inode;
        fields >> range >> permissions >> offset >> device >> inode;
        std::string mappedPath;
        std::getline(fields, mappedPath);
        mappedPath.erase(0, mappedPath.find_first_not_of(' '));
        if (permissions.size() < 4 || permissions[0] != 'r' || permissions[1] != 'w' ||
            permissions[3] != 'p') continue;
        if (!mappedPath.empty() && mappedPath[0] != '[') continue;
        auto dash = range.find('-');
        if (dash == std::string::npos) continue;
        std::uint64_t start = std::stoull(range.substr(0, dash), nullptr, 16);
        std::uint64_t end = std::stoull(range.substr(dash + 1), nullptr, 16);
        if (end <= start || end - start > kMaxRegionSize) continue;

        std::size_t overlap = 0;
        for (std::uint64_t position = start; position < end && found.size() < salts.size();) {
            std::size_t requested = static_cast<std::size_t>(
                std::min<std::uint64_t>(kChunkSize, end - position));
            ssize_t count = pread(memory, chunk.data() + overlap, requested,
                                  static_cast<off_t>(position));
            if (count <= 0) break;
            std::size_t available = overlap + static_cast<std::size_t>(count);
            for (std::size_t i = 0; i + 98 <= available; ++i) {
                if (chunk[i] != 'x' || chunk[i + 1] != '\'') continue;
                bool valid = true;
                for (std::size_t j = 0; j < 96; ++j)
                    if (!isHex(chunk[i + 2 + j])) { valid = false; break; }
                if (!valid) continue;
                std::string key(reinterpret_cast<char *>(chunk.data() + i + 2), 96);
                key = lower(key);
                std::string salt = key.substr(64, 32);
                if (std::find(salts.begin(), salts.end(), salt) != salts.end())
                    found.emplace(salt, key);
            }
            overlap = std::min<std::size_t>(128, available);
            std::memmove(chunk.data(), chunk.data() + available - overlap, overlap);
            position += static_cast<std::size_t>(count);
        }
    }
    close(memory);
    return found;
}

class Database {
public:
    Database(const fs::path &path, const std::string &key) {
        if (sqlite3_open_v2(path.c_str(), &db_, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
            throw std::runtime_error("cannot open database snapshot");
        std::string pragma = "PRAGMA key=\"x'" + key + "'\";";
        char *error = nullptr;
        if (sqlite3_exec(db_, pragma.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
            std::string message = error ? error : "cannot set SQLCipher key";
            sqlite3_free(error);
            throw std::runtime_error(message);
        }
        sqlite3_stmt *check = nullptr;
        if (sqlite3_prepare_v2(db_, "SELECT count(*) FROM sqlite_schema", -1, &check, nullptr) != SQLITE_OK ||
            sqlite3_step(check) != SQLITE_ROW) {
            if (check) sqlite3_finalize(check);
            throw std::runtime_error("SQLCipher key check failed");
        }
        sqlite3_finalize(check);
    }
    ~Database() { if (db_) sqlite3_close(db_); }
    sqlite3 *get() const { return db_; }
private:
    sqlite3 *db_ = nullptr;
};

static std::string columnText(sqlite3_stmt *statement, int column) {
    const unsigned char *text = sqlite3_column_text(statement, column);
    return text ? reinterpret_cast<const char *>(text) : "";
}

static int asInt(const std::string &text) {
    try { return std::stoi(text); } catch (...) { return 0; }
}

static std::string normalized(std::string text, std::size_t limit = 180) {
    std::string result;
    bool space = false;
    for (unsigned char c : text) {
        if (std::isspace(c) || c == 0x1f) {
            space = !result.empty();
        } else {
            if (space) result.push_back(' ');
            result.push_back(static_cast<char>(c));
            space = false;
        }
    }
    if (result.size() > limit) result = result.substr(0, limit - 3) + "...";
    return result;
}

static std::string truncateBody(std::string text, std::size_t limit = 400) {
    if (text.size() > limit) text = text.substr(0, limit - 3) + "...";
    return text;
}

static std::string decodeEntities(std::string text) {
    const std::array<std::pair<const char *, const char *>, 7> entities{{
        {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"},
        {"&quot;", "\""}, {"&#39;", "'"}, {"&nbsp;", " "}, {"&apos;", "'"},
    }};
    for (const auto &[from, to] : entities) {
        std::string needle(from);
        std::size_t pos = 0;
        while ((pos = text.find(needle, pos)) != std::string::npos) {
            text.replace(pos, needle.size(), to);
            pos += std::strlen(to);
        }
    }
    return text;
}

static const char *messageTypeLabel(int msgType) {
    switch (msgType) {
        case 3: return "[图片]";
        case 34: return "[语音]";
        case 43: return "[视频]";
        case 47: return "[表情]";
        case 48: return "[位置]";
        case 49: return "[链接]";
        case 50: return "[视频通话]";
        case 57: return "[引用]";
        case 10000: return "[系统消息]";
        case 10002: return "[群系统消息]";
        default: return nullptr;
    }
}

static std::string stripSenderPrefix(const std::string &text) {
    auto newline = text.find('\n');
    if (newline != std::string::npos) {
        std::string first = text.substr(0, newline);
        if (!first.empty() && first.back() == ':') first.pop_back();
        bool idLike = first.rfind("wxid_", 0) == 0 || first.rfind("gh_", 0) == 0 ||
                      first.find('@') != std::string::npos;
        if (idLike) return text.substr(newline + 1);
    }
    return text;
}

static std::string plainContent(const std::string &raw, int msgType) {
    std::string text = stripSenderPrefix(raw);
    std::string stripped;
    bool inTag = false;
    for (char c : text) {
        if (c == '<') {
            inTag = true;
        } else if (c == '>') {
            inTag = false;
        } else if (!inTag) {
            stripped.push_back(c);
        }
    }
    text = decodeEntities(std::move(stripped));
    text = normalized(std::move(text), 220);
    const char *label = messageTypeLabel(msgType);
    if (label) {
        if (text.empty()) return label;
        return std::string(label) + " " + text;
    }
    if (text.empty()) return "[消息]";
    return text;
}

static std::optional<Notice> queryNotice(Database &sessionDb, Database &contactDb,
                                         Database *messageDb) {
    const char *sessionSql = R"SQL(
        SELECT username, coalesce(last_sender_display_name, ''), coalesce(summary, ''),
               unread_count, last_timestamp, sort_timestamp
        FROM SessionTable
        WHERE is_hidden=0 AND unread_count>0
          AND username NOT IN ('brandsessionholder','brandservicesessionholder',
                               'notifymessage','opencustomerservicemsg','@opencustomerservicemsg')
          AND username NOT LIKE 'gh_%'
        ORDER BY sort_timestamp DESC LIMIT 200
    )SQL";
    sqlite3_stmt *sessions = nullptr;
    if (sqlite3_prepare_v2(sessionDb.get(), sessionSql, -1, &sessions, nullptr) != SQLITE_OK)
        throw std::runtime_error("cannot query SessionTable");

    sqlite3_stmt *contact = nullptr;
    const char *contactSql =
        "SELECT coalesce(remark,''),coalesce(nick_name,''),coalesce(verify_flag,0),"
        "coalesce(chat_room_notify,0),coalesce(flag,0) FROM contact WHERE username=?1";
    if (sqlite3_prepare_v2(contactDb.get(), contactSql, -1, &contact, nullptr) != SQLITE_OK) {
        sqlite3_finalize(sessions);
        throw std::runtime_error("cannot query contacts");
    }

    std::vector<ChatEntry> entries;
    while (sqlite3_step(sessions) == SQLITE_ROW) {
        ChatEntry entry;
        entry.username = columnText(sessions, 0);
        entry.sender = columnText(sessions, 1);
        entry.summary = columnText(sessions, 2);
        entry.unread = asInt(columnText(sessions, 3));
        entry.lastTimestamp = columnText(sessions, 4);
        entry.sortTimestamp = columnText(sessions, 5);

        sqlite3_reset(contact);
        sqlite3_clear_bindings(contact);
        sqlite3_bind_text(contact, 1, entry.username.c_str(), -1, SQLITE_TRANSIENT);
        std::string remark, nickname;
        int verifyFlag = 0, roomNotify = 0, flags = 0;
        if (sqlite3_step(contact) == SQLITE_ROW) {
            remark = columnText(contact, 0);
            nickname = columnText(contact, 1);
            verifyFlag = sqlite3_column_int(contact, 2);
            roomNotify = sqlite3_column_int(contact, 3);
            flags = sqlite3_column_int(contact, 4);
        }

        bool chatroom = entry.username.ends_with("@chatroom");
        if ((!chatroom && verifyFlag != 0) || (chatroom && roomNotify != 1) || (flags & 512))
            continue;

        entry.title = !remark.empty() ? remark : (!nickname.empty() ? nickname : entry.username);
        entry.sender = normalized(entry.sender, 80);
        entry.content = entry.summary;
        if (messageDb) {
            gchar *digest = g_compute_checksum_for_string(G_CHECKSUM_MD5,
                                                          entry.username.c_str(), -1);
            std::string table = "Msg_" + std::string(digest);
            g_free(digest);
            std::string messageSql =
                "SELECT message_content, local_type FROM \"" + table +
                "\" ORDER BY local_id DESC LIMIT 1";
            sqlite3_stmt *messages = nullptr;
            if (sqlite3_prepare_v2(messageDb->get(), messageSql.c_str(), -1,
                                   &messages, nullptr) == SQLITE_OK) {
                if (sqlite3_step(messages) == SQLITE_ROW) {
                    std::string raw = columnText(messages, 0);
                    int msgType = sqlite3_column_int(messages, 1);
                    std::string content = plainContent(raw, msgType);
                    if (!content.empty() && content != "[消息]") entry.content = content;
                }
                sqlite3_finalize(messages);
            }
        }
        entry.content = normalized(entry.content);

        entries.push_back(std::move(entry));
    }
    sqlite3_finalize(contact);
    sqlite3_finalize(sessions);
    if (entries.empty()) return std::nullopt;

    std::string signature;
    for (const auto &entry : entries) {
        signature += entry.username + "|" + entry.content + "|" +
                     std::to_string(entry.unread) + "|" + entry.lastTimestamp + "|" +
                     entry.sortTimestamp + "\n";
    }

    if (entries.size() == 1) {
        const auto &entry = entries.front();
        std::string body;
        if (!entry.sender.empty() && entry.sender != entry.title)
            body = entry.sender + ": " + entry.content;
        else if (!entry.content.empty()) body = entry.content;
        else body = "有新消息";
        if (entry.unread > 1) body += " (" + std::to_string(entry.unread) + " 条未读)";
        return Notice{signature, normalized(entry.title, 80), normalized(body)};
    }

    int totalUnread = 0;
    for (const auto &entry : entries) totalUnread += entry.unread;
    std::string title = std::to_string(entries.size()) + " 个会话有新消息";
    std::string body;
    std::size_t shown = std::min<std::size_t>(entries.size(), 6);
    for (std::size_t i = 0; i < shown; ++i) {
        const auto &entry = entries[i];
        if (!body.empty()) body += "\n";
        body += entry.title;
        body += ": ";
        body += entry.content;
        if (entry.unread > 1) body += " (" + std::to_string(entry.unread) + " 条未读)";
    }
    if (entries.size() > shown)
        body += "\n…等共 " + std::to_string(entries.size()) + " 个会话，" +
                std::to_string(totalUnread) + " 条未读";
    return Notice{signature, title, truncateBody(body)};
}

static ExtractResult extractNotice(bool forceKeyScan = false) {
    try {
        auto pid = findWechatPid();
        auto live = findDatabases();
        if (!pid || !live) return {false, {}, "WeChat process or databases not found"};
        auto copied = snapshot(*live);
        if (!copied) return {false, {}, "could not obtain a stable database snapshot"};
        auto sessionSalt = databaseSalt(copied->second.session);
        auto contactSalt = databaseSalt(copied->second.contact);
        if (!sessionSalt || !contactSalt) return {false, {}, "database salt unavailable"};
        std::vector<std::string> salts{*sessionSalt, *contactSalt};
        std::optional<std::string> messageSalt;
        if (!copied->second.message.empty()) {
            messageSalt = databaseSalt(copied->second.message);
            if (messageSalt) salts.push_back(*messageSalt);
        }
        auto keys = forceKeyScan ? std::map<std::string, std::string>{} : loadKeyCache();
        if (keys.empty()) {
            keys = scanKeys(*pid, salts);
            if (keys.size() == salts.size()) saveKeyCache(keys);
        }
        if (!keys.contains(*sessionSalt) || !keys.contains(*contactSalt))
            return {false, {}, "database keys unavailable"};
        Database sessionDb(copied->second.session, keys[*sessionSalt]);
        Database contactDb(copied->second.contact, keys[*contactSalt]);
        std::optional<Database> messageDb;
        if (messageSalt && keys.contains(*messageSalt)) {
            try {
                messageDb.emplace(live->message, keys[*messageSalt]);
            } catch (const std::exception &) {
                messageDb.reset();
            }
        }
        return {true, queryNotice(sessionDb, contactDb,
                                  messageDb ? &*messageDb : nullptr), {}};
    } catch (const std::exception &error) {
        return {false, {}, error.what()};
    }
}

static bool relevantName(const std::string &name) {
    return name.starts_with("session.db") || name.starts_with("contact.db") ||
           name.starts_with("message_0.db") || name.ends_with(".material");
}

static int runShell(const std::string &command) {
    pid_t child = fork();
    if (child == 0) {
        execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char *>(nullptr));
        _exit(127);
    }
    if (child > 0) {
        int status = 0;
        if (waitpid(child, &status, 0) == child && WIFEXITED(status))
            return WEXITSTATUS(status);
    }
    return -1;
}

static std::optional<std::uint64_t> niriWechatWindowId() {
    FILE *pipe = popen("niri msg windows 2>/dev/null", "r");
    if (!pipe) return std::nullopt;
    char line[512];
    std::optional<std::uint64_t> current;
    while (fgets(line, sizeof(line), pipe)) {
        std::uint64_t id = 0;
        if (std::sscanf(line, "Window ID %lu:", &id) == 1) {
            current = id;
            continue;
        }
        if (current && std::strstr(line, "App ID:") && std::strstr(line, "wechat")) {
            pclose(pipe);
            return current;
        }
    }
    pclose(pipe);
    return std::nullopt;
}

static bool runAndCapture(const std::string &command, std::string &output) {
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe) return false;
    char buffer[512];
    output.clear();
    while (fgets(buffer, sizeof(buffer), pipe)) output += buffer;
    pclose(pipe);
    return true;
}

static bool wechatWindowFocused() {
    std::string output;
    if (runAndCapture("niri msg focused-window 2>/dev/null", output) &&
        output.find("App ID:") != std::string::npos) {
        std::istringstream lines(output);
        std::string line;
        while (std::getline(lines, line)) {
            if (line.find("App ID:") == std::string::npos) continue;
            return line.find("wechat") != std::string::npos;
        }
        return false;
    }
    if (runAndCapture("xdotool getactivewindow getwindowclassname 2>/dev/null", output))
        if (lower(output).find("wechat") != std::string::npos) return true;
    if (runAndCapture("xdotool getactivewindow getwindowname 2>/dev/null", output))
        if (lower(output).find("weixin") != std::string::npos) return true;
    std::string id;
    if (runAndCapture("kdotool getactivewindow 2>/dev/null", output)) {
        std::istringstream lines(output);
        lines >> id;
        if (!id.empty()) {
            if (runAndCapture("kdotool getwindowclassname " + id + " 2>/dev/null", output))
                if (lower(output).find("wechat") != std::string::npos) return true;
            if (runAndCapture("kdotool getwindowname " + id + " 2>/dev/null", output))
                if (lower(output).find("weixin") != std::string::npos) return true;
        }
    }
    return false;
}

static bool focusWechatWindow() {
    if (auto id = niriWechatWindowId()) {
        return runShell("niri msg action focus-window --id " + std::to_string(*id)) == 0;
    }

    // This launcher discovers WeChat's StatusNotifierItem and invokes its
    // Activate method: exactly the same operation as clicking the tray icon.
    if (findWechatPid())
        return runShell("/usr/lib/wechat-universal/start.sh >/dev/null 2>&1") == 0;

    // If WeChat is not running, start it outside this long-lived service's
    // cgroup.  systemd-run returns after queueing the transient user unit.
    return runShell("systemd-run --user --scope --quiet "
                    "/usr/lib/wechat-universal/start.sh >/dev/null 2>&1") == 0;
}

static void onNotificationActivated(NotifyNotification *notification, gchar *action,
                                    gpointer user_data) {
    (void)notification;
    (void)action;
    (void)user_data;
    std::cerr << "notification activated: opening WeChat\n";
    if (!focusWechatWindow())
        std::cerr << "activation failed: could not focus or restore WeChat\n";
}

static void onNotificationClosed(NotifyNotification *notification, gpointer user_data) {
    (void)user_data;
    // showNotification deliberately keeps its initial reference alive so
    // libnotify can deliver action callbacks after this function returns.
    g_object_unref(notification);
}

static bool showNotification(const Notice &notice) {
    gchar *title = g_markup_escape_text(notice.title.c_str(), -1);
    gchar *body = g_markup_escape_text(notice.body.c_str(), -1);
    NotifyNotification *notification =
        notify_notification_new(title, body, "wechat-universal");
    notify_notification_set_urgency(notification, NOTIFY_URGENCY_NORMAL);
    notify_notification_set_timeout(notification, NOTIFY_EXPIRES_DEFAULT);
    notify_notification_set_hint_string(notification, "desktop-entry", "wechat-universal");
    notify_notification_add_action(notification, "default", "打开微信",
                                   onNotificationActivated, nullptr, nullptr);
    g_signal_connect(notification, "closed", G_CALLBACK(onNotificationClosed), nullptr);
    GError *error = nullptr;
    bool ok = notify_notification_show(notification, &error);
    if (error) {
        std::cerr << "notification error: " << error->message << '\n';
        g_error_free(error);
    } else {
        std::cerr << "notification shown: " << notice.title << " | " << notice.body << '\n';
    }
    if (!ok)
        g_object_unref(notification);
    g_free(title);
    g_free(body);
    return ok;
}

struct WatchContext {
    int fd = -1;
    guint fdSourceId = 0;
    guint debounceId = 0;
    bool pending = false;
    fs::path signatureFile;
    GMainLoop *loop = nullptr;
};

static gboolean debounceFired(gpointer data) {
    auto *ctx = static_cast<WatchContext *>(data);
    ctx->pending = false;
    ctx->debounceId = 0;
    auto result = extractNotice();
    if (!result.ok) {
        std::cerr << "read deferred: " << result.error << '\n';
        return G_SOURCE_REMOVE;
    }
    if (result.notice) {
        std::string previous = readText(ctx->signatureFile);
        if (previous != result.notice->signature) {
            writePrivate(ctx->signatureFile, result.notice->signature);
            if (wechatWindowFocused()) {
                std::cerr << "notification skipped: WeChat window is focused\n";
            } else {
                showNotification(*result.notice);
            }
        }
    }
    return G_SOURCE_REMOVE;
}

static gboolean inotifyReady(int fd, GIOCondition condition, gpointer data) {
    (void)condition;
    auto *ctx = static_cast<WatchContext *>(data);
    std::array<char, 64 * 1024> events{};
    ssize_t size;
    while ((size = read(fd, events.data(), events.size())) > 0) {
        for (std::size_t offset = 0; offset < static_cast<std::size_t>(size);) {
            auto *event = reinterpret_cast<inotify_event *>(events.data() + offset);
            if (event->mask & (IN_DELETE_SELF | IN_MOVE_SELF | IN_IGNORED)) {
                if (ctx->debounceId) g_source_remove(ctx->debounceId);
                g_main_loop_quit(ctx->loop);
                return G_SOURCE_REMOVE;
            }
            if (event->len && relevantName(event->name) && !ctx->pending) {
                ctx->pending = true;
                ctx->debounceId = g_timeout_add(700, debounceFired, ctx);
            }
            offset += sizeof(inotify_event) + event->len;
        }
    }
    return G_SOURCE_CONTINUE;
}

static int watch(const Databases &databases) {
    int fd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (fd < 0) return 1;
    std::uint32_t mask = IN_MODIFY | IN_CLOSE_WRITE | IN_CREATE | IN_MOVED_TO |
                         IN_DELETE_SELF | IN_MOVE_SELF;
    int sessionWatch = inotify_add_watch(fd, databases.session.parent_path().c_str(), mask);
    int contactWatch = inotify_add_watch(fd, databases.contact.parent_path().c_str(), mask);
    int messageWatch = -1;
    if (!databases.message.empty())
        messageWatch = inotify_add_watch(fd, databases.message.parent_path().c_str(), mask);
    if (sessionWatch < 0 || contactWatch < 0) { close(fd); return 1; }

    WatchContext ctx;
    ctx.fd = fd;
    ctx.signatureFile = stateDir() / "last-signature";
    ctx.loop = g_main_loop_new(nullptr, FALSE);

    auto baseline = extractNotice();
    if (baseline.ok && baseline.notice)
        writePrivate(ctx.signatureFile, baseline.notice->signature);

    ctx.fdSourceId = g_unix_fd_add(fd, G_IO_IN, inotifyReady, &ctx);
    g_main_loop_run(ctx.loop);
    g_source_remove(ctx.fdSourceId);
    g_main_loop_unref(ctx.loop);
    (void)messageWatch;
    close(fd);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && std::string(argv[1]) == "--version") {
        std::cout << "wechat-universal-notify " << kVersion << '\n';
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--check") {
        auto result = extractNotice();
        if (!result.ok) {
            std::cerr << "check failed: " << result.error << '\n';
            return 1;
        }
        std::cout << "session_snapshot_query=ok\ncontact_snapshot_query=ok\n";
        if (result.notice) {
            std::cout << "unread_chat_count="
                      << (std::count(result.notice->signature.begin(),
                                     result.notice->signature.end(), '\n'))
                      << '\n';
            std::cout << "title=" << result.notice->title << '\n';
            std::cout << "body=" << result.notice->body << '\n';
        }
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--refresh-key") {
        auto result = extractNotice(true);
        if (!result.ok) {
            std::cerr << "key refresh failed: " << result.error << '\n';
            return 1;
        }
        std::cout << "database_key_cache=updated\n";
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--activate") {
        return focusWechatWindow() ? 0 : 1;
    }
    if (argc != 1) {
        std::cerr << "Usage: " << argv[0]
                  << " [--activate|--check|--refresh-key|--version]\n";
        return 2;
    }
    if (!notify_init("WeChat")) {
        std::cerr << "failed to initialize desktop notifications\n";
        return 1;
    }
    for (;;) {
        auto databases = findDatabases();
        if (databases) watch(*databases);
        std::this_thread::sleep_for(5s);
    }
}
