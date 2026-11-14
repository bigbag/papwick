#include "Game2048.h"
#include "test_utils.h"

namespace {

using namespace g2048;

Board withRows(const uint8_t (&rows)[kSize][kSize]) {
  Board b;
  for (int r = 0; r < kSize; ++r) {
    for (int c = 0; c < kSize; ++c) b.cells[r][c] = rows[r][c];
  }
  return b;
}

int countTiles(const Board& b) {
  int n = 0;
  for (int r = 0; r < kSize; ++r) {
    for (int c = 0; c < kSize; ++c) n += b.cells[r][c] != 0;
  }
  return n;
}

int countExponent(const Board& b, uint8_t exponent) {
  int n = 0;
  for (int r = 0; r < kSize; ++r) {
    for (int c = 0; c < kSize; ++c) n += b.cells[r][c] == exponent;
  }
  return n;
}

void testSlides(TestUtils::TestRunner& r) {
  Board row = withRows({{1, 1, 1, 1}});
  MoveResult m = move(row, Dir::Left);
  r.expectTrue(m.moved && m.merged && row.cells[0][0] == 2 && row.cells[0][1] == 2 && countTiles(row) == 3,
               "[2,2,2,2] Left gives [4,4] plus one spawned tile");

  Board mixed = withRows({{1, 1, 2, 0}});
  m = move(mixed, Dir::Left);
  r.expectTrue(mixed.cells[0][0] == 2 && mixed.cells[0][1] == 2 && m.gained == 4,
               "[2,2,4,0] Left merges once per tile");

  Board edges = withRows({{2, 0, 0, 2}});
  m = move(edges, Dir::Right);
  r.expectTrue(edges.cells[0][3] == 3 && countExponent(edges, 3) == 1 && m.gained == 8,
               "[4,0,0,4] Right gives one 8 at the right");

  Board up = withRows({{0, 1, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}});
  m = move(up, Dir::Up);
  r.expectTrue(up.cells[0][1] == 2 && m.gained == 4, "column merges Up");

  Board down = withRows({{0, 0, 0, 0}, {0, 2, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}});
  m = move(down, Dir::Down);
  r.expectTrue(down.cells[3][1] == 1 && down.cells[2][1] == 2 && m.gained == 0, "column slides Down without merge");

  Board still = withRows({{1, 2, 0, 0}});
  still.score = 7;
  m = move(still, Dir::Left);
  r.expectTrue(!m.moved && !m.merged && countTiles(still) == 2 && still.score == 7,
               "a move that changes nothing does not spawn");
}

void testScore(TestUtils::TestRunner& r) {
  Board b = withRows({{1, 1, 0, 0}});
  move(b, Dir::Left);
  r.expectTrue(b.score == 4 && b.best == 4, "score and best take the merged value");

  Board c = withRows({{2, 2, 0, 0}});
  c.score = 4;
  c.best = 4;
  move(c, Dir::Left);
  r.expectTrue(c.score == 12 && c.best == 12, "score adds up and best follows it");
}

void testWin(TestUtils::TestRunner& r) {
  Board b = withRows({{10, 10, 0, 0}});
  MoveResult m = move(b, Dir::Left);
  r.expectTrue(m.reachedWin && b.won && m.gained == 2048, "1024 + 1024 makes the first 2048");

  Board again = withRows({{10, 10, 0, 0}});
  again.won = true;
  m = move(again, Dir::Left);
  r.expectTrue(!m.reachedWin && again.won, "reachedWin is true once and won stays true");
}

void testNewGame(TestUtils::TestRunner& r) {
  Board b;
  b.best = 500;
  b.score = 99;
  b.won = true;
  b.cells[0][0] = 5;
  newGame(b);
  r.expectTrue(countTiles(b) == 2 && b.score == 0 && !b.won && b.best == 500,
               "new game clears the board and keeps best");

  bool spawnOk = true;
  for (uint32_t seed = 1; seed <= 200; ++seed) {
    Board fresh;
    fresh.seed = seed;
    newGame(fresh);
    spawnOk = spawnOk && countTiles(fresh) == 2;
    for (int rr = 0; rr < kSize; ++rr) {
      for (int c = 0; c < kSize; ++c) spawnOk = spawnOk && fresh.cells[rr][c] <= 2;
    }

    Board single = withRows({{1, 0, 0, 0}});
    single.seed = seed;
    move(single, Dir::Right);
    spawnOk = spawnOk && countTiles(single) == 2;
    for (int rr = 0; rr < kSize; ++rr) {
      for (int c = 0; c < kSize; ++c) spawnOk = spawnOk && single.cells[rr][c] <= 2;
    }
  }
  r.expectTrue(spawnOk, "each spawn adds one tile of 2 or 4 over many seeds");
}

void testCanMove(TestUtils::TestRunner& r) {
  Board full = withRows({{1, 2, 1, 2}, {2, 1, 2, 1}, {1, 2, 1, 2}, {2, 1, 2, 1}});
  r.expectTrue(!canMove(full), "full board without equal neighbours cannot move");
  full.cells[0][0] = 2;
  r.expectTrue(canMove(full), "two equal neighbours can move");
  r.expectTrue(tileValue(0) == 0 && tileValue(1) == 2 && tileValue(kWinExponent) == 2048, "tile values");
}

void testValid(TestUtils::TestRunner& r) {
  Board b;
  r.expectTrue(isValid(b), "default board is valid");
  b.version = kVersion - 1;
  r.expectTrue(!isValid(b), "a wrong version is invalid");
  b = Board{};
  b.cells[2][3] = 18;
  r.expectTrue(!isValid(b), "a cell exponent of 18 is invalid");
  b = Board{};
  b.seed = 0;
  r.expectTrue(!isValid(b), "a zero seed is invalid");
}

}  // namespace

int main() {
  TestUtils::TestRunner runner("Game2048");
  testSlides(runner);
  testScore(runner);
  testWin(runner);
  testNewGame(runner);
  testCanMove(runner);
  testValid(runner);
  return runner.exitCode();
}
