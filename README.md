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

- `dms-legion-control` — DMS Legion sensor and platform-control widget.
- `dms-material-transition` — niri/DMS login transition integration.
- `dms-shell-local-patches` — package-managed DMS patches and pacman hook.
- `dms-spotlight-translate` — DMS Spotlight translation provider.
- `dms-weather-sun-times` — DMS weather/sun-times patch.
- `niri-dms-extreme-power` — niri/DMS TLP power-profile integration.
- `niri-session-env-fix` — niri session-environment launcher fix.
- `wechat-universal-notify` — native notifications and activation for WeChat
  Universal.
- `xwayland-satellite-dnd-fix` — XWayland Satellite XDND fix. It provides and
  conflicts with the distribution `xwayland-satellite` package; install it only
  if you want that replacement. Its PKGBUILD clones the pinned upstream commit
  and applies the included patch during `prepare()`.

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
