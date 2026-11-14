#include "Sound.h"

#include <BoardSupport.h>

namespace sound {

using board::beep;
using board::vibrate;

void cue(fit::Cue c) {
  switch (c) {
    case fit::Cue::GetReady:
      beep(880, 80);
      beep(880, 80);
      vibrate(80);
      break;
    case fit::Cue::Go:
      beep(1320, 400);
      vibrate(200);
      break;
    case fit::Cue::Rest:
      beep(880, 150);
      beep(660, 250);
      vibrate(120, 100, 2);
      break;
    case fit::Cue::Halfway:
      beep(1047, 70, 70);
      beep(1047, 70);
      vibrate(80, 80, 2);
      break;
    case fit::Cue::Three:
    case fit::Cue::Two:
      beep(880, 100);
      break;
    case fit::Cue::One:
      beep(880, 100);
      vibrate(60);
      break;
    case fit::Cue::Done:
      beep(660, 150);
      beep(880, 150);
      beep(1100, 150);
      beep(1320, 500);
      vibrate(400, 150, 3);
      break;
  }
}

}  // namespace sound
