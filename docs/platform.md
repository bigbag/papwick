# Platform

## Controls

Touch works on the content. The buttons move through the flow. Each button has one meaning on all screens.

- Tap: select an item or change an option
- Swipe left or right: change the page or the app
- Swipe up or down: turn a wheel picker
- Hold an app icon: open the app page in Settings
- Blue button (left): back, or no in a question
- Green button (right): confirm or next, or yes in a question
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
       .-'    clock      '-.         y 30..70   title band
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

The home screen shows one app at a time, with the clock, the battery level and page dots. Swipe left or right to change the app. Tap the icon or push green to open it. Settings is the last entry.

## Settings

Settings has five pages. Swipe left or right to change the page. Tap a row to change it.

- Device: brightness, sleep (30 s, 1 min, 2 min, 5 min, never), sound, vibration, units (metric or imperial)
- Time: zone, set time, sync now
- Wi-Fi: add network, and the saved networks. Tap a network to forget it
- Apps: one row for each app. The app page shows the app settings, Clear data and the number of saved NVS entries
- About: build date, battery, free RAM, reset all

The screen dims at half the sleep time.

## Wi-Fi

The radio is on only for the network setup and the time sync. The device saves up to 4 networks.

To add a network, open Settings > Wi-Fi > Add network:

- When a saved network answers, the device joins it and shows a QR code of its form page, for example `http://192.168.1.23`. Open it on a phone in the same network. Push green to use the hotspot.
- Else the device starts the hotspot `Papwick-XXXX` with a random 8-digit password, and shows a QR code to join it. The hotspot answers all names with its own address, so the phone opens the form page itself. A second QR code opens `http://192.168.4.1`.
- On the form page, select a network or type its name, and type the password. The device then connects and syncs the time.

A connection tries the saved networks in list order, 15 s each. A network that connects moves to the first position.

Security: the device has no flash encryption. A person with the device and a USB cable can read the saved passwords. While the form page is open in the home network, each device in that network can open it.

## Time

- The RTC holds UTC.
- The time zone is a UTC offset, from UTC-12 to UTC+14, with the 30 and 45 minute offsets. The zone has no daylight saving time. Change the offset when the clocks change.
- A time sync asks `pool.ntp.org` and `time.google.com` for 10 s, then writes the time to the RTC.
- The sync runs on "Sync now", after "Add network", and at start-up. The start-up sync runs when the last sync is older than 7 days, and the device is on USB or the battery has 20 % or more.

## Software

`src/main.cpp` lists the apps and starts the shell. All apps compile into one firmware image.

The shell loop reads the input, controls the light and the red button, and gives the input to the current app. It draws the app when the app is dirty, and it calls `animate()` every 50 ms.

An app implements `App` in `src/shell/Shell.h`:

- `info()`: the id (also the NVS namespace), name, icon color and icon text
- `boot()`: loads the saved data once at start-up
- `start()`, `stop()`: open and leave the app. `stop()` saves the state
- `update()`, `draw()`, `animate()`: input, full frame and small animation
- `keepAwake()`: true keeps the screen on, for example during a workout
- `settings()`, `settingsChanged()`: the setting rows for Settings > Apps
- `clearData()`: clears the app data

Rules for apps:

- Use the one full-screen sprite of `lib/Ui`. Do not make a second one.
- Do not call `M5.Speaker.begin()` or `M5.Mic.begin()`. Use `board::micBegin()` and `board::micEnd()`.
- Do not use the red button. The shell owns it.

## Storage

The data is in NVS (`Preferences`):

- `sys`: device settings
- `wifi`: saved networks
- `fit`, `g2048`: app data, one namespace for each app id

Each saved struct has a version. The firmware ignores a struct of an unknown version and uses the defaults. Reset all clears all namespaces and restarts the device.
