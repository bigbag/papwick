# Papwick

Papwick is an app platform firmware for the [M5Stack TriviaPOD](https://docs.m5stack.com/en/core/TriviaPOD). It has a home screen, device settings, Wi-Fi time sync and two apps: Workout and 2048. M5Unified 0.3.0 runs the TriviaPOD as the M5Stack StopWatch (board 30).

> The firmware replaces the stock Etermax trivia firmware. Make a backup before the first write. See [flashing](docs/flashing.md).

## Quick start

```bash
uv sync         # .venv with Python 3.13 and PlatformIO 6.2.0
make backup     # once, before the first write; needs esptool 5 or later on PATH
make upload     # build and flash the firmware
make monitor    # read the serial log
make test       # run the host unit tests
```

## Firmware

| ENV | Source | Function |
| --- | --- | --- |
| `fitness` (default) | `src/` | The platform and its apps |
| `probe` | `examples/probe/` | Hardware self-test: display, touch, buttons, speaker, microphone, vibration, IMU, RTC and battery |

## Documents

- [Platform](docs/platform.md): controls, screens, Wi-Fi, time, software structure and storage.
- [Apps](docs/apps.md): Workout and 2048.
- [Specifications](docs/triviapod-specifications.md): hardware, pins and power button.
- [Flashing](docs/flashing.md): backup, restore and upload.

## Layout

| Path | Contents |
| --- | --- |
| `src/shell/` | Platform: app interface, run loop, launcher, settings, Wi-Fi setup and time sync |
| `src/apps/` | Apps: `fitness/` (Workout) and `game2048/` |
| `lib/BoardSupport/` | Hardware library: input, light and power, tones, vibration, microphone, clock, NTP and Wi-Fi |
| `lib/Ui/` | Drawing helpers, wheel pickers, settings rows and confirm screens |
| `lib/FitnessCore/` | Workout rules without hardware code; host-tested |
| `lib/Game2048Core/` | 2048 rules; host-tested |
| `lib/SystemCore/` | Device settings, time zones and saved Wi-Fi networks; host-tested |
| `lib/Logging/` | `LOG_ERR/WRN/INF/DBG` serial log macros |
| `examples/` | Hardware examples; one PlatformIO env each |
| `test/` | Host unit tests |
| `scripts/` | `backup_flash.sh`, `preview_figures.py` |

Run `make help` for all commands.
