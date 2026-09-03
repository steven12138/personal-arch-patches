# niri-xwayland-dnd-fix

This package replaces Arch's `niri` package and supplies the compositor half
of bidirectional drag-and-drop for an external `xwayland-satellite` process.
Install it together with `xwayland-satellite-dnd-fix`.

The patch gives each on-demand satellite process a dedicated Wayland socket,
marks only that client as external Xwayland, and keeps pointer events flowing
to the X11 drag source while normal Wayland data-device focus follows the real
drop target. Other Wayland clients retain niri's normal DND behavior.

## Updating safely

niri and Smithay are deliberately pinned as a compatible pair. To update:

1. Change `_niri_commit`, `pkgver`, and `_smithay_commit` to the Smithay revision
   referenced by that niri commit.
2. Run `makepkg -Ccf` in this directory. Patch or Cargo failures mean upstream
   changed the touched API and the package must not be published yet.
3. Run the package tests, install both replacement packages, then log out and
   test X11 -> Wayland, Wayland -> X11, X11 -> X11, clipboard in both directions,
   Copy, Move, rejection/cancel, and a file large enough to exercise INCR.

This policy tracks tested upstream pairs rather than silently applying a patch
to an incompatible Smithay revision.
