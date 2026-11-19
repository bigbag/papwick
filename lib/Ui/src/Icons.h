#pragma once

#include <cstdint>

// App pictograms for the home screen. Each one fits an 80 px square centered at x, y.
// ink draws the shape. bg is the color under the icon and cuts the holes.
// The pictograms use the same stroke width, so they look alike. Only the gear teeth are wider.
namespace icon {

using Draw = void (*)(int x, int y, uint16_t ink, uint16_t bg);

void dumbbell(int x, int y, uint16_t ink, uint16_t bg);
void hourglass(int x, int y, uint16_t ink, uint16_t bg);
void clock(int x, int y, uint16_t ink, uint16_t bg);
void tiles(int x, int y, uint16_t ink, uint16_t bg);
void discs(int x, int y, uint16_t ink, uint16_t bg);
void lights(int x, int y, uint16_t ink, uint16_t bg);
void crate(int x, int y, uint16_t ink, uint16_t bg);
void flag(int x, int y, uint16_t ink, uint16_t bg);
void radio(int x, int y, uint16_t ink, uint16_t bg);
void meter(int x, int y, uint16_t ink, uint16_t bg);
void gear(int x, int y, uint16_t ink, uint16_t bg);

}  // namespace icon
