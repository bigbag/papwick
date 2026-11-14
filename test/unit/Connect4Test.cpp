#include "Connect4.h"
#include "test_utils.h"

namespace {

using namespace connect4;

// Plays the columns in turn, from the side to move.
Board play(const char* cols) {
  Board b;
  for (const char* c = cols; *c; ++c) drop(b, *c - '0');
  return b;
}

void testDrop(TestUtils::TestRunner& r) {
  Board b;
  r.expectEq(kRows - 1, drop(b, 3), "the first disc lands on the bottom row");
  r.expectEq(kRows - 2, drop(b, 3), "the second disc lands on the first");
  r.expectEq(kPlayer, b.turn, "the turn alternates");
  r.expectEq(-1, drop(b, 7), "a column out of range is refused");
  b = play("000000");
  r.expectFalse(canDrop(b, 0), "a full column is refused");
  r.expectEq(-1, drop(b, 0), "a drop into a full column is refused");
}

void testWins(TestUtils::TestRunner& r) {
  Board b = play("0102030");
  r.expectEq(kPlayer, b.result, "four in a column wins");
  r.expectEq(1, static_cast<int>(b.wins), "a win counts for the player");
  r.expectEq(-1, drop(b, 4), "no drop after the end");

  b = play("0011223");
  r.expectEq(kPlayer, b.result, "four in a row wins");
  int line[4];
  r.expectTrue(winLine(b, line) && line[0] == 5 * kCols && line[3] == 5 * kCols + 3, "the winning line is the row");

  b = play("01123223633");
  r.expectEq(kPlayer, b.result, "a rising diagonal wins");
  b = play("06554344303");
  r.expectEq(kNone, b.result, "no line yet");
  drop(b, 3);
  r.expectEq(kDevice, b.result, "a falling diagonal wins for the second side");
  r.expectEq(1, static_cast<int>(b.losses), "a device win counts as a loss");
}

void testDraw(TestUtils::TestRunner& r) {
  Board b = play("43601455115016015510463266046520424222333");
  r.expectEq(kNone, b.result, "41 discs and no line");
  r.expectTrue(isValid(b), "a played board is valid");
  drop(b, 3);
  r.expectEq(kDraw, b.result, "a full board without a line is a draw");
  r.expectEq(1, static_cast<int>(b.draws), "a draw counts");
  r.expectTrue(isValid(b), "a drawn board is valid");
}

void testNewGame(TestUtils::TestRunner& r) {
  Board b = play("0102030");
  newGame(b);
  r.expectEq(kDevice, b.first, "the other side starts the next game");
  r.expectEq(kDevice, b.turn, "the device moves first");
  r.expectEq(kNone, b.result, "the result is clear");
  r.expectEq(1, static_cast<int>(b.wins), "the totals stay");
  r.expectTrue(canDrop(b, 0) && isValid(b), "the board is empty and valid");
}

void testSearch(TestUtils::TestRunner& r) {
  Board b = play("0606061");
  r.expectEq(6, bestMove(b, 1), "the search takes a win before it blocks");
  b = play("06162");
  r.expectEq(3, bestMove(b, 1), "the search blocks three in a row");
  r.expectEq(3, bestMove(b, 7), "a deep search also blocks");
  for (int depth : {1, 3, 7}) {
    Board e;
    const int col = bestMove(e, depth);
    r.expectTrue(col >= 0 && col < kCols, "the search finds a move on an empty board");
  }
  b = play("33333322222");
  r.expectTrue(canDrop(b, bestMove(b, 7)), "the search skips full columns");
}

void testValid(TestUtils::TestRunner& r) {
  r.expectTrue(isValid(Board{}), "an empty board is valid");
  Board b;
  b.cells[0][0] = kPlayer;
  r.expectFalse(isValid(b), "a floating disc is invalid");
  b = Board{};
  b.cells[kRows - 1][0] = kDevice;
  r.expectFalse(isValid(b), "a wrong disc count is invalid");
  b = Board{};
  b.version = 0;
  r.expectFalse(isValid(b), "a wrong version is invalid");
  b = play("0102030");
  b.result = kNone;
  r.expectFalse(isValid(b), "a line without a result is invalid");
  b.result = kDevice;
  r.expectFalse(isValid(b), "a win for the side without the line is invalid");
}

}  // namespace

int main() {
  TestUtils::TestRunner runner("Connect4");
  testDrop(runner);
  testWins(runner);
  testDraw(runner);
  testNewGame(runner);
  testSearch(runner);
  testValid(runner);
  return runner.exitCode();
}
