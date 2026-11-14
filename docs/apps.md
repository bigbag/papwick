# Apps

## Workout

A 28-day home workout plan with no equipment. The calorie values are estimates. The app does not give medical advice.

### Onboarding

The onboarding asks for units, sex, age, height, weight, target weight, push-ups in a row and low impact. A safety screen tells the user to stop at chest pain, dizziness or unusual shortness of breath. The last screen shows the BMI, the resting burn and the weeks to the target weight.

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
- The app settings turn the sound and the vibration on or off.
- The app settings also select the theme. "Soft" has soft colors and dark text. "Bright" has strong colors.
- The app saves the board and the best score when the user leaves the app.
