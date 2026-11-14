#pragma once

#include <cstdint>

#include "Body.h"

namespace fit {

constexpr int kPlanDays = 28;
constexpr int kMaxSteps = 80;
constexpr uint8_t kProgressVersion = 1;

enum class DayType : uint8_t { FullBody, LowerCore, UpperCore, Rest };
enum class Feedback : uint8_t { Hard, Ok, Easy };

struct Step {
  uint8_t exercise;
  uint16_t seconds;
  bool rest;
};

struct Workout {
  Step steps[kMaxSteps];
  uint8_t count = 0;
};

struct Progress {
  uint8_t version = kProgressVersion;
  int8_t adjustSec = 0;
  uint32_t doneMask = 0;
  uint16_t workouts = 0;
  uint32_t activeSec = 0;
  float kcal = 0.0f;
};

DayType dayType(int day);
const char* dayTypeName(DayType t);
// restPercent scales the rest steps. A rest step is at least 5 s.
// exercises sets the main exercises of the workout, without warm-up and cool-down. The circuit repeats as needed.
// 0 uses the level default: the level circuit size times the rounds of the week.
Workout buildWorkout(int day, Level level, bool lowImpact, int adjustSec, int restPercent = 100, int exercises = 0);
uint32_t totalSeconds(const Workout& w);

bool isValid(const Progress& p);
// Returns the first day that is not done, or kPlanDays when the plan is complete.
int currentDay(const Progress& p);
bool isDone(const Progress& p, int day);
void markDone(Progress& p, int day);
void applyFeedback(Progress& p, Feedback f);

}  // namespace fit
