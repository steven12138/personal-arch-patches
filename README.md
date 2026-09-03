# Personal Arch patches

Reproducible local Arch packages maintained by Steven. Each directory is an
independent PKGBUILD; build artifacts are intentionally not committed.

## Install

Install `base-devel`, `git`, and an AUR helper first. Clone this repository,
then build and install only the packages you want:

```sh
git clone https://github.com/steven12138/personal-arch-patches.git
cd personal-arch-patches
paru -Bi dms-legion-control
# or
yay -Bi dms-legion-control
```

Both `paru -Bi <directory>` and `yay -Bi <directory>` resolve dependencies,
build the directory's PKGBUILD, and install the resulting package. Without an
AUR helper, use:

```sh
cd dms-legion-control
makepkg -si
```

To install every package, review the list first, then run either command:

```sh
for package in */PKGBUILD; do
  paru -Bi "${package%/PKGBUILD}"
done

for package in */PKGBUILD; do
  yay -Bi "${package%/PKGBUILD}"
done
```

## Packages

| Package | What it installs or changes | Use it when |
| --- | --- | --- |
| `dms-legion-control` | A DankMaterialShell bar widget that shows Legion CPU/GPU/IC temperatures, fan RPM, battery power, and firmware profile. Its popout can change only a small Polkit-protected set: power profile, battery conservation, rapid charging, lights, keyboard backlight, and iGPU mode. It deliberately does not edit fan curves or CPU/GPU limits. | You have a supported Lenovo Legion and want those controls inside DMS. |
| `dms-material-transition` | A short Material 3 overlay from the greeter into niri/DMS. It adds a niri `ExecStartPre` helper to prepare the first output mode and a narrowly scoped greeter-exit patch. | You use niri + DMS and want the custom login transition. |
| `dms-shell-local-patches` | A package-owned patch manager plus pacman hook. It reapplies selected DMS QML fixes after DMS upgrades: segmented-button corners, fullscreen bar animation, lock-screen selection, niri session environment import, Qt theme propagation, and qt5ct/qt6ct config fixes. It also modifies `/usr/bin/niri-session` through those managed patches. | You need these DMS/niri fixes to persist across upgrades. |
| `dms-spotlight-translate` | A DMS Spotlight plugin backed by `translate-shell`: `> hello` translates to Chinese, `> 你好` to English, and Enter copies the result. | You want quick Chinese/English translation from the DMS launcher. |
| `dms-weather-sun-times` | A DMS patch and reapply hook that adds a toggleable sunrise/sunset timeline to the Weather sky chart. | You use DMS Weather and want daylight times in its graph. |
| `niri-dms-extreme-power` | The machine-specific TLP/DMS policy: low-power and performance CPU/GPU limits, display refresh/brightness changes, D-Bus profile watcher, Bluetooth/NVIDIA power handling, and guarded Legion zero-RPM fan locking in Power Saver. | You have the matching Legion/niri setup and want DMS Power Profiles to control this whole policy. |
| `niri-xwayland-dnd-fix` | A replacement for Arch's `niri`, paired with the exact Smithay revision used by niri 26.04. It identifies the on-demand satellite through a dedicated Wayland socket and lets an X11 drag retain pointer motion while native Wayland targets receive the data offer. | You need X11 -> Wayland drag-and-drop; install it together with `xwayland-satellite-dnd-fix` and log out/in. |
| `wechat-universal-notify` | A user service that watches WeChat Universal's encrypted message database and sends native Linux notifications. Its default action focuses an existing WeChat window, then tries tray activation, then starts WeChat if needed. | You run `wechat-universal-bwrap` and want native notifications plus reliable notification-click activation. |
| `xwayland-satellite-dnd-fix` | A replacement for the distribution `xwayland-satellite`. It tracks upstream `main` and adds bidirectional Wayland/X11 XDND, Copy/Move negotiation, multiple MIME types, large INCR transfers, proxy routing over native surfaces, and isolated DND state so the upstream clipboard path stays intact. | You need drag-and-drop between native Wayland apps and XWayland apps such as WeChat; pair it with `niri-xwayland-dnd-fix` for the X11 -> Wayland direction. |

## Updating

From a fresh clone, pull changes and rebuild the package you use:

```sh
git pull --ff-only
paru -Bi niri-dms-extreme-power
# or
yay -Bi niri-dms-extreme-power
```

Before trusting a package, inspect its `PKGBUILD`, `.SRCINFO`, and patch files.
These packages are machine-specific local patches, not official Arch packages.

For the paired XDND packages, check the newest niri tag, its exact Smithay
revision, and satellite `main` without changing the package files:

```sh
./scripts/check-xdnd-upstream.sh
```

A failed dry-run means upstream changed the touched code and the patches need a
review. A successful dry-run is only the first gate; rebuild both packages and
run their tests before bumping the pinned niri/Smithay pair.
