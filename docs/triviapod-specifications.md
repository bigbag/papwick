# TriviaPOD Specifications

The M5Stack TriviaPOD (SKU TP-01-A) is an ESP32-S3R8 handheld with a 1.75 inch round AMOLED touchscreen. It ships with trivia firmware from Etermax. M5Stack publishes a feature list, but no pin map, schematic or board support.

The official comparison table puts the TriviaPOD next to the [M5Stack StopWatch](https://docs.m5stack.com/en/core/StopWatch). Both devices use the same SoC, display, IMU and RTC. This document uses the StopWatch support in M5GFX and M5Unified 0.3.0 as the reference. A value is **verified** only after a test on the real device.

Sources:

- [TriviaPOD product page](https://docs.m5stack.com/en/core/TriviaPOD)
- [TriviaPOD PDF datasheet](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/static/pdf/static/en/core/TriviaPOD.pdf)
- [M5GFX StopWatch board spec](https://github.com/m5stack/M5GFX/blob/master/extras/board_spec/boards/m5stopwatch.json)
- [StopWatch schematic](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1242/C152-SCH_Stopwatch_PRJ_Main_VA_20251201_2026_04_24_17_46_22.pdf)
- M5Unified 0.3.0 `src/M5Unified.inl` and `src/utility/Power_Class.inl`

## Hardware

- SoC: ESP32-S3R8 (QFN56), revision v0.2, dual-core LX7 at 240 MHz, Wi-Fi 2.4 GHz, BLE 5 (source: Official, esptool)
- PSRAM: 8 MB octal, in package (AP_3v3) (source: Official, esptool)
- Flash: 32 MB, DIO mode, 80 MHz (source: Official, esptool, stock bootloader header)
- Crystal: 40 MHz (source: esptool)
- Display: 1.75 in round AMOLED, 466 x 466, QSPI, CO5300 driver (480 x 480 memory) (source: Official size; driver from StopWatch)
- Touch: CST816S at 0x15 or CST9217 at 0x5A (source: StopWatch)
- Buttons: 2 programmable + 1 power (source: Official)
- Haptics: vibration motor on M5IOE1 PWM1 (G9), 2 kHz (source: Official motor; pin from StopWatch)
- Audio: ES8311 codec at 0x18, 8 ohm 1 W 1318 cavity speaker, MEMS microphone (source: Official speaker and mic; codec from StopWatch)
- IMU: BMI270 at 0x68 (source: Official)
- RTC: RX8130CE at 0x32 (source: Official)
- PMIC: M5PM1 at 0x6E (source: StopWatch)
- IO expander: M5IOE1 at 0x4F (source: StopWatch)
- Battery: 450 mAh Li-ion, USB-C 5 V charge (source: Official)
- USB: native USB Serial/JTAG, VID 303a, PID 1001 (source: Verified)
- Expansion: none. No Grove port, no rear header, no magnet (source: Official)

## Pin map

- GPIO 1: Button B (green), active low, external pull-up (verified)
- GPIO 2: Button A (blue), active low, external pull-up (verified)
- GPIO 12: M5PM1 interrupt (from the StopWatch)
- GPIO 13: Touch interrupt (verified)
- GPIO 15: I2S WS (verified)
- GPIO 16: I2S data in (microphone, I2S1) (verified)
- GPIO 17: I2S BCK (verified)
- GPIO 18: I2S MCK (verified)
- GPIO 21: I2S data out (speaker, I2S0) (verified)
- GPIO 38: Display TE input (from the StopWatch)
- GPIO 39: Display CS (verified)
- GPIO 40: QSPI SCLK (verified)
- GPIO 41, 42, 46, 45: QSPI IO0, IO1, IO2, IO3 (verified)
- GPIO 47: Internal I2C SDA, port 1 (verified)
- GPIO 48: Internal I2C SCL, port 1 (verified)
- GPIO 19, 20: USB D-, D+ (verified)
- GPIO 26 to 37: Octal PSRAM (verified)
- GPIO 0: BOOT, driven by M5PM1 BOOT_OUT_OD on the StopWatch (from the StopWatch)

M5IOE1 lines:

- G3: Audio codec power
- G9 (PWM1): Vibration motor
- G10: Speaker amplifier enable

The speaker and the microphone share the ES8311 codec and the I2S clock pins. Stop one before you start the other.

## Stock flash layout

The stock partition table at 0x8000:

- nvs: data/nvs at 0x9000, 20 KB
- otadata: data/ota at 0xE000, 8 KB
- recovery: app/factory at 0x10000, 3584 KB
- app0: app/ota_0 at 0x390000, 15360 KB
- spiffs: data/spiffs at 0x1290000, 10240 KB
- ram_data: data/spiffs at 0x1C90000, 64 KB
- coredump: data/coredump at 0x1CA0000, 64 KB

## Probe results

The `probe` example gives these results on a TP-01-A unit. M5Unified 0.3.0 runs the device as board 30 (`board_M5StopWatch`).

- Display: Works. M5GFX reports 468 x 468; the visible round area is 466 px wide
- Touch: Works, CST816S at 0x15
- Buttons A, B, PWR: Work
- Speaker: Works through `M5.Speaker`
- Microphone: Works through `M5.Mic`
- Vibration: Works through `M5.Power.setVibration()`
- IMU: Works, 1 g on Z when flat
- Battery level: Works, `M5.Power.getBatteryLevel()`
- I2C scan before audio power-up: 0x15, 0x32, 0x4F, 0x68, 0x6E

The ES8311 (0x18) does not answer the scan in `setup()`. Its power rail (M5IOE1 G3) is off until `M5.Speaker` or `M5.Mic` starts.

An independent ESPHome port, [menemy/triviapod-esphome-demo](https://github.com/menemy/triviapod-esphome-demo), uses the same pins and I2C addresses.

## Display performance

A full-screen push of a 466 x 466 16-bit sprite takes about 32 ms. A full-screen redraw in a PSRAM sprite takes about 20 ms. Redraw the full screen only when necessary. Draw small moving parts directly on the display or in small sprites.

## RTC

The firmware keeps UTC in the RX8130CE. When the RTC has no valid date, `M5.Rtc.getDateTime()` gives -1 in all fields. Then `board::syncClock()` sets the RTC from the build time. The StopWatch backs the RTC with a 10 uF capacitor only, so an empty battery can erase the time.

## Serial log

`lib/Logging` prints `[millis] [LVL] [origin] message` lines on the USB serial port. The `ENABLE_SERIAL_LOG` and `LOG_LEVEL` build flags in `platformio.ini` control the output. Use `make monitor` to read it.

## Power button and power-on

- The M5PM1 register 0x48 holds the red button state. Bit 0 is the press now. Bit 7 is a press since the last read, and a read clears it. `board::poll()` reads the register in each loop and counts the clicks.
- By default, the M5PM1 powers the device off on a double click (register 0x4A, bit 0 `DOUBLE_OFF_DIS` = 0). `board::beginInput()` sets the bit at each start. The M5PM1 keeps the value while it has power.
- The M5PM1 click events (register 0x42) do not report a double click when `DOUBLE_OFF_DIS` is set. M5Unified uses these events for `M5.BtnPWR`, so the firmware does not use `M5.BtnPWR`.
- The press that powers the device on stays in bit 7 of register 0x48. `board::beginInput()` reads the register once to clear it.
- With USB, `M5.Power.powerOff()` cuts the display power, but the ESP32 continues to run. Thus the firmware powers off only on battery. `M5.Power.getVBUSVoltage()` reads about 4850 mV with USB.
- The LED next to the USB port is the M5PM1 LED_EN pin. `M5.Power.M5pm1.setLedEnLevel(false)` turns it off. Verified on the device. The firmware uses it as a charge LED.
- After a cold power-on, the display can stay black while sound and buttons work. The M5IOE1 that powers the panel starts after the display init. A warm reset always shows the display. `board::begin()` restarts the ESP32 once after a power-on reset.
