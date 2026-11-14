#include "Exercise.h"

namespace fit {

namespace {

const Exercise kExercises[kExerciseCount] = {
    {"March in place", "Lift knees, swing arms", 3.0f, false, MarchInPlace},
    {"Arm circles", "Small circles, arms straight", 2.3f, false, ArmCircles},
    {"Jumping jacks", "Land soft, arms overhead", 7.5f, true, StepJacks},
    {"Step jacks", "Step out, arms overhead", 3.8f, false, StepJacks},
    {"Squats", "Hips back, knees over toes", 3.8f, false, Squats},
    {"Knee push-ups", "Straight line knees to head", 3.0f, false, KneePushUps},
    {"Push-ups", "Body straight, chest to floor", 3.8f, false, PushUps},
    {"Lunges", "Front knee over ankle", 3.8f, false, Lunges},
    {"Plank", "Elbows under shoulders, tight core", 2.8f, false, Plank},
    {"Crunches", "Lift shoulders, chin off chest", 2.8f, false, Crunches},
    {"Mountain climbers", "Hips low, drive knees", 7.5f, true, KneeDrives},
    {"Knee drives", "Stand tall, knee to hands", 3.8f, false, KneeDrives},
    {"High knees", "Run in place, knees high", 7.5f, true, HighMarch},
    {"High march", "March fast, knees high", 3.8f, false, HighMarch},
    {"Burpees", "Squat, jump back, jump up", 11.0f, true, StepBackBurpees},
    {"Step-back burpees", "Squat, step back, stand up", 6.5f, false, StepBackBurpees},
    {"Glute bridge", "Squeeze glutes, hips up", 3.0f, false, GluteBridge},
    {"Wall sit", "Back on wall, knees at 90", 3.0f, false, WallSit},
    {"Leg raises", "Lower back on floor", 2.8f, false, LegRaises},
    {"Superman", "Lift arms and legs, hold", 2.8f, false, Superman},
    {"Triceps dips", "Chair behind you, elbows back", 3.8f, false, TricepsDips},
    {"Shoulder taps", "Plank, touch opposite shoulder", 3.8f, false, ShoulderTaps},
    {"Quad stretch", "Heel to glute, switch half way", 2.3f, false, QuadStretch},
    {"Hamstring stretch", "Straight leg, reach to toes", 2.3f, false, HamstringStretch},
    {"Cobra stretch", "Hips down, chest up", 2.3f, false, CobraStretch},
};

}  // namespace

const Exercise& exercise(uint8_t id) { return kExercises[id < kExerciseCount ? id : 0]; }

}  // namespace fit
