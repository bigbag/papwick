#include "Connect4.h"

#include <initializer_list>

namespace connect4 {

namespace {

// Bitboard: each column uses kRows + 1 bits, bottom first. The extra bit stops carries between columns.
constexpr int kStride = kRows + 1;
constexpr int kOrder[kCols] = {3, 2, 4, 1, 5, 0, 6};
constexpr int kWin = 1000;

constexpr uint64_t bottomRow() {
  uint64_t m = 0;
  for (int c = 0; c < kCols; ++c) m |= uint64_t{1} << (c * kStride);
  return m;
}

constexpr uint64_t kBottom = bottomRow();
constexpr uint64_t kAll = kBottom * ((uint64_t{1} << kRows) - 1);

uint64_t bottomOf(int col) { return uint64_t{1} << (col * kStride); }
uint64_t topOf(int col) { return uint64_t{1} << (kRows - 1 + col * kStride); }
uint64_t columnOf(int col) { return ((uint64_t{1} << kRows) - 1) << (col * kStride); }

struct Position {
  uint64_t own = 0;   // the discs of the side to move
  uint64_t mask = 0;  // all discs
  int moves = 0;

  bool canPlay(int col) const { return (mask & topOf(col)) == 0; }
  void play(int col) {
    own ^= mask;
    mask |= mask + bottomOf(col);
    ++moves;
  }
  bool winsWith(int col) const { return aligned(own | ((mask + bottomOf(col)) & columnOf(col))); }

  static bool aligned(uint64_t p) {
    for (int shift : {1, kStride - 1, kStride, kStride + 1}) {
      const uint64_t m = p & (p >> shift);
      if (m & (m >> (2 * shift))) return true;
    }
    return false;
  }
};

// The empty cells that complete a line of four for the discs in p.
uint64_t openCells(uint64_t p, uint64_t mask) {
  uint64_t r = (p << 1) & (p << 2) & (p << 3);
  for (int s : {kStride - 1, kStride, kStride + 1}) {
    uint64_t q = (p << s) & (p << 2 * s);
    r |= q & (p << 3 * s);
    r |= q & (p >> s);
    q = (p >> s) & (p >> 2 * s);
    r |= q & (p << s);
    r |= q & (p >> 3 * s);
  }
  return r & (kAll ^ mask);
}

int popcount(uint64_t v) {
  int n = 0;
  for (; v; v &= v - 1) ++n;
  return n;
}

int negamax(const Position& p, int depth, int alpha, int beta) {
  if (p.moves == kRows * kCols) return 0;
  for (int col = 0; col < kCols; ++col) {
    if (p.canPlay(col) && p.winsWith(col)) return kWin + depth;
  }
  if (depth == 0) {
    const uint64_t other = p.own ^ p.mask;
    return popcount(openCells(p.own, p.mask)) - popcount(openCells(other, p.mask));
  }
  int best = -2 * kWin;
  for (int col : kOrder) {
    if (!p.canPlay(col)) continue;
    Position next = p;
    next.play(col);
    const int score = -negamax(next, depth - 1, -beta, -alpha);
    if (score > best) best = score;
    if (score > alpha) alpha = score;
    if (alpha >= beta) break;
  }
  return best;
}

Position toPosition(const Board& b) {
  Position p;
  for (int col = 0; col < kCols; ++col) {
    for (int row = kRows - 1; row >= 0; --row) {
      const uint8_t c = b.cells[row][col];
      if (c == kNone) break;
      const uint64_t bit = uint64_t{1} << (col * kStride + kRows - 1 - row);
      p.mask |= bit;
      if (c == b.turn) p.own |= bit;
      ++p.moves;
    }
  }
  return p;
}

uint8_t other(uint8_t side) { return side == kPlayer ? kDevice : kPlayer; }

bool full(const Board& b) {
  for (int col = 0; col < kCols; ++col) {
    if (b.cells[0][col] == kNone) return false;
  }
  return true;
}

}  // namespace

void newGame(Board& b) {
  b.first = other(b.first);
  b.turn = b.first;
  b.result = kNone;
  for (auto& row : b.cells) {
    for (auto& c : row) c = kNone;
  }
}

bool canDrop(const Board& b, int col) {
  return b.result == kNone && col >= 0 && col < kCols && b.cells[0][col] == kNone;
}

int drop(Board& b, int col) {
  if (!canDrop(b, col)) return -1;
  int row = kRows - 1;
  while (b.cells[row][col] != kNone) --row;
  b.cells[row][col] = b.turn;
  int line[4];
  if (winLine(b, line)) {
    b.result = b.turn;
    ++(b.turn == kPlayer ? b.wins : b.losses);
  } else if (full(b)) {
    b.result = kDraw;
    ++b.draws;
  }
  b.turn = other(b.turn);
  return row;
}

int bestMove(const Board& b, int depth) {
  const Position p = toPosition(b);
  int bestCol = -1, best = -3 * kWin;
  for (int col : kOrder) {
    if (!p.canPlay(col)) continue;
    if (p.winsWith(col)) return col;
    Position next = p;
    next.play(col);
    const int score = -negamax(next, depth - 1, -3 * kWin, -best);
    if (score > best) {
      best = score;
      bestCol = col;
    }
  }
  return bestCol;
}

bool winLine(const Board& b, int (&cells)[4]) {
  constexpr int kDirs[][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
  for (int row = 0; row < kRows; ++row) {
    for (int col = 0; col < kCols; ++col) {
      const uint8_t c = b.cells[row][col];
      if (c != kPlayer && c != kDevice) continue;
      for (const auto& d : kDirs) {
        const int endRow = row + 3 * d[0], endCol = col + 3 * d[1];
        if (endRow >= kRows || endCol < 0 || endCol >= kCols) continue;
        int i = 1;
        while (i < 4 && b.cells[row + i * d[0]][col + i * d[1]] == c) ++i;
        if (i < 4) continue;
        for (i = 0; i < 4; ++i) cells[i] = (row + i * d[0]) * kCols + col + i * d[1];
        return true;
      }
    }
  }
  return false;
}

bool isValid(const Board& b) {
  if (b.version != kVersion || (b.first != kPlayer && b.first != kDevice) || b.result > kDraw) return false;
  int count[3] = {};
  for (int col = 0; col < kCols; ++col) {
    bool empty = false;
    for (int row = kRows - 1; row >= 0; --row) {
      const uint8_t c = b.cells[row][col];
      if (c > kDevice || (empty && c != kNone)) return false;
      empty = c == kNone;
      ++count[c];
    }
  }
  const int started = count[b.first], second = count[other(b.first)];
  if (started != second && started != second + 1) return false;
  if (b.turn != (started == second ? b.first : other(b.first))) return false;
  int line[4];
  if (b.result == kNone) return !winLine(b, line) && !full(b);
  if (b.result == kDraw) return full(b) && !winLine(b, line);
  return winLine(b, line) && b.cells[line[0] / kCols][line[0] % kCols] == b.result;
}

}  // namespace connect4
