#pragma once

#include <cstdint>

namespace g2048 {

constexpr int kSize = 4;
constexpr uint8_t kVersion = 1;
constexpr uint8_t kWinExponent = 11;  // 2^11 = 2048

enum class Dir : uint8_t { Left, Right, Up, Down };

// Each cell holds an exponent: 0 is empty, 1 is the tile 2, 2 is the tile 4, 11 is the tile 2048.
struct Board {
  uint8_t version = kVersion;
  uint8_t cells[kSize][kSize] = {};  // [row][col], row 0 is the top
  uint32_t score = 0;
  uint32_t best = 0;
  uint32_t seed = 1;  // xorshift32 state; the caller seeds it, the tests fix it
  bool won = false;   // true after the first 2048 tile; play continues
};

struct MoveResult {
  bool moved = false;       // at least one tile moved or merged
  bool merged = false;      // at least one merge
  bool reachedWin = false;  // this move made the first 2048 tile
  uint32_t gained = 0;      // points of this move (sum of the merged tile values)
};

uint32_t tileValue(uint8_t exponent);  // 0 -> 0, otherwise 1u << exponent

// Clears the cells, the score and won. Keeps best and seed. Spawns two tiles.
void newGame(Board& b);

// Slides and merges toward dir. A tile merges at most once per move. Adds gained to score, updates best.
// Spawns one tile in a random empty cell only when the move changed the board: a 2 with 90 % chance, else a 4.
MoveResult move(Board& b, Dir dir);

// True when an empty cell exists or two equal neighbours exist.
bool canMove(const Board& b);

// True when version == kVersion, each cell <= 17 and seed != 0. Use it to check a board loaded from flash.
bool isValid(const Board& b);

}  // namespace g2048
