# DMS Legion Control

An independent DankMaterialShell widget for the `legion_laptop` kernel module.
It shows CPU/GPU/IC temperatures, fan RPM and targets, battery charge state,
net battery charging power, and the current platform profile.

The popout can change only a deliberately small, whitelisted set of firmware
controls: platform profile, battery conservation, rapid charge, Legion lights,
keyboard backlight level, and iGPU mode. Each change uses `pkexec` and needs
Polkit authentication. Fan curves, thermal limits, and CPU/GPU power limits are
read-only by design: they are too easy to destabilize from a shell widget.

After installation, open **DMS Settings → Plugins → Scan for Plugins**, enable
**Legion Control**, then add it under **Appearance → DankBar Layout**.

The widget reads `/sys/devices/platform/legion` and `/sys/class/power_supply`.
It does not expose USB-PD PDO/contract data because this machine's firmware does
not export a Type-C partner through `/sys/class/typec`.
