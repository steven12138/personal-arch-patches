# DMS Spotlight Translate

A DMS Spotlight launcher plugin for automatic Chinese-English translation.

## Usage

- `> hello world` translates to Simplified Chinese.
- `> 你好世界` translates to English.
- `> en 你好世界` explicitly selects English.
- `> zh hello world` explicitly selects Simplified Chinese.
- `> ja: good morning` selects another target language supported by translate-shell.
- Press Enter to copy the translation.

The default `>` trigger can be changed in DMS Settings → Plugins → Spotlight Translate.

## Origin

This plugin is derived from DankTranslate 0.1.3 by alcxyz. It uses a distinct
plugin ID so it can be packaged system-wide without overwriting the upstream
plugin. The translation process, asynchronous refresh, timeout handling, and
launcher result model follow the upstream design. Chinese-English direction
detection, safer explicit-language parsing, DMS-native clipboard handling, and
the Arch package integration are maintained here.

## Install

```bash
yay -Bi /path/to/dms-spotlight-translate
```

Enable `Spotlight Translate` in DMS Settings → Plugins, then restart DMS.
