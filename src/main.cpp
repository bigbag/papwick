#include "apps/clock/App.h"
#include "apps/connect4/App.h"
#include "apps/fitness/App.h"
#include "apps/game2048/Game.h"
#include "apps/lightsout/App.h"
#include "apps/pomodoro/App.h"
#include "apps/sokoban/App.h"
#include "shell/Shell.h"

namespace {

App* const kApps[] = {&fitnessApp(),  &pomodoroApp(),  &clockApp(),  &game2048App(),
                      &connect4App(), &lightsOutApp(), &sokobanApp()};

}  // namespace

void setup() { shell::begin(kApps, sizeof(kApps) / sizeof(*kApps)); }

void loop() { shell::loop(); }
