# dms-shell-local-patches

Local Arch package that keeps machine-specific DankMaterialShell QML fixes
across official `dms-shell` installs and upgrades.

The package does not replace or copy the full DMS package. It installs local
functional patches and compatibility migrations, an idempotent patch manager,
and a pacman PostTransaction hook. The hook runs after every `dms-shell`
install or upgrade.

## Managed patches

1. `0001-segmented-button-corners.patch`
   - Keeps large radii only on the visual outer edges of segmented controls.
   - Prevents a selected internal button from creating disconnected large
     corners in the notification center and other button groups.

2. `0002-niri-fullscreen-bar-animation.patch`
   - Uses DMS's compositor fullscreen signal to slide the bar away.
   - Gives bar slide transitions a minimum 180 ms OutCubic duration.
   - Requires the relevant bar's `useOverlayLayer` setting to remain enabled.

3. `0003-lock-screen-select-all.patch`
   - Makes `Ctrl+A` select the full custom lock-screen password buffer.
   - Backspace or Delete clears the selection, while typing replaces it.
   - Shows a selection highlight without replacing the security-oriented
     custom password input with a copyable standard text field.
   - Aligns the highlight with left-aligned short passwords and the visible
     portion of right-aligned overflowing passwords.

4. `0004-lock-screen-selection-alignment.patch`
   - Migrates systems that already applied the original `0003` highlight.
   - It is skipped when the corrected `0003` was applied to a pristine DMS
     installation.

5. `0005-niri-session-import-environment.patch`
   - Imports an explicit safe list of niri session variables into the systemd
     user manager without systemd's deprecated import-all warning.

6. `0006-niri-session-qt-environment.patch`
   - Propagates DMS's Qt platform and theme variables to applications launched
     through systemd or Files, and removes them when the niri session exits.

7. `0007-qt-colors-xdg-data-path.patch`
   - Makes DMS find `DankMatugen.colors` in `XDG_DATA_HOME` instead of the
     invalid `~/.config/.local/share` path.

8. `0008-qt-colors-config-newlines.patch`
   - Writes real line breaks when DMS creates a new `qt5ct.conf` or
     `qt6ct.conf`, instead of invalid literal `\\n` text.

9. `0009-niri-session-qt-environment-defaults.patch`
   - Defines the DMS-supported GTK3 Qt platform theme before niri, DMS,
     systemd, and D-Bus application activation diverge into separate paths.

## Commands

Check without modifying files:

```sh
sudo /usr/lib/dms-shell-local-patches/apply --check
```

Apply manually:

```sh
sudo /usr/lib/dms-shell-local-patches/apply --apply
dms restart
```

The automatic pacman hook does not restart a user's graphical shell. Restart
DMS or log in again after a `dms-shell` upgrade to load newly patched QML.

If an upstream release changes the affected code, the hook performs a dry run,
prints an incompatibility error, and leaves that file unchanged for manual
review. Removing this package does not reverse already-applied changes;
reinstall `dms-shell` to restore pristine upstream files.
