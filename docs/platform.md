# Platform

## Controls

Touch works on the content. The buttons move through the flow. Each button has one meaning on all screens.

- Tap: select an item or change an option
- Swipe left or right: change the page or the app
- Swipe up or down: turn a wheel picker
- Hold an app icon on the home screen: move the app
- Blue button (left): back, or no in a question
- Green button (right): the main action of the screen, for example confirm, next or start, or yes in a question
- Red click: go to the home screen. On the home screen, the click turns the screen off. When the screen is off, the click turns it on
- Red double click: the "Power off?" question

More rules:

- A screen has no Back, OK or Next buttons. The bottom band shows the labels of the blue and green buttons.
- On-screen controls do not use blue or green. These colors mean the physical buttons.
- A wheel picker shows the value in the center and two neighbors above and below it. Swipe up or down to turn it. Tap a neighbor to select it. Hold above or below the center to scroll.
- When the screen is off, the first input only turns the screen on.
- The firmware reads the red button state from the M5PM1 and counts the clicks. A click acts about 350 ms after the release, because the firmware waits for a second click.
- A hold of the red button has no function. With USB, a long hold starts the download mode.
- With USB, the M5PM1 cannot cut the power. The question is then "Screen off? (USB)", and yes turns the screen off.

## Screen layout

```
          +-------------+
       .-'    time       '-.         y 30..70   title band
     /                       \
    |   +-------------------+ |      inscribed square 68..397
    |   |      content      | |      touch targets 56 px or more
    |   +-------------------+ |
     \   (o) back   (o) next /       y 380..420 button labels
       '-.     battery     .-'
          +-------------+
     edge ring r 214..232: progress and page dots
```

Near the bottom edge, a touch lands lower than the finger aims. Thus the hit area of a control extends 24 px down (`ui::Rect::hit`).

## Home screen

The home screen shows one app at a time, with the time, the date, the battery icon and page dots. The battery icon shows the level. A bolt shows USB power. The fill is amber at 20 % or less and red at 10 % or less. Swipe left or right to change the app. Tap the icon or push green to open it. Settings is the last entry.

To change the order of the apps, hold the icon of an app:

- The icon gets smaller. Arrows at its sides show the directions in which the app can move.
- Swipe left or right, or tap an arrow, to move the app one place. The page dots show the new place.
- Green keeps the new order. Blue restores the old order.
- Tap the icon to keep the order and open the app page in Settings.
- Settings stays the last entry. Settings > Apps shows the apps in the same order.

## Settings

Settings has a page for each group. Swipe left or right to change the page. Tap a row to change it.

- Device: brightness, sleep (30 s, 1 min, 2 min, 5 min, never), sound, vibration, units (metric or imperial)
- Time: zone, set time, sync now
- Wi-Fi: add network, and the saved networks. Tap a network to forget it
- Apps: one row for each app, five apps on each page. The app page shows the app settings, Clear data and the number of saved NVS entries
- About: device name, firmware version, battery, free RAM, reset all

## Power

- The screen dims at half the sleep time.
- When a frame has no changes, the main loop waits 10 ms. During the wait, the idle task halts the CPU until the next interrupt (WAITI).
- While the screen is off, the CPU runs at 80 MHz instead of 240 MHz. A lower clock stops the PLL, and then USB, Wi-Fi and I2S stop.
- The speaker amplifier and the codec DAC are on only during a tone or a voice, and for 3 s after it. With the sound off, tones do not turn them on.
- With the vibration off, the motor gets no power.
- The shell puts the IMU into suspend mode at start-up. Only Mini golf uses it. Mini golf turns on only the accelerometer, and only during a game and on its calibration screen.
- The AMOLED uses power only for lit pixels. Use dark backgrounds.

## Wi-Fi

The radio is on only for the network setup, the time sync and while the Walkie-talkie app is open. The device saves up to 4 networks.

To add a network or to set the device name, open Settings > Wi-Fi > Add network or Settings > About > Name:

- When a saved network answers, the device joins it and shows a QR code of its form page, for example `http://192.168.1.23`. Open it on a phone in the same network. Push green to use the hotspot.
- Else the device starts the hotspot `Papwick-XXXX` with a random 8-digit password, and shows a QR code to join it. The hotspot answers all names with its own address, so the phone opens the form page itself. A second QR code opens `http://192.168.4.1`.
- On the form page, type the device name, select a network or type its name, and type the password. The device then connects and syncs the time.
- The device name has up to 15 Latin letters, digits, spaces and signs. The walkie-talkie shows it. An empty name gives `Papwick-XXXX`. "No change" keeps the networks and saves only the name.

