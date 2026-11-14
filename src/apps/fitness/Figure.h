#pragma once

#include <M5Unified.h>

namespace figure {

constexpr int kW = 180;
constexpr int kH = 150;

// Draws the stick figure of the exercise at time ms. x and y give the top-left corner of the kW x kH box.
void draw(LovyanGFX& g, int x, int y, uint8_t exercise, uint32_t ms);

}  // namespace figure
