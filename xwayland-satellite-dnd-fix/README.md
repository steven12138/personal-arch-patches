# xwayland-satellite-dnd-fix

Local Arch package for Steven's niri/Xwayland setup. It is pinned to upstream
commit `6d0de1cedde9dc02abb8877d1b04b90a8c22c3d0` (five commits after v0.8.2), so
upstream changes cannot silently alter a rebuild.

## Fixes included

- Bridges a Wayland `text/uri-list` drag into an X11 XDND v5 transaction.
- Advertises and accepts only the Copy action. It does not move or delete files.
- Converts surface-local pointer coordinates to X11 root coordinates.
- Rejects non-file offers and targets that do not advertise `XdndAware`.
- Serializes Position/Status round trips and keeps only the newest queued motion.
- Defers Drop until the final Position is acknowledged by the X11 target.
- Retains a dropped Wayland offer through compositor leave until `XdndFinished`.
- Handles XDND v2-v5 completion semantics and target destruction safely.
- Lets a new Wayland clipboard/primary offer replace a stale X11-owned source.
- Clears the correct selection when an X11 owner closes or is destroyed.

This directly targets Files/Nautilus (native Wayland) -> WeChat (XWayland) and
the stale `cpsend`/`xclip` clipboard owner masking a newer Wayland image. It does
not broaden WeChat's bubblewrap mounts or change the `cpsend` alias.

The XDND implementation is intentionally limited to Wayland -> X11 file drops.
X11 -> Wayland drag source emulation requires a separate Wayland drag-source
lifecycle and is not claimed by this package.

## Build and install

```sh
cd /home/steven/aur/xwayland-satellite-dnd-fix
makepkg --cleanbuild --clean --force
sudo pacman -U ./xwayland-satellite-dnd-fix-0.8.2.r5.g6d0de1c-2-x86_64.pkg.tar.zst
```

Installing conflicts with and replaces the installed package only for this
transaction; the package declares `provides=('xwayland-satellite=0.8.2')` and
does not use `replaces`.

Log out and back in after installation so niri starts the new satellite. Then:

1. Copy an image in a native Wayland app and paste it into WeChat.
2. Drag a small file from Files into a WeChat conversation and verify it sends
   a copy while the original remains in place.
3. Run `journalctl --user -b -u xwayland-satellite.service` if either operation
   fails.

## Rollback

```sh
sudo pacman -S xwayland-satellite
```

Log out and back in again after rollback. The package owns only the satellite
binary, its user service, and its MPL license copy.

## Verification

The package check phase runs the upstream suite plus two new end-to-end tests:

- A complete Wayland file offer -> XDND v5 exchange, including queued motion,
  drop, post-drop leave, selection transfer, and `XdndFinished`.
- An X11 `text/uri-list` owner being destroyed, followed by a Wayland
  `image/png` offer taking ownership and transferring the new image bytes.

## Licensing

Unchanged upstream code is MPL-2.0. The new XDND bridge is GPL-2.0-or-later and
uses KWin's Xwayland drag state ordering as a behavioral reference. The package
therefore declares both licenses.
