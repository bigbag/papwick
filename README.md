# Papwick

Papwick is an app platform firmware for the [M5Stack TriviaPOD](https://docs.m5stack.com/en/core/TriviaPOD). It has a home screen, device settings, Wi-Fi time sync and seven apps: Workout, Pomodoro, Clock, 2048, Four in a Row, Lights Out and Sokoban.

> **Warning:** Papwick replaces the stock Etermax trivia firmware. The stock firmware has no public download. Make a [backup](docs/flashing.md#back-up-the-stock-firmware) before the first write.

This project is **not affiliated with M5Stack or Etermax**. It is a personal project.

## Documentation

- **Use:** [Platform](docs/platform.md), [Apps](docs/apps.md)
- **Install:** [Flashing](docs/flashing.md), [Troubleshooting](docs/troubleshooting.md)
- **Hardware:** [Specifications](docs/triviapod-specifications.md)

## Supported devices

- **[M5Stack TriviaPOD](docs/triviapod-specifications.md)** (TP-01-A)
  - SoC: ESP32-S3R8, 32 MB flash, 8 MB PSRAM
  - Display: 466 x 466 round AMOLED with touch
  - M5Unified 0.3.0 runs the device as the M5Stack StopWatch (board 30)

## Features

### Platform

- Home screen with one app on each page, the clock and a battery icon
- Touch for the content, physical buttons for the flow: blue is back, green is confirm
- Red button: a click goes home, a double click asks to power off
- Wheel pickers for numbers, dates and times
- Settings for brightness, sleep, sound, vibration and units
- App manager with the app settings and Clear data for each app
- Power saving: the CPU stops between frames, the speaker is on only for tones, and the CPU runs at 80 MHz while the screen is off
- All user interface texts come from one list, ready for translations

### Network and time

- Wi-Fi setup through a hotspot with a QR code, or through a form page in the home network
- Up to 4 saved networks
- NTP time sync with UTC offset time zones, from UTC-12 to UTC+14

### Apps

- **Workout:** a 28-day home workout plan with no equipment, animated exercises, a weight log and statistics
- **Pomodoro:** a focus timer with 25-minute sessions and breaks; the alarm also comes in other apps
- **Clock:** an analog or digital watch face with the date and the battery level
- **2048:** the tile game with swipe control and two color themes
- **Four in a Row:** play against the device on three levels
- **Lights Out:** turn off all lights on a 5 x 5 grid
- **Sokoban:** push the boxes onto the targets in 12 levels

## Installing

Make a backup of the stock firmware first. Then build and flash Papwick:

```bash
make backup
make upload
```

To go back to the stock firmware, run `make restore`. See [flashing](docs/flashing.md) for the details and the known problems.

## Development

### Prerequisites

- [uv](https://docs.astral.sh/uv/) and Python 3.13 for PlatformIO 6.2.0
- esptool 5 or later on `PATH`, for `make backup` and `make restore`
- CMake 3.16 or later and a C++17 compiler, for the host tests
- clang-format, for `make format`
- USB-C data cable

### Checking out the code

```bash
git clone https://github.com/bigbag/papwick
cd papwick
uv sync
```

`uv sync` makes a `.venv` with PlatformIO. The `Makefile` puts `.venv/bin` first in `PATH`.

### Building

Run the host tests before you build the firmware:

```bash
# Build and run the host tests
make test

# Build the firmware
make build

# Build the hardware self-test
make build ENV=probe

# Run the static analysis
make check
```

### Flashing your device

```bash
# Build and flash the firmware
make upload

# Build and flash the hardware self-test
make upload ENV=probe

# Use another serial port
make upload PORT=/dev/ttyACM1
```

The upload resets the chip into download mode through USB. Buttons are not necessary.

### Serial monitor

```bash
make monitor
make log LOG=device.log
```

`make log` appends the log to a file and connects again after a reset.

The monitor opens the port with DTR and RTS low. A program that raises them can reset the chip into download mode. See [troubleshooting](docs/troubleshooting.md#the-device-stays-in-download-mode) for the recovery.

Run `make help` for all commands.

---

Papwick uses [M5Unified](https://github.com/m5stack/M5Unified) and [M5GFX](https://github.com/m5stack/M5GFX) by M5Stack.

**Not affiliated with M5Stack or Etermax.**
