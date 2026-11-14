#include "apps/fitness/App.h"
#include "apps/game2048/Game.h"
#include "shell/Shell.h"

namespace {

App* const kApps[] = {&fitnessApp(), &game2048App()};

}  // namespace

void setup() { shell::begin(kApps, sizeof(kApps) / sizeof(*kApps)); }

void loop() { shell::loop(); }
