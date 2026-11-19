#include "apps/connect4/App.h"
#include "apps/fitness/App.h"
#include "apps/game2048/Game.h"
#include "apps/lightsout/App.h"
#include "apps/minigolf/App.h"
#include "apps/noise/App.h"
#include "apps/pomodoro/App.h"
#include "apps/sokoban/App.h"
#include "apps/stopwatch/App.h"
#include "apps/talk/App.h"
#include "shell/Shell.h"

namespace {

App* const kApps[] = {&fitnessApp(),   &pomodoroApp(), &stopwatchApp(), &game2048App(), &connect4App(),
                      &lightsOutApp(), &sokobanApp(),  &miniGolfApp(),  &talkApp(),     &noiseApp()};

}  // namespace

void setup() { shell::begin(kApps, sizeof(kApps) / sizeof(*kApps)); }

void loop() { shell::loop(); }
