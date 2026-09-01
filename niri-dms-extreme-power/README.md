# niri-dms-extreme-power

Local Arch/AUR-style package for the machine-specific power policy used by the
Lenovo Legion Y9000P IAH7H niri/DankMaterialShell session.

## Managed behavior

- TLP performance profile: CPU fixed at 4.9 GHz, performance EPP, boost on,
  firmware performance mode, and Intel GPU up to 1450 MHz.
- TLP power-saver profile: CPU fixed at 400 MHz, boost off, Intel GPU limits at
  100 MHz, low-power firmware mode, PCIe powersupersave, two NVMe drives, radio,
  audio, USB, and deep-sleep tuning.
- `tlp-pd` exposes those profiles to DMS through the standard PowerProfiles
  D-Bus API.
- In niri, performance selects 165.004 Hz. Balanced and power-saver select
  60.002 Hz; power-saver also selects 20% Intel-panel brightness and remembers
  the previous value for restoration when leaving power-saver for either
  balanced or performance. Starting the watcher while already in power-saver
  also preserves the current brightness before lowering it.
- Niri ignores the NVIDIA DRM render device in every power profile and uses the
  Intel GPU by default. Power-saver stops `nvidia-powerd`, unloads idle NVIDIA
  modules, and removes the NVIDIA graphics and HDMI-audio functions from the
  PCI bus so the GPU is fully powered off. If an offloaded application is using
  the modules, removal is skipped safely. Performance and balanced rescan the
  parent PCI bridge, reload the DRM driver, and restore `nvidia-powerd`. The
  niri rule is written to
  `~/.config/niri/dms/extreme-power-gpu.kdl` and included from the user's config
  with one package-managed optional include line.
- A `niri.service` `ExecStartPre` helper writes the GPU rule before the
  compositor opens DRM devices. This is what allows the NVIDIA GPU to enter
  runtime suspend after logging into niri while SAV is active.
- A root systemd watcher listens to the PowerProfiles D-Bus signal directly, so
  manual DMS profile changes apply device state without a polkit prompt.
- In power-saver, the optional Legion experiment locks the EC fan controller
  only when both `legion_hwmon` fans already report `0 RPM`. It never commands
  a fan down to zero, ignores systems without the supported sysfs nodes, and
  unlocks the controller immediately on balanced or performance. Set
  `LOCK_FANS_ON_POWER_SAVER=0` to disable it.

## Files and persistence

The package owns its TLP drop-in, scripts, systemd unit, udev rule, and an XDG
autostart entry whose command exits immediately unless
`XDG_CURRENT_DESKTOP=niri`. Official TLP, DMS, and niri upgrades therefore do
not overwrite the integration or activate it in KDE.

`/etc/niri-dms-extreme-power.conf` controls panel modes, brightness, Bluetooth,
NVIDIA handling, and the guarded Legion fan-lock experiment. A user may
override only the display/brightness values in `~/.config/niri-dms-extreme-power.conf`.

DMS's `~/.config/DankMaterialShell/settings.json` remains user state because DMS
writes that file itself. This package intentionally does not own or overwrite
the complete DMS settings file.

Niri live-reloads the generated GPU rule. An already-running compositor keeps
its startup renderer until the next login, so log out and back in once after
installing this package version. To let niri use NVIDIA deliberately, set
`NIRI_IGNORE_NVIDIA_GPU=0` in `~/.config/niri-dms-extreme-power.conf` and log in
again.

Set `REMOVE_NVIDIA_ON_POWER_SAVER=0` in that user override to retain the older
runtime-suspend-only behavior.

## Commands

```sh
makepkg -si
sudo systemctl enable --now niri-dms-extreme-power.service
sudo tlp start
```

Validate with:

```sh
tlpctl get
sudo tlp-stat -c
systemctl status niri-dms-extreme-power.service
niri msg outputs
brightnessctl --device=intel_backlight info
```

Removing the package removes the managed files and disables its system unit.
TLP then falls back to its own defaults unless another local configuration is
present. Existing DMS settings are never removed.
