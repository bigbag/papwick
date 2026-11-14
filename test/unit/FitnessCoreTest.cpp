#include "Body.h"
#include "Exercise.h"
#include "Plan.h"
#include "Session.h"
#include "Settings.h"
#include "test_utils.h"

namespace {

using namespace fit;

void testBody(TestUtils::TestRunner& r) {
  Profile p;
  r.expectTrue(isValid(p), "default profile is valid");
  r.expectNear(27.76, bmi(85, 175), 0.01, "BMI 85 kg 175 cm");
  r.expectTrue(bmiClass(24.9f) == BmiClass::Healthy && bmiClass(25.0f) == BmiClass::Overweight, "BMI 25 boundary");
  r.expectTrue(bmiClass(18.4f) == BmiClass::Underweight && bmiClass(30.0f) == BmiClass::Obese, "BMI outer classes");
  r.expectTrue(bmrKcal(p) == 1799, "Mifflin-St Jeor male");
  p.sex = Sex::Female;
  r.expectTrue(bmrKcal(p) == 1633, "Mifflin-St Jeor female");
  r.expectTrue(weeksToTarget(p) == 10, "5 kg at 0.5 kg a week is 10 weeks");
  p.targetKg = 84.8f;
  r.expectTrue(weeksToTarget(p) == 1, "partial week rounds up");
  r.expectNear(5.6525, kcal(3.8f, 85, 60), 0.001, "MET kcal for one minute");
  p.weightKg = 95;
  r.expectTrue(suggestLowImpact(p), "BMI 31 suggests low impact");
  p.age = 15;
  r.expectTrue(!isValid(p), "age below 16 is invalid");

  WeightLog log;
  for (int i = 0; i < kMaxWeights + 2; ++i) addWeight(log, i, 80.0f + i);
  r.expectTrue(isValid(log) && log.count == kMaxWeights, "weight log keeps 30 entries");
  log.entries[3].kg = NAN;
  r.expectTrue(!isValid(log), "weight log with a non-finite weight is invalid");
  log.entries[3].kg = 250.0f;
  r.expectTrue(!isValid(log), "weight log with a weight out of range is invalid");
  r.expectTrue(log.entries[0].time == 2 && log.entries[kMaxWeights - 1].kg == 80.0f + kMaxWeights + 1,
               "full weight log drops the oldest entry");
}

bool hasHighImpact(const Workout& w) {
  for (int i = 0; i < w.count; ++i) {
    if (exercise(w.steps[i].exercise).highImpact) return true;
  }
  return false;
}

bool hasExercise(const Workout& w, uint8_t id) {
  for (int i = 0; i < w.count; ++i) {
    if (w.steps[i].exercise == id) return true;
  }
  return false;
}

void testPlan(TestUtils::TestRunner& r) {
  r.expectTrue(dayType(3) == DayType::Rest && dayType(6) == DayType::Rest && dayType(10) == DayType::Rest,
               "days 4 and 7 of each week are rest days");
  r.expectTrue(buildWorkout(3, Level::Beginner, false, 0).count == 0, "rest day has no steps");

  const Workout b = buildWorkout(0, Level::Beginner, false, 0);
  r.expectTrue(b.count == 18, "beginner day 1: 2 warm-up + 6 x (rest + work) + rest + 3 cool-down");
  r.expectTrue(totalSeconds(b) == 540, "beginner day 1 lasts 9 minutes");
  r.expectTrue(exerciseTotal(b) == 11, "beginner day 1 has 11 exercise steps");
  r.expectTrue(!b.steps[0].rest && b.steps[2].rest && b.steps[2].exercise == b.steps[3].exercise,
               "rest step names the next exercise");

  r.expectTrue(buildWorkout(7, Level::Beginner, false, 0).steps[3].seconds == 35, "week 2 adds 5 s of work");
  r.expectTrue(buildWorkout(14, Level::Beginner, false, 0).count == 30, "week 3 beginner does 2 rounds");
  r.expectTrue(buildWorkout(0, Level::Beginner, false, -40).steps[3].seconds == 20, "work time stays >= 20 s");

  r.expectTrue(hasExercise(buildWorkout(2, Level::Beginner, false, 0), KneePushUps), "beginner gets knee push-ups");
  r.expectTrue(hasExercise(buildWorkout(2, Level::Advanced, false, 0), PushUps), "advanced gets push-ups");
  r.expectTrue(hasHighImpact(buildWorkout(0, Level::Advanced, false, 0)), "normal plan has jumps");
  r.expectTrue(!hasHighImpact(buildWorkout(0, Level::Advanced, true, 0)), "low impact plan has no jumps");
  r.expectTrue(buildWorkout(21, Level::Advanced, false, 0).count <= kMaxSteps, "advanced week 4 fits");
  r.expectTrue(buildWorkout(0, Level::Beginner, false, 0, 200).steps[2].seconds == 60, "long rest doubles rest");
  r.expectTrue(buildWorkout(0, Level::Advanced, false, 0, 10).steps[2].seconds == 5, "rest stays >= 5 s");
  r.expectTrue(exerciseTotal(buildWorkout(0, Level::Beginner, false, 0, 100, 5)) == 10, "5 exercises: 2 + 5 + 3");
  const Workout twenty = buildWorkout(0, Level::Beginner, false, 0, 100, 20);
  r.expectTrue(exerciseTotal(twenty) == 25 && twenty.count <= kMaxSteps, "20 exercises repeat the circuit");
  r.expectTrue(twenty.steps[3].exercise == twenty.steps[23].exercise, "exercise 11 repeats exercise 1");
  r.expectTrue(exerciseTotal(buildWorkout(21, Level::Advanced, false, 0, 100, 99)) == 35, "count clamps to 30");

  bool allValid = true;
  for (int day = 0; day < kPlanDays; ++day) {
    for (Level lv : {Level::Beginner, Level::Intermediate, Level::Advanced}) {
      for (bool low : {false, true}) {
        for (int adjust : {-10, 15}) {
          for (int8_t work : kWorkOffsetSec) {
            for (uint8_t restPct : kRestPercent) {
              for (uint8_t count : kExerciseCounts) {
                const Workout w = buildWorkout(day, lv, low, adjust + work, restPct, count);
                bool ok = (dayType(day) == DayType::Rest) == (w.count == 0) && w.count < kMaxSteps &&
                          (count == 0 || w.count == 0 || exerciseTotal(w) == count + 5) && !(low && hasHighImpact(w));
                for (int i = 0; i < w.count; ++i) {
                  ok = ok && w.steps[i].exercise < kExerciseCount && w.steps[i].seconds >= 5;
                }
                allValid = allValid && ok;
              }
            }
          }
        }
      }
    }
  }
  r.expectTrue(allValid, "each day, level and setting gives a full workout with known exercises");

  Progress bad;
  bad.doneMask = 1u << kPlanDays;
  r.expectTrue(!isValid(bad), "a done bit past day 28 is invalid");
  bad = Progress{};
  bad.adjustSec = 16;
  r.expectTrue(!isValid(bad), "adjust past 15 s is invalid");
  bad = Progress{};
  markDone(bad, kPlanDays);
  markDone(bad, -1);
  r.expectTrue(bad.doneMask == 0, "days outside the plan are not marked");

  Progress p;
  r.expectTrue(isValid(p) && currentDay(p) == 0, "new progress starts at day 1");
  markDone(p, 0);
  markDone(p, 1);
  r.expectTrue(currentDay(p) == 2 && isDone(p, 1), "current day is the first open day");
  for (int d = 0; d < kPlanDays; ++d) markDone(p, d);
  r.expectTrue(currentDay(p) == kPlanDays && isValid(p), "complete plan");
  for (int i = 0; i < 5; ++i) applyFeedback(p, Feedback::Hard);
  r.expectTrue(p.adjustSec == -10, "hard feedback stops at -10 s");
  applyFeedback(p, Feedback::Easy);
  r.expectTrue(p.adjustSec == -5, "easy feedback adds 5 s");
}

void testSession(TestUtils::TestRunner& r) {
  Workout w;
  w.steps[0] = {Squats, 30, false};
  w.steps[1] = {Plank, 10, true};
  w.steps[2] = {Plank, 4, false};
  w.count = 3;

  Session s;
  Cue c[kMaxCues];
  r.expectTrue(start(s, &w, 80, c, kMaxCues) == 1 && c[0] == Cue::GetReady, "start cues get ready");
  r.expectTrue(nextExercise(s) == &w.steps[0], "get ready shows the first exercise");
  r.expectTrue(update(s, 7000, c, kMaxCues) == 1 && c[0] == Cue::Three, "3 s left cues three");
  r.expectTrue(update(s, 1000, c, kMaxCues) == 1 && c[0] == Cue::Two, "2 s left cues two");
  r.expectTrue(update(s, 1000, c, kMaxCues) == 1 && c[0] == Cue::One, "1 s left cues one");
  r.expectTrue(update(s, 1000, c, kMaxCues) == 1 && c[0] == Cue::Go && s.step == 0, "get ready ends with go");
  r.expectTrue(exerciseNumber(s) == 1, "first exercise is number 1");

  r.expectTrue(update(s, 14999, c, kMaxCues) == 0, "no cue before halfway");
  r.expectTrue(update(s, 1, c, kMaxCues) == 1 && c[0] == Cue::Halfway, "halfway at 15 s");

  togglePause(s);
  r.expectTrue(update(s, 60000, c, kMaxCues) == 0 && s.elapsedMs == 15000, "paused session does not move");
  togglePause(s);

  const int n = update(s, 15000, c, kMaxCues);
  r.expectTrue(n == 1 && c[0] == Cue::Rest, "a jump to the step end skips the countdown");
  r.expectTrue(s.step == 1 && exerciseNumber(s) == 2 && nextExercise(s) == &w.steps[2], "rest shows next exercise");
  r.expectNear(kcal(3.8f, 80, 30), s.kcal, 0.001, "kcal counts work time only");

  r.expectTrue(skip(s, c, kMaxCues) == 1 && c[0] == Cue::Go && s.step == 2, "skip leaves rest");
  r.expectTrue(update(s, 4000, c, kMaxCues) == 1 && c[0] == Cue::Done && s.finished, "short step has no countdown");
  r.expectTrue(s.workMs == 34000, "work time sums exercise steps");
  r.expectTrue(update(s, 1000, c, kMaxCues) == 0, "finished session does nothing");

  Workout empty;
  r.expectTrue(start(s, &empty, 80, c, kMaxCues) == 1 && c[0] == Cue::Done, "empty workout ends at once");

  start(s, &w, 80, c, kMaxCues);
  r.expectTrue(exerciseNumber(s) == 1, "get ready counts the first exercise");
  r.expectTrue(skip(s, c, kMaxCues) == 1 && c[0] == Cue::Go && s.step == 0, "skip ends get ready");

  start(s, &w, 80, c, kMaxCues);
  const int all = update(s, 54000, c, kMaxCues);
  r.expectTrue(all == 5 && c[0] == Cue::Go && c[1] == Cue::Halfway && c[2] == Cue::Rest && c[3] == Cue::Go &&
                   c[4] == Cue::Done && s.finished && s.workMs == 34000,
               "one long update runs all steps in order");
}

void testCatalog(TestUtils::TestRunner& r) {
  bool ok = true;
  for (int i = 0; i < kExerciseCount; ++i) {
    const Exercise& e = exercise(i);
    ok = ok && e.met > 0 && !exercise(e.lowImpactAlt).highImpact;
  }
  r.expectTrue(ok, "each low impact alternative is low impact");
  r.expectTrue(&exercise(kExerciseCount) == &exercise(0), "an unknown id gives the first exercise");
}

void testSettings(TestUtils::TestRunner& r) {
  Settings s;
  r.expectTrue(isValid(s), "default settings are valid");
  s.exercises = sizeof(kExerciseCounts);
  r.expectTrue(!isValid(s), "exercise count index past the table is invalid");
  s = Settings{};
  s.version = kSettingsVersion - 1;
  r.expectTrue(!isValid(s), "an old settings version is invalid");

  SettingsV3 old;
  old.sleep = 4;
  old.units = Units::Imperial;
  old.volume = 0;
  old.vibration = false;
  old.brightness = 3;
  old.workTime = 2;
  old.restTime = 0;
  old.exercises = 3;
  sys::Device d;
  d.zone = 3;
  r.expectTrue(migrate(old, s, d) && isValid(s) && sys::isValid(d), "version 3 settings migrate");
  r.expectTrue(s.workTime == 2 && s.restTime == 0 && s.exercises == 3, "migration keeps the workout settings");
  r.expectTrue(d.sleep == 4 && d.units == 1 && d.volume == 0 && !d.vibration && d.brightness == 3 && d.zone == 3,
               "migration moves the device settings and keeps the others");
  old.sleep = 5;
  s = Settings{};
  d = sys::Device{};
  r.expectTrue(!migrate(old, s, d) && s.workTime == 1 && d.sleep == 2, "invalid version 3 settings change nothing");
  r.expectTrue(sizeof(SettingsV3) == 9, "version 3 layout is 9 bytes as saved");

  bool lbOk = true;
  const Range wr = weightRange(Units::Imperial);
  for (float lb = wr.min; lb <= wr.max; lb += wr.step) {
    Profile p;
    p.weightKg = fromWeightUnits(lb, Units::Imperial);
    lbOk = lbOk && isValid(p) && toWeightUnits(p.weightKg, Units::Imperial) == lb;
  }
  r.expectTrue(lbOk, "each lb value gives a valid weight and shows the same lb again");
  bool inOk = true;
  const Range hr = heightRange(Units::Imperial);
  for (float in = hr.min; in <= hr.max; in += hr.step) {
    Profile p;
    p.heightCm = fromHeightUnits(in, Units::Imperial);
    inOk = inOk && isValid(p) && toHeightUnits(p.heightCm, Units::Imperial) == in;
  }
  r.expectTrue(inOk, "each inch value gives a valid height and shows the same inch again");

  r.expectTrue(toWeightUnits(85, Units::Imperial) == 187, "85 kg shows as 187 lb");
  r.expectNear(85, fromWeightUnits(toWeightUnits(85, Units::Imperial), Units::Imperial), 0.25, "lb round trip");
  r.expectTrue(toHeightUnits(175, Units::Imperial) == 69 && fromHeightUnits(69, Units::Imperial) == 175,
               "175 cm shows as 69 in");
  for (Units u : {Units::Metric, Units::Imperial}) {
    Profile p;
    p.weightKg = fromWeightUnits(weightRange(u).min, u);
    p.targetKg = fromWeightUnits(weightRange(u).max, u);
    p.heightCm = fromHeightUnits(heightRange(u).min, u);
    const bool lowOk = isValid(p);
    p.heightCm = fromHeightUnits(heightRange(u).max, u);
    r.expectTrue(lowOk && isValid(p), u == Units::Metric ? "metric ranges are valid" : "imperial ranges are valid");
  }
}

}  // namespace

int main() {
  TestUtils::TestRunner runner("FitnessCore");
  testBody(runner);
  testPlan(runner);
  testSession(runner);
  testCatalog(runner);
  testSettings(runner);
  return runner.exitCode();
}
