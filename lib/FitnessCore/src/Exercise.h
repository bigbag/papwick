#pragma once

#include <cstdint>

namespace fit {

enum Ex : uint8_t {
  MarchInPlace,
  ArmCircles,
  JumpingJacks,
  StepJacks,
  Squats,
  KneePushUps,
  PushUps,
  Lunges,
  Plank,
  Crunches,
  MountainClimbers,
  KneeDrives,
  HighKnees,
  HighMarch,
  Burpees,
  StepBackBurpees,
  GluteBridge,
  WallSit,
  LegRaises,
  Superman,
  TricepsDips,
  ShoulderTaps,
  QuadStretch,
  HamstringStretch,
  CobraStretch,
  kExerciseCount
};

struct Exercise {
  const char* name;
  const char* cue;
  // MET from the 2024 Compendium of Physical Activities (see docs/apps.md).
  float met;
  bool highImpact;
  Ex lowImpactAlt;
};

const Exercise& exercise(uint8_t id);

}  // namespace fit
