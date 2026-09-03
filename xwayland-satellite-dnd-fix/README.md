# xwayland-satellite-dnd-fix

This VCS package replaces Arch's `xwayland-satellite` and provides the
satellite half of bidirectional X11/Wayland drag-and-drop. Install it together
with `niri-xwayland-dnd-fix` for X11 -> Wayland support.

## What it does

- Wayland -> X11: bridges all advertised MIME types, negotiates Copy or Move,
  preserves XDND Position/Status ordering, supports XDND v2-v5, and transfers
  large payloads with ICCCM INCR.
- X11 -> Wayland: watches `XdndSelection` as soon as an X drag starts, exposes
  its targets through a real Wayland drag source, proxies the X root window to
  the internal XDND target, and returns Status/Finished with Copy or Move.
- X11 -> X11: native X windows still take precedence; the root proxy is used
  only when the pointer is over a native Wayland surface.
- Clipboard: the normal upstream clipboard and primary-selection ownership
  paths are retained. DND sources have separate event and lifetime state.

The code is based on the XDND protocol and uses KWin only as a behavioral
reference; no KWin source is copied.

## Build and install

```sh
cd xwayland-satellite-dnd-fix
makepkg -si
```

Or from the repository root:

```sh
paru -Bi xwayland-satellite-dnd-fix
# or
yay -Bi xwayland-satellite-dnd-fix
```

Log out and back in after installing both replacement packages. Restarting
WeChat alone is insufficient because niri must launch satellite through its
dedicated Wayland connection.

## Updating safely

The source follows upstream `main`. Every rebuild reapplies the patch with
normal context checks; an incompatible upstream change makes `prepare()` fail
instead of silently omitting the fix. After a successful rebuild, run the full
test suite and repeat real-session tests in both directions before publishing.

## Rollback

```sh
sudo pacman -S niri xwayland-satellite
```

Then log out and back in.

## Required real-session checks

Test Wayland -> X11, X11 -> Wayland, and X11 -> X11 with Copy and Move; cancel
and rejected drops; a large file (INCR); and WeChat text/image clipboard in both
directions. Build tests validate protocol state and regressions but cannot prove
the behavior of a running niri/WeChat session.
