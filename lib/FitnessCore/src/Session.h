#pragma once

#include <cstdint>

#include "Plan.h"

namespace fit {

enum class Cue : uint8_t { GetReady, Go, Rest, Halfway, Three, Two, One, Done };

constexpr uint32_t kGetReadyMs = 10000;
constexpr int kMaxCues = 8;

struct Session {
  const Workout* workout = nullptr;
  int step = -1;  // -1 is the get-ready countdown
  uint32_t elapsedMs = 0;
  bool paused = false;
  bool finished = false;
  bool halfwayCued = false;
  int countdown = 3;
  uint32_t workMs = 0;
  float kcal = 0.0f;
  float weightKg = 0.0f;
};

// Each function writes the cues it raises to out (at most maxOut) and returns their count.
int start(Session& s, const Workout* w, float weightKg, Cue* out, int maxOut);
int update(Session& s, uint32_t dtMs, Cue* out, int maxOut);
int skip(Session& s, Cue* out, int maxOut);
void togglePause(Session& s);

const Step* currentStep(const Session& s);
// Returns the next exercise that is not a rest step, or nullptr at the end.
const Step* nextExercise(const Session& s);
uint32_t stepDurationMs(const Session& s);
uint32_t remainingMs(const Session& s);
// Position of the current exercise among the exercise steps, from 1. Rest steps count the next exercise.
int exerciseNumber(const Session& s);
int exerciseTotal(const Workout& w);

}  // namespace fit
