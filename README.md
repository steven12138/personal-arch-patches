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
| `wechat-universal-notify` | A user service that watches WeChat Universal's encrypted message database and sends native Linux notifications. Its default action focuses an existing WeChat window, then tries tray activation, then starts WeChat if needed. | You run `wechat-universal-bwrap` and want native notifications plus reliable notification-click activation. |
| `xwayland-satellite-dnd-fix` | A replacement for the distribution `xwayland-satellite`. It follows upstream `main` on rebuild and applies a copy-only Wayland-to-X11 XDND and stale-clipboard patch, primarily for native Files/Nautilus to XWayland WeChat drag-and-drop. It conflicts with `xwayland-satellite`; log out and back in after changing it. | You need that specific XWayland drag-and-drop/clipboard fix and accept replacing the distro package. |

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
