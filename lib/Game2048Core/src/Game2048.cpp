#include "Game2048.h"

namespace g2048 {
namespace {

constexpr uint8_t kMaxExponent = 17;

uint32_t nextRandom(uint32_t& state) {
  if (state == 0) state = 1;
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

// Slides one line toward index 0. Returns the points of its merges.
uint32_t slideLine(uint8_t (&line)[kSize]) {
  uint8_t out[kSize] = {};
  uint32_t gained = 0;
  int n = 0;
  bool fresh = false;  // a merged tile cannot merge again in the same move
  for (uint8_t exponent : line) {
    if (exponent == 0) continue;
    if (n > 0 && !fresh && out[n - 1] == exponent) {
      ++out[n - 1];
      gained += tileValue(out[n - 1]);
      fresh = true;
    } else {
      out[n++] = exponent;
      fresh = false;
    }
  }
  for (int i = 0; i < kSize; ++i) line[i] = out[i];
  return gained;
}

// Returns the cell at position pos of a line. Position 0 is the cell the line slides toward.
uint8_t& cellAt(Board& b, Dir dir, int line, int pos) {
  switch (dir) {
    case Dir::Left:
      return b.cells[line][pos];
    case Dir::Right:
      return b.cells[line][kSize - 1 - pos];
    case Dir::Up:
      return b.cells[pos][line];
    default:
      return b.cells[kSize - 1 - pos][line];
  }
}

bool hasTile(const Board& b, uint8_t exponent) {
  for (int r = 0; r < kSize; ++r) {
    for (int c = 0; c < kSize; ++c) {
      if (b.cells[r][c] == exponent) return true;
    }
  }
  return false;
}

void spawnTile(Board& b) {
  uint32_t empty = 0;
  for (int r = 0; r < kSize; ++r) {
    for (int c = 0; c < kSize; ++c) empty += b.cells[r][c] == 0;
  }
  if (empty == 0) return;

  uint32_t pick = nextRandom(b.seed) % empty;
  const uint8_t exponent = nextRandom(b.seed) % 10 == 0 ? 2 : 1;
  for (int r = 0; r < kSize; ++r) {
    for (int c = 0; c < kSize; ++c) {
      if (b.cells[r][c] != 0) continue;
      if (pick == 0) {
        b.cells[r][c] = exponent;
        return;
      }
      --pick;
    }
  }
}

}  // namespace

uint32_t tileValue(uint8_t exponent) { return exponent == 0 ? 0 : 1u << exponent; }

void newGame(Board& b) {
  for (int r = 0; r < kSize; ++r) {
    for (int c = 0; c < kSize; ++c) b.cells[r][c] = 0;
  }
  b.score = 0;
  b.won = false;
  spawnTile(b);
  spawnTile(b);
}

MoveResult move(Board& b, Dir dir) {
  MoveResult result;
  for (int line = 0; line < kSize; ++line) {
    uint8_t slide[kSize];
    for (int pos = 0; pos < kSize; ++pos) slide[pos] = cellAt(b, dir, line, pos);
    const uint32_t gained = slideLine(slide);
    if (gained > 0) result.merged = true;
    result.gained += gained;
    for (int pos = 0; pos < kSize; ++pos) {
      uint8_t& cell = cellAt(b, dir, line, pos);
      if (cell != slide[pos]) result.moved = true;
      cell = slide[pos];
    }
  }
  if (!result.moved) return result;

  b.score += result.gained;
  if (b.score > b.best) b.best = b.score;
  if (!b.won && hasTile(b, kWinExponent)) {
    b.won = true;
    result.reachedWin = true;
  }
  spawnTile(b);
  return result;
}

bool canMove(const Board& b) {
  for (int r = 0; r < kSize; ++r) {
    for (int c = 0; c < kSize; ++c) {
      if (b.cells[r][c] == 0) return true;
      if (c + 1 < kSize && b.cells[r][c] == b.cells[r][c + 1]) return true;
      if (r + 1 < kSize && b.cells[r][c] == b.cells[r + 1][c]) return true;
    }
  }
  return false;
}

bool isValid(const Board& b) {
  if (b.version != kVersion || b.seed == 0) return false;
  for (int r = 0; r < kSize; ++r) {
    for (int c = 0; c < kSize; ++c) {
      if (b.cells[r][c] > kMaxExponent) return false;
    }
  }
  return true;
}

}  // namespace g2048
