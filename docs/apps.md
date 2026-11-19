# Apps

## Workout

A 28-day home workout plan with no equipment. The calorie values are estimates. The app does not give medical advice.

### Onboarding

The onboarding asks for sex, age, height, weight, target weight, push-ups in a row and low impact. A safety screen tells the user to stop at chest pain, dizziness or unusual shortness of breath. The last screen shows the BMI, the resting burn and the weeks to the target weight.

The push-up answer sets the level: 0 to 10 is beginner, 11 to 25 is intermediate, more than 25 is advanced. The app suggests low impact when the BMI is 30 or more.

### Plan

Each week has these days: full body, legs and core, upper body and core, rest, full body, legs and core, rest.

- Beginner: 6 exercises per round, 30 s work, 30 s rest. Rounds: 1 in weeks 1 and 2, then 2
- Intermediate: 8 exercises per round, 40 s work, 20 s rest. Rounds: 2
- Advanced: 10 exercises per round, 45 s work, 15 s rest. Rounds: 2 in weeks 1 and 2, then 3

More rules:

- The work time increases by 5 s each week.
- After a workout, "Hard" decreases and "Easy" increases the work time by 5 s, from -10 s to +15 s in total.
- Each workout starts with 2 warm-up exercises and ends with 3 stretches, 30 s each.
- Low impact replaces the jumping exercises with step variants. Beginners do knee push-ups.

### Workout screen

An animated stick figure shows each exercise. During rest, it shows the next exercise. Tones and vibration mark the start, the halfway point, the 3-2-1 countdown, the rest and the end.

- Green skips a step. Blue pauses the workout.
- When paused, green resumes, and blue asks to end the workout.
- A red click pauses the workout and goes home. The workout stays open until the user ends it.
- At the end, the app saves the result. Then it asks "How was it?".

The home screen of the app also opens the weight log (30 entries) and the statistics with a weight graph.

### Settings

- Exercises: "Auto" (by level and week), 10, 15 or 20, without warm-up and stretches. The round repeats as necessary
- Exercise time: short (-10 s), medium or long (+10 s)
- Rest time: short (half), medium or long (double)

### Formulas

