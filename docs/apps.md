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
- At the end of a period, the device turns the screen on, vibrates and plays a tone. The timer also runs on the home screen and in other apps.
- The "Start mode" setting selects "Auto" (the next period starts at once) or "Manual" (green starts the next period).
- The screen goes off as set in Settings. The timer continues. The device does not keep the timer after a power off.

## Clock

A watch face with the time, the date and the battery level. Tap the screen or push green to change between the analog and the digital face.

The face is black with white marks, so the AMOLED uses little power. The face changes once a minute. The screen goes off as set in Settings, and the clock has no always-on mode.

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

- Swipe to move the cat.
- Green undoes the last move, up to 128 moves. After a solved level, green opens the next level.
- Hold the screen to restart the level.
- The app has 12 levels. They get harder, from 2 boxes to 5 boxes. The "Level" setting opens a level.
- The app saves the game when the user leaves the app.

To make new levels, run `python3 scripts/make_sokoban_levels.py 9 9 3`. The arguments are the width, the height and the number of boxes. The script prints the best levels with their push-optimal solutions. Add a level to `lib/SokobanCore/src/SokobanLevels.cpp` and its solution to `test/unit/SokobanTest.cpp`. A different number of levels also changes `kLevelCount` and `kLevelNames`.
