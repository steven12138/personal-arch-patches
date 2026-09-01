# xwayland-satellite-dnd-fix

This is a VCS package: every normal rebuild retrieves the current upstream
`main` branch, derives a version with `pkgver()`, and then applies the local
XDND/clipboard patch. It intentionally fails during `prepare()` if upstream
changes make the patch incompatible; that is safer than silently installing an
unreviewed merge.

Update an installed copy with:

```sh
git pull --ff-only
paru -Bi xwayland-satellite-dnd-fix
# or: yay -Bi xwayland-satellite-dnd-fix
```

Local Arch package for Steven's niri/Xwayland setup. It follows upstream
`main`; the local patch is deliberately reapplied on every rebuild, so upstream
changes cannot silently bypass or alter the DND fix.

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
sudo pacman -U ./*.pkg.tar.zst
```

Installing conflicts with and replaces the installed package only for this
transaction; the package declares `provides=('xwayland-satellite')` and does
not use `replaces`.

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