A connection tries the saved networks in list order, 15 s each. A network that connects moves to the first position.

Security: the device has no flash encryption. A person with the device and a USB cable can read the saved passwords. While the form page is open in the home network, each device in that network can open it.

## Time

- The RTC holds UTC.
- The time zone is a UTC offset, from UTC-12 to UTC+14, with the 30 and 45 minute offsets. The zone has no daylight saving time. Change the offset when the clocks change.
- A time sync asks `pool.ntp.org` and `time.google.com` for up to 40 s, then writes the time to the RTC. lwIP waits up to 5 s before the first request and 15 s for each server.
- The sync runs on "Sync now", after "Add network", and at start-up. The start-up sync runs at each start, when the device is on USB or the battery has 20 % or more. The RTC has no battery, so a power off stops its time.
- When the RTC lost the time, the home screen shows "Time not synced" instead of the date. A sync or "Set time" removes the text.

## Software

`src/main.cpp` lists the apps and starts the shell. All apps compile into one firmware image. `[papwick] version` in `platformio.ini` sets the firmware version. The log shows the version and the reset reason at start-up.

The shell loop reads the input, controls the light and the red button, and gives the input to the current app. It draws the app when the app is dirty, and it calls `animate()` once in each frame period of the app.

An app implements `App` in `src/shell/Shell.h`:

- `info()`: the id (also the NVS namespace), name, icon color and pictogram. The pictograms are in `lib/Ui/src/Icons.cpp` and use one stroke width, except the gear teeth
- `boot()`: loads the saved data once at start-up
- `start()`, `stop()`: open and leave the app. `stop()` saves the state
- `update()`, `draw()`, `animate()`: input, full frame and small animation
- `frameMs()`: the frame period. The default is 50 ms. Mini golf uses 25 ms while a level or the calibration is on the screen
- `background()`: runs in each loop for each app, also while the screen is off, for example for the Pomodoro timer
- `keepAwake()`: true keeps the screen on, for example during a workout
- `settings()`, `settingsChanged()`: the setting rows for Settings > Apps
- `clearData()`: clears the app data

Rules for apps:

- Use the one full-screen sprite of `lib/Ui`. Do not make a second one. Mini golf keeps the picture of the level in a PSRAM buffer, and it frees the buffer in `stop()`.
- Do not call `M5.Speaker.begin()` or `M5.Mic.begin()`. Use `board::micBegin()` and `board::micEnd()`.
- Do not use the red button. The shell owns it.
- When the user leaves the app, stop its activity. Pomodoro resets its timer, also a paused timer. Keep the state of a game and of a paused workout. The stopwatch is an exception: its time continues.
- Get each user interface text from `tr(KEY)` of `lib/I18n`. Do not write texts in the code.
- Call `tr()` at run time. Do not call it in a static initializer. Fill option lists in a function-local static.
- Write a gesture hint, for example "Hold the screen to reset", in DejaVu18 or larger. On this display, DejaVu12 is about 1.1 mm high. Wear OS asks for 12 sp, that is about 20 px.

## Texts

`lib/I18n/src/I18n.h` holds one list of keys with the English texts. `tr(KEY)` returns the text of the current language. Only English exists now. The `STR_` prefix of the keys stops a clash with Arduino macros such as `LOW` and `DEC`.

To add a text, add a line to the list. Keys that start with `FMT_` are printf formats. The form page and the serial log stay in English.

The Workout app does not use `tr()` yet.

The DejaVu fonts hold only Latin characters. The button hints use Open Sans 16 with Latin and Cyrillic characters. `scripts/make_font.py` converts a TTF font into a LovyanGFX font header, for example:

```bash
python3 scripts/make_font.py /usr/share/fonts/TTF/OpenSans-Regular.ttf 16 OpenSans16 lib/Ui/src/OpenSans16.h
```

The script needs Pillow. A language with other characters needs such fonts for all texts.

## Storage

The data is in NVS (`Preferences`):

- `sys`: device settings and the app order
- `wifi`: saved networks
- `fit`, `g2048`, `pomodoro`, `connect4`, `lights`, `sokoban`: app data, one namespace for each app id

Each saved struct has a version. The firmware ignores a struct of an unknown version and uses the defaults. Reset all clears all namespaces and restarts the device.
