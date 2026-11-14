#include "Plan.h"

#include <algorithm>

#include "Exercise.h"

namespace fit {

namespace {

constexpr int kCircuitSize = 10;
constexpr int kMaxMainExercises = 30;
constexpr Ex kFullBody[kCircuitSize] = {JumpingJacks, Squats,    PushUps,     MountainClimbers, Lunges,
                                        Plank,        HighKnees, GluteBridge, Burpees,          Crunches};
constexpr Ex kLowerCore[kCircuitSize] = {Squats,    Lunges,       GluteBridge, HighKnees,        WallSit,
                                         LegRaises, JumpingJacks, Crunches,    MountainClimbers, Plank};
constexpr Ex kUpperCore[kCircuitSize] = {PushUps,      TricepsDips, Plank,     Superman,     MountainClimbers,
                                         ShoulderTaps, Crunches,    LegRaises, JumpingJacks, Burpees};
constexpr Ex kWarmUp[] = {MarchInPlace, ArmCircles};
constexpr Ex kCoolDown[] = {QuadStretch, HamstringStretch, CobraStretch};

constexpr int kExercisesPerLevel[] = {6, 8, 10};
constexpr int kBaseWorkSec[] = {30, 40, 45};
constexpr int kRestSec[] = {30, 20, 15};
constexpr int kMinRestSec = 5;
constexpr int kWarmSec = 30;
constexpr int kMinWorkSec = 20;
constexpr int kMaxWorkSec = 75;
constexpr int kMinAdjust = -10;
constexpr int kMaxAdjust = 15;

int rounds(Level level, int week) {
  switch (level) {
    case Level::Beginner:
      return week < 2 ? 1 : 2;
    case Level::Intermediate:
      return 2;
    default:
      return week < 2 ? 2 : 3;
  }
}

uint8_t pick(Ex id, Level level, bool lowImpact) {
  if (id == PushUps && level == Level::Beginner) id = KneePushUps;
  if (lowImpact && exercise(id).highImpact) id = exercise(id).lowImpactAlt;
  return id;
}

void add(Workout& w, uint8_t exercise, int seconds, bool rest) {
  if (w.count < kMaxSteps) w.steps[w.count++] = {exercise, static_cast<uint16_t>(seconds), rest};
}

int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

}  // namespace

DayType dayType(int day) {
  static constexpr DayType week[] = {DayType::FullBody, DayType::LowerCore, DayType::UpperCore, DayType::Rest,
                                     DayType::FullBody, DayType::LowerCore, DayType::Rest};
  return week[((day % 7) + 7) % 7];
}

const char* dayTypeName(DayType t) {
  static const char* const names[] = {"Full body", "Legs & core", "Upper & core", "Rest day"};
  return names[static_cast<int>(t)];
}

Workout buildWorkout(int day, Level level, bool lowImpact, int adjustSec, int restPercent, int exercises) {
  Workout w;
  const DayType type = dayType(day);
  if (type == DayType::Rest) return w;

  const int lv = static_cast<int>(level);
  const int week = clampInt(day / 7, 0, 3);
  const int work = clampInt(kBaseWorkSec[lv] + 5 * week + adjustSec, kMinWorkSec, kMaxWorkSec);
  const int rest = std::max(kMinRestSec, kRestSec[lv] * restPercent / 100);
  const int main =
      exercises > 0 ? clampInt(exercises, 1, kMaxMainExercises) : kExercisesPerLevel[lv] * rounds(level, week);
  const int cycle = exercises > 0 ? kCircuitSize : kExercisesPerLevel[lv];
  const Ex* circuit = type == DayType::FullBody ? kFullBody : (type == DayType::LowerCore ? kLowerCore : kUpperCore);

  for (Ex id : kWarmUp) add(w, id, kWarmSec, false);
  for (int i = 0; i < main; ++i) {
    const uint8_t id = pick(circuit[i % cycle], level, lowImpact);
    add(w, id, rest, true);
    add(w, id, work, false);
  }
  add(w, kCoolDown[0], rest, true);
  for (Ex id : kCoolDown) add(w, id, kWarmSec, false);
  return w;
}

uint32_t totalSeconds(const Workout& w) {
  uint32_t sum = 0;
  for (int i = 0; i < w.count; ++i) sum += w.steps[i].seconds;
  return sum;
}

bool isValid(const Progress& p) {
  return p.version == kProgressVersion && p.adjustSec >= kMinAdjust && p.adjustSec <= kMaxAdjust &&
         (p.doneMask >> kPlanDays) == 0;
}

int currentDay(const Progress& p) {
  for (int d = 0; d < kPlanDays; ++d) {
    if (!isDone(p, d)) return d;
  }
  return kPlanDays;
}

bool isDone(const Progress& p, int day) { return day >= 0 && day < kPlanDays && (p.doneMask >> day) & 1u; }

void markDone(Progress& p, int day) {
  if (day >= 0 && day < kPlanDays) p.doneMask |= 1u << day;
}

void applyFeedback(Progress& p, Feedback f) {
  const int delta = f == Feedback::Hard ? -5 : (f == Feedback::Easy ? 5 : 0);
  p.adjustSec = static_cast<int8_t>(clampInt(p.adjustSec + delta, kMinAdjust, kMaxAdjust));
}

}  // namespace fit
