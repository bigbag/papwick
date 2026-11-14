#pragma once

namespace board {

// The input events of one loop. The touch events carry their point in x and y.
struct Input {
  bool tap = false;
  // The touch passed the hold time. Comes once per touch.
  bool hold = false;
  // The touch stays held after the hold time. Comes every 110 ms.
  bool repeat = false;
  int x = 0, y = 0;
  bool swipeLeft = false, swipeRight = false, swipeUp = false, swipeDown = false;
  // The swipe length in px along its main axis.
  int distance = 0;
  // The blue button is on the left (BtnA, GPIO2). The green button is on the right (BtnB, GPIO1).
  bool blue = false, green = false;
  // The red power button. A series of more than two clicks counts as a double click.
  bool red = false, redDouble = false;
  bool any() const;
};

// Sets up the red button in the M5PM1. board::begin() calls it.
void beginInput();
// Reads the touch and the buttons. Call it once in each loop after M5.update().
Input poll();

}  // namespace board
