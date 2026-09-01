import QtQuick
import Quickshell
import Quickshell.Io
import qs.Services

QtObject {
    id: root

    property var pluginService: null
    property string pluginId: "spotlightTranslate"
    property string trigger: ">"
    property string _lastQuery: ""
    property string _lastResult: ""
    property string _lastError: ""
    property string _stdoutBuffer: ""
    property string _stderrBuffer: ""
    property string _pendingQuery: ""
    property string _pendingLang: ""
    property bool _translating: false
    property bool _timedOut: false

    signal itemsChanged

    Component.onCompleted: {
        if (pluginService)
            trigger = pluginService.loadPluginData(pluginId, "trigger", ">");
    }

    property Timer debounceTimer: Timer {
        interval: 300
        repeat: false
        onTriggered: {
            if (root._pendingQuery)
                root.startTranslation(root._pendingQuery, root._pendingLang);
        }
    }

    property Timer translationTimeout: Timer {
        interval: 15000
        repeat: false
        onTriggered: {
            root._timedOut = true;
            if (transProcess.running)
                transProcess.running = false;
            root.finishTranslation("", "Translation timed out");
        }
    }

    function containsChinese(text) {
        return /[\u3400-\u4dbf\u4e00-\u9fff\uf900-\ufaff]/.test(text);
    }

    function parseQuery(raw) {
        var trimmed = (raw || "").trim();
        if (!trimmed)
            return { lang: "", text: "", direction: "" };

        var match = trimmed.match(/^(en|zh(?:-CN)?)\s+(.+)$/i);
        if (match) {
            var explicitLang = match[1].toLowerCase();
            if (explicitLang === "zh")
                explicitLang = "zh-CN";
            return {
                lang: explicitLang,
                text: match[2].trim(),
                direction: "explicit → " + explicitLang
            };
        }

        match = trimmed.match(/^([a-z]{2,3}(?:-[a-z]{2})?):\s*(.+)$/i);
        if (match) {
            return {
                lang: match[1],
                text: match[2].trim(),
                direction: "explicit → " + match[1]
            };
        }

        var chineseInput = containsChinese(trimmed);
        return {
            lang: chineseInput ? "en" : "zh-CN",
            text: trimmed,
            direction: chineseInput ? "中文 → English" : "English → 简体中文"
        };
    }

    function startTranslation(text, lang) {
        if (transProcess.running)
            transProcess.running = false;
        _stdoutBuffer = "";
        _stderrBuffer = "";
        _timedOut = false;
        _translating = true;
        _lastError = "";
        transProcess.command = ["sh", "-c", "command -v trans >/dev/null 2>&1 || exit 127; exec trans -brief -t \"$1\" \"$2\"", "sh", lang, text];
        transProcess.running = true;
        translationTimeout.restart();
    }

    function finishTranslation(result, errorText) {
        translationTimeout.stop();
        _translating = false;
        _lastResult = result ? result.trim() : "";
        _lastError = errorText || "";
        if (pluginService)
            pluginService.requestLauncherUpdate(pluginId);
    }

    property Process transProcess: Process {
        running: false

        stdout: StdioCollector {
            onStreamFinished: root._stdoutBuffer = text.trim()
        }

        stderr: StdioCollector {
            onStreamFinished: root._stderrBuffer = text.trim()
        }

        onExited: exitCode => {
            if (root._timedOut)
                return;
            if (exitCode === 0 && root._stdoutBuffer)
                root.finishTranslation(root._stdoutBuffer, "");
            else if (exitCode === 127)
                root.finishTranslation("", "translate-shell (trans) is not installed");
            else
                root.finishTranslation("", root._stderrBuffer || "Translation failed");
        }
    }

    function getItems(query) {
        if (!query || !query.trim()) {
            return [{
                name: "输入中文或英文",
                icon: "material:translate",
                comment: "自动中英互译；也可输入 en 中文、zh English 或 ja: text",
                action: "none:",
                categories: ["Spotlight Translate"],
                _preScored: 1000
            }];
        }

        var parsed = parseQuery(query);
        var queryKey = parsed.lang + ":" + parsed.text;
        if (queryKey !== _lastQuery) {
            _lastQuery = queryKey;
            _lastResult = "";
            _lastError = "";
            _pendingQuery = parsed.text;
            _pendingLang = parsed.lang;
            debounceTimer.restart();
        }

        if (_lastError) {
            return [{
                name: "翻译失败",
                icon: "material:error_outline",
                comment: _lastError,
                action: "none:",
                categories: ["Spotlight Translate"],
                _preScored: 1000
            }];
        }

        if (_translating || !_lastResult) {
            return [{
                name: "正在翻译…",
                icon: "material:hourglass_empty",
                comment: parsed.direction,
                action: "none:",
                categories: ["Spotlight Translate"],
                _preScored: 1000
            }];
        }

        return _lastResult.split("\n").filter(function(line) {
            return line.trim().length > 0;
        }).map(function(line) {
            return {
                name: line,
                icon: "material:translate",
                comment: parsed.direction + " · 回车复制",
                action: "copy:" + line,
                categories: ["Spotlight Translate"],
                _preScored: 1000
            };
        });
    }

    function executeItem(item) {
        if (!item || !item.action || !item.action.startsWith("copy:"))
            return;
        var text = item.action.substring(5);
        if (!text)
            return;
        Quickshell.execDetached(["dms", "cl", "copy", text]);
        if (typeof ToastService !== "undefined")
            ToastService.showInfo("Spotlight Translate", "译文已复制");
    }

    function getPasteText(item) {
        if (!item || !item.action || !item.action.startsWith("copy:"))
            return null;
        return item.action.substring(5);
    }

    onTriggerChanged: {
        if (pluginService)
            pluginService.savePluginData(pluginId, "trigger", trigger);
    }
}
