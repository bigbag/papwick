#pragma once

#include <cstdint>

namespace lightsout {
constexpr uint8_t kSide = 5;
constexpr uint8_t kCells = 25;
constexpr uint8_t kVersion = 1;
struct Game {
  uint32_t lights = 0;
  uint32_t moves = 0;
  uint8_t version = kVersion;
};
Game generate(uint32_t toggles);
bool toggle(Game& game, uint8_t index);
bool valid(const Game& game);
bool isWon(const Game& game);
}  // namespace lightsout