- BMI: kg / m², classes at 18.5, 25 and 30. Source: [CDC](https://www.cdc.gov/bmi/adult-calculator/index.html)
- Resting burn: 10 × kg + 6.25 × cm − 5 × age + 5 (men) or − 161 (women), the Mifflin-St Jeor formula. Source: [Mifflin 1990](https://pubmed.ncbi.nlm.nih.gov/2305711/)
- Weeks to the target: weight difference / 0.5 kg per week. A loss of 0.5 to 1 kg a week is safe. Source: [NHS](https://www.nhs.uk/conditions/obesity/)
- Exercise kcal: MET × 3.5 × kg / 200 per minute. Source: ACSM

MET values come from the [2024 Compendium of Physical Activities](https://pacompendium.com/conditioning-exercise/):

- MET 2.3, stretching, mild (02101): arm circles, stretches
- MET 2.8, calisthenics, light (02024): plank, crunches, leg raises, superman
- MET 3.0, body weight exercises, general (02056): march in place, knee push-ups, glute bridge, wall sit
- MET 3.8, calisthenics, moderate (02022): squats, push-ups, lunges, step jacks, knee drives, high march, triceps dips, shoulder taps
- MET 6.5, body weight exercises, high intensity (02057): step-back burpees
- MET 7.5, calisthenics, vigorous (02020): jumping jacks, mountain climbers, high knees
- MET 11.0, HIIT (02214): burpees

To tune the figure poses in `src/apps/fitness/Figure.cpp`, run `python3 scripts/preview_figures.py out.png`. The script needs Pillow.

## 2048

- Swipe to move the tiles.
- Each tile value has its own color.
- Green asks to start a new game. After the game ends, green starts a new game at once.
- A merge gives a short vibration. The 2048 tile plays a tone. The end of the game plays a low tone.
- The app settings select the theme. "Soft" has soft colors and dark text. "Bright" has strong colors.
- The app saves the board and the best score when the user leaves the app.

## Pomodoro

A focus timer: 4 focus sessions of 25 minutes, with a 5-minute break after each session and a 15-minute break after the fourth.

- Green starts, pauses or resumes a focus session, and skips a break.
- Hold the screen to reset the timer. The app asks first.
- At the end of a period, the device turns the screen on, vibrates and plays a tone.
- The "Start mode" setting selects "Auto" (the next period starts at once) or "Manual" (green starts the next period).
- The screen goes off as set in Settings. The timer continues while the app is open. When you leave the app, the timer resets.

## Stopwatch

The app measures time to 1/100 s. Below one hour the time shows as mm:ss.cc. From one hour it shows as h:mm:ss.

- Green starts and stops the time. After a stop, green continues the time.
- Tap the screen during the time to end a lap. The device vibrates. The app keeps 99 laps. After the 99th lap, a tap does nothing.
- Above the time, the screen shows the number of the current lap.
- The screen shows the last three laps. With two or more different laps, the shortest lap is green and the longest lap is red.
- Hold the screen after a stop to reset the time and the laps.
- Without laps, the edge ring fills once in each minute. Its color changes in each minute.
- After a lap, the ring fills in the time of the last lap. The ring is green while the current lap is shorter than the last lap. When the current lap is longer, the ring turns red.
- At 99:59:59 the time stops. Then green does nothing, and a hold resets the time.
- While the app is open and the time runs, the screen stays on. The time also runs when you leave the app. A power off clears it.

## Four in a Row

Put four discs in a row before the device does. Tap a column to drop a disc. The row can be horizontal, vertical or diagonal.

- The device answers after the move of the player is on the screen.
- The first move alternates between the player and the device.
- Green asks to start a new game. After the game ends, green starts a new game at once.
- The "Difficulty" setting selects "Easy", "Normal" or "Hard". The device looks 1, 4 or 8 half moves ahead.
- The app saves the board and the totals of wins, losses and draws.

## Lights Out

Turn off all lights on the 5 x 5 grid. A tap changes the light and its four neighbors. Each game is solvable.

- Green asks to start a new game. After a solved game, green starts a new game at once.
- The app saves the game when the user leaves the app.

## Sokoban

Push all boxes onto the targets. A cat pushes the boxes. A box moves only when the cat pushes it, and the cat cannot pull a box.

- The app has 12 levels. They get harder, from 2 boxes to 5 boxes.
- The app opens the menu. The menu shows the level and page dots at the bottom edge. Swipe left or right to select a level. Green opens it. After a solved level, the menu selects the next level.
- Each unsolved level keeps its position and its counts when you open a different level. Only the open level keeps its undo moves.
- Swipe to move the cat.
- Green undoes the last move, up to 128 moves. After a solved level, green opens the next level.
- Hold the screen to restart the level.
- Blue goes back to the menu.
- The app saves the game when the user leaves the app.

To make new levels, run `python3 scripts/make_sokoban_levels.py 9 9 3`. The arguments are the width, the height and the number of boxes. The script prints the best levels with their push-optimal solutions. Add a level to `lib/SokobanCore/src/SokobanLevels.cpp` and its solution to `test/unit/SokobanTest.cpp`. A different number of levels also changes `kLevelCount`.

## Mini golf

Tilt the device to roll the ball into the hole. The app has 27 levels in five themes: meadow, coast, evening, autumn and winter night. The levels have walls, hills, sand and water. Some levels have a bridge or a turning windmill bar.

- The menu shows the level, its par time, the best time and the stars. Swipe left or right to select a level. A level opens when you finish the level before it.
- Before the first game, the app opens the calibration. Put the device on a flat surface and push green. The app measures the tilt for 1 s and saves it as the level. To calibrate again, hold the menu screen. A dot on the calibration screen rolls to the low side.
- Push green to open the level, and push green again to start. Hold the device flat to keep the ball still.
- During a level, green starts the level again and blue goes back to the menu.
- The time runs until the ball drops into the hole. Water puts the ball back on the tee and adds 5 s.
- A time at par or faster gets 3 stars. A time up to 1.6 times par gets 2 stars. A slower time gets 1 star.
- Sand slows the ball. A fast ball rolls over the hole.
- The accelerometer is on only during a game and on the calibration screen. The screen stays on while a level or the calibration is on the screen. After 60 s without input, the app goes back to the menu. During a game, a tilt change of the device is input. A device on a table does not change its tilt, also while the ball rolls.
- The app saves the best times and the open levels. The save has space for 40 levels. When a new version adds levels, the save stays. After the last level, the next new level is open.

The levels are in `lib/MiniGolfCore/src/Courses.cpp`. The unit test checks that each level fits the round screen and that a bot finishes each level within par. The bot finds its way with a search on a grid. To see the bot times, run the test with `GOLF_BOT=1`. Par is 1.5 times the bot time plus 1 s, rounded up to 0.5 s. The app paints the picture of a level once, when the level opens. During a level it pushes only small sprites: the ball, the windmill and the time.

## Walkie-talkie

Push-to-talk voice between two or more TriviaPODs in the same Wi-Fi network. Each device must have the app open.

- The app connects to the saved networks. Without a saved network, add one in Settings > Wi-Fi. After a failed connection, push green to try again.
- The screen shows the name of the device, for example `Papwick-1A2B`, and the other devices that have the app open. To change the name, open Settings > About > Name.
- Talks go only between paired devices. The list shows a paired device in white. It shows the status after the name of the other devices: "new", "waiting" or "offline". A paired device that is not in the network shows as "offline".
- Tap a new device to pair. The other device asks "Connect Papwick-1A2B?". Green agrees, blue refuses. Both devices save the pairing. A tone and a short text tell the result: "Connected", "Refused" or "No answer". The question closes after 25 s. Tap the device again to cancel the request.
- Hold a paired device to forget it. The other device also removes the pairing. An offline device removes it at its next talk.
- Settings > Apps > Walkie-talkie > Clear data removes all pairings.
- The app looks for devices when it opens, every 30 s, and when the user swipes down.
- Push green to talk, and push green again to stop. Or hold green while you talk. The talk then stops at the release. Blue also stops a talk. Then the app stays open. Start to speak when "Wait" changes to "Talking". The microphone needs up to 1 s to start. Without sound from the microphone for 2 s, the talk stops and the screen shows "Microphone error".
- A green ring shows the talk. A talk stops after 60 s.
- The other devices show the name of the talker and play the voice. A short tone tells the end of the talk.
- One device talks at a time. While a device talks, green on the other devices shows "Busy". When two devices start at the same time, the device with the lower id continues. The other device stops and shows "Busy".
- An incoming talk or a pairing question turns the screen on.
- The Sound setting does not change the voice and the tones of this app. The Vibration setting applies. When you leave the app, it turns the microphone off and stops its sound. Then all sound follows the Sound setting again.
- The app does not save the talks.
- The radio stays on while the app is open. Between the talks the Wi-Fi power save is on. Then a talk starts with a delay of up to about 0.3 s. During a talk and for 3 s after it, the power save is off. Go home to turn the radio off.

The devices use UDP port 47800. HELLO goes to the broadcast address. Each device answers it. The other packets go to each device. The sound is 16 kHz mono 16-bit PCM without compression, 20 ms in each packet. The sender removes hum below 100 Hz, lowers a loud voice, lowers the pauses by 12 dB and limits the peaks. The sender never raises the level. A near device plays the voice back into the microphone, and a higher level can start a feedback loop. Playback starts after 100 ms of sound. A device that leaves sends BYE. A device that does not answer a search goes off the list.

Security: the app has no encryption. The pairing is a consent step, not a protection: a host in the network can copy the id of a paired device. Each host in the network can receive the sound. Some routers isolate the clients, for example in a guest network. Then the devices cannot find each other.

To test with one device, run `python3 scripts/talk_peer.py --echo --save talk.wav` on a PC in the same network. The PC shows on the device as "Test PC". The PC asks to pair, so push green on the device. The PC sends each talk back and writes it to `talk.wav`. The PC firewall must let UDP port 47800 in.

## Noise meter

The app shows the sound level in dB(A). The ring around the edge shows the level from 30 dB to 110 dB.

- The large number is the level with the fast time weighting of 125 ms. Below it are the maximum and the average since the start or the last reset. The average is the equivalent continuous level (Leq).
- A white segment holds the peak for 1.2 s. Then it falls back to the level.
- The ring color shows how far the level is above the normal level. Blue is 10 dB or more below it. Green is at the normal level. Yellow is 10 dB above it, and red is 20 dB above it. More than 20 dB above it, the ring pulses. The pulse gets faster as the level increases.
- A white dot and a white number on the scale show the normal level. Tap the screen to select the next normal level: 30, 40, 55, 70 or 85 dB. Settings > Apps > Noise meter also has this setting. The app saves it.
- Push green to reset the maximum and the average. Blue goes back.
- Hold the screen to calibrate. Swipe the wheel to set a correction from -6 dB to +6 dB in steps of 0.5 dB. The app adds the correction to each level. Above the wheel, the app shows the average level (Leq) with the correction, to 0.1 dB. Green saves the correction. Blue keeps the last saved correction. The calibration starts the maximum and the average again. During the calibration, the screen stays on.
- The microphone runs only while the app is open and the screen is on. The app continues to measure while the power question is open. The screen turns off after the sleep time. Then the measurement stops, and the maximum and the average stay. When the screen turns on, the app starts the microphone again. The level shows "--" until the first new sound frame.
- When a sample is at full scale, "overload" shows in red for 1 s in the place of "dB(A)". Then the level is too low.
- When the microphone gives no sound for 2 s, the app shows the microphone error. Green tries again.

Reference values: WHO recommends less than 30 dB(A) in a bedroom at night and 35 dB(A) in a living room during the day.

The A weighting follows IEC 61672 within 0.1 dB up to 2 kHz. Above 4 kHz it falls faster than the standard, because the sample rate is 16 kHz. The codec also cuts the sound below about 200 Hz. The microphone sensitivity is not known. `kFullScaleDb` in `lib/NoiseCore/src/Noise.h` sets the dB value of the full scale for all devices. The correction calibrates one device.

To calibrate, put the device and a reference meter (class 1 or class 2 of IEC 61672) next to each other. A speaker at 1 m to 2 m plays steady pink noise. Open the calibration and wait 30 s. Set the correction until the Leq of the app is the same as the Leq of the reference meter. Do this again at a different level to check the result. An acoustic calibrator (IEC 60942) is more accurate, but its coupler must seal the microphone hole of the case. Use a level that gives no "overload", for example 94 dB. The microphone noise sets the lowest level that the app can show, approximately 30 dB.
