# Apps

## Workout

A 28-day home workout plan with no equipment. The calorie values are estimates. The app does not give medical advice.

### Onboarding

The onboarding asks for units, sex, age, height, weight, target weight, push-ups in a row and low impact. A safety screen tells the user to stop at chest pain, dizziness or unusual shortness of breath. The last screen shows the BMI, the resting burn and the weeks to the target weight.

The push-up answer sets the level: 0 to 10 is beginner, 11 to 25 is intermediate, more than 25 is advanced. The app suggests low impact when the BMI is 30 or more.

### Plan

Each week has these days: full body, legs and core, upper body and core, rest, full body, legs and core, rest.

| Level | Exercises per round | Work | Rest | Rounds |
| --- | --- | --- | --- | --- |
| Beginner | 6 | 30 s | 30 s | 1 in weeks 1 and 2, then 2 |
| Intermediate | 8 | 40 s | 20 s | 2 |
| Advanced | 10 | 45 s | 15 s | 2 in weeks 1 and 2, then 3 |

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

| Row | Options |
| --- | --- |
| Exercises | Auto (by level and week), 10, 15 or 20 without warm-up and stretches. The round repeats as necessary |
| Exercise time | Short (-10 s), medium, long (+10 s) |
| Rest time | Short (half), medium, long (double) |

### Formulas

| Value | Formula | Source |
| --- | --- | --- |
| BMI | kg / m², classes at 18.5, 25 and 30 | [CDC](https://www.cdc.gov/bmi/adult-calculator/index.html) |
| Resting burn | Mifflin-St Jeor: 10 × kg + 6.25 × cm − 5 × age + 5 (men) or − 161 (women) | [Mifflin 1990](https://pubmed.ncbi.nlm.nih.gov/2305711/) |
| Weeks to the target | Weight difference / 0.5 kg per week | [NHS](https://www.nhs.uk/conditions/obesity/): 0.5 to 1 kg a week is safe |
| Exercise kcal | MET × 3.5 × kg / 200 per minute | ACSM |

MET values come from the [2024 Compendium of Physical Activities](https://pacompendium.com/conditioning-exercise/):

| MET | Compendium activity | Exercises |
| --- | --- | --- |
| 2.3 | Stretching, mild (02101) | Arm circles, stretches |
| 2.8 | Calisthenics, light (02024) | Plank, crunches, leg raises, superman |
| 3.0 | Body weight exercises, general (02056) | March in place, knee push-ups, glute bridge, wall sit |
| 3.8 | Calisthenics, moderate (02022) | Squats, push-ups, lunges, step jacks, knee drives, high march, triceps dips, shoulder taps |
| 6.5 | Body weight exercises, high intensity (02057) | Step-back burpees |
| 7.5 | Calisthenics, vigorous (02020) | Jumping jacks, mountain climbers, high knees |
| 11.0 | HIIT (02214) | Burpees |

To tune the figure poses in `src/apps/fitness/Figure.cpp`, run `python3 scripts/preview_figures.py out.png`. The script needs Pillow.

## 2048

- Swipe to move the tiles.
- Green asks to start a new game. After the game ends, green starts a new game at once.
- A merge gives a short vibration. The 2048 tile plays a tone. The end of the game plays a low tone.
- The app settings turn the sound and the vibration on or off.
- The app saves the board and the best score when the user leaves the app.
