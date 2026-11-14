#pragma once

#include <cstdint>

namespace connect4 {

constexpr int kCols = 7;
constexpr int kRows = 6;
constexpr uint8_t kVersion = 1;

// The owner of a cell, the side to move and the game result.
constexpr uint8_t kNone = 0, kPlayer = 1, kDevice = 2, kDraw = 3;

struct Board {
  uint8_t version = kVersion;
  uint8_t cells[kRows][kCols] = {};  // [row][col], row 0 is the top
  uint8_t turn = kPlayer;
  uint8_t first = kPlayer;  // the side that started this game
  uint8_t result = kNone;
  uint16_t wins = 0, losses = 0, draws = 0;
};

// Clears the cells and keeps the totals. The side that did not start the last game starts.
void newGame(Board& b);

// Drops a disc of the side to move into col. Returns the row of the disc, or -1 when the column is full or the game is
// over. Sets the result and the totals at the end of the game.
int drop(Board& b, int col);

bool canDrop(const Board& b, int col);

// Returns the best column for the side to move. depth is the number of half moves to look ahead, 1 or more.
// The search takes a win at once and blocks a win of the other side.
int bestMove(const Board& b, int depth);

// Writes the four cells (row * kCols + col) of a winning line. Returns false when no line exists.
bool winLine(const Board& b, int (&cells)[4]);

// True when the version, the cell values, the gravity and the disc counts are correct.
bool isValid(const Board& b);

}  // namespace connect4
