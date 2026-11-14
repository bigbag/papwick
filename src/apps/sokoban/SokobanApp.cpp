#include <BoardSupport.h>
#include <I18n.h>
#include <Logging.h>
#include <Sokoban.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <initializer_list>

#include "App.h"

namespace {

constexpr const char* TAG = "SOKOBAN";
constexpr int kAreaW = 300, kAreaH = 250, kAreaY = 245, kMaxCell = 40;
constexpr uint16_t kWall = lgfx::color565(95, 88, 80);
constexpr uint16_t kFloor = lgfx::color565(32, 32, 36);
constexpr uint16_t kGoal = lgfx::color565(200, 170, 90);
constexpr uint16_t kBox = lgfx::color565(180, 140, 95);
constexpr uint16_t kBoxDone = lgfx::color565(235, 205, 120);
constexpr uint16_t kFur = lgfx::color565(232, 232, 238);
constexpr uint16_t kInnerEar = lgfx::color565(245, 160, 175);
constexpr uint16_t kEye = lgfx::color565(30, 30, 36);
constexpr uint16_t kNose = lgfx::color565(240, 130, 150);
constexpr uint16_t kWhisker = lgfx::color565(140, 140, 150);
constexpr const char* kLevelNames[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"};
static_assert(sizeof(kLevelNames) / sizeof(*kLevelNames) == sokoban::kLevelCount, "one name for each level");

// The worker is a cat face. The parts scale with the cell. Small cells get no inner ears and no whiskers.
void drawCat(int x, int y, int cell) {
  auto& c = ui::canvas();
  const float cx = x + cell / 2.0f, cy = y + cell / 2.0f, h = cell / 2.0f - 2;
  auto px = [&](float k) { return static_cast<int>(lroundf(cx + k * h)); };
  auto py = [&](float k) { return static_cast<int>(lroundf(cy + k * h)); };
  auto len = [&](float k, int least) { return std::max(least, static_cast<int>(lroundf(k * h))); };
  const bool large = h >= 10;
  for (int s : {-1, 1}) {
    c.fillTriangle(px(s * 0.85f), py(0.05f), px(s * 0.7f), py(-0.95f), px(s * 0.1f), py(-0.4f), kFur);
    if (large) c.fillTriangle(px(s * 0.7f), py(-0.1f), px(s * 0.64f), py(-0.7f), px(s * 0.3f), py(-0.38f), kInnerEar);
  }
  c.fillEllipse(px(0), py(0.225f), len(0.88f, 1), len(0.675f, 1), kFur);
  for (int s : {-1, 1}) c.fillEllipse(px(s * 0.36f), py(0.12f), len(0.11f, 1), len(0.17f, 2), kEye);
  c.fillTriangle(px(-0.13f), py(0.36f), px(0.13f), py(0.36f), px(0), py(0.5f), kNose);
  if (!large) return;
  for (int s : {-1, 1}) {
    c.drawLine(px(s * 0.28f), py(0.5f), px(s * 1.0f), py(0.38f), kWhisker);
    c.drawLine(px(s * 0.28f), py(0.55f), px(s * 1.0f), py(0.7f), kWhisker);
  }
}

class SokobanApp : public App {
 public:
  const AppInfo& info() const override {
    static const AppInfo i = {"sokoban", tr(SOKOBAN), lgfx::color565(190, 160, 125), icon::crate};
    return i;
  }

  void boot() override {
    if (!prefs_.begin(info().id, false)) LOG_ERR(TAG, "NVS open failed");
    if (!shell::load(prefs_, "game", game_) || !sokoban::valid(game_)) sokoban::start(game_, 0);
    level_ = game_.level;
  }

  void start() override { askRestart_ = false; }

  void stop() override { shell::save(prefs_, "game", game_); }

  void update(const ui::Input& in) override {
    if (askRestart_) {
      if (in.green) startLevel(game_.level);
      if (in.green || in.blue) {
        askRestart_ = false;
        dirty = true;
      }
      return;
    }
    if (in.blue) {
      shell::home();
      return;
    }
    const bool won = sokoban::isWon(game_);
    if (in.green) {
      if (won) {
        startLevel((game_.level + 1) % sokoban::kLevelCount);
      } else if (sokoban::undo(game_)) {
        dirty = true;
      }
      return;
    }
    if (in.hold && !won) {
      askRestart_ = true;
      dirty = true;
      return;
    }
    if (in.swipeLeft) move(sokoban::Direction::Left);
    if (in.swipeRight) move(sokoban::Direction::Right);
    if (in.swipeUp) move(sokoban::Direction::Up);
    if (in.swipeDown) move(sokoban::Direction::Down);
  }

  void draw() override {
    if (askRestart_) {
      ui::confirmDraw(tr(RESTART_LEVEL_Q), tr(YES));
      return;
    }
    ui::clear();
    char line[48];
    snprintf(line, sizeof(line), tr(FMT_LEVEL), game_.level + 1);
    ui::text(line, ui::cx(), 50, &fonts::DejaVu18);
    snprintf(line, sizeof(line), tr(FMT_MOVES_PUSHES), static_cast<unsigned long>(game_.moves),
             static_cast<unsigned long>(game_.pushes));
    ui::text(line, ui::cx(), 80, &fonts::DejaVu12, ui::kGrey);
    drawBoard();
    const bool won = sokoban::isWon(game_);
    if (won) ui::banner(tr(SOLVED), kAreaY, ui::kAmber);
    ui::hints(410, tr(BACK), won ? tr(NEXT_LEVEL) : tr(UNDO));
  }

  ui::Row* settings(int& count) override {
    static ui::Row rows[] = {{tr(LEVEL), kLevelNames, sokoban::kLevelCount, &level_}};
    level_ = game_.level;
    count = 1;
    return rows;
  }

  void settingsChanged() override {
    if (level_ != game_.level) startLevel(level_);
    shell::save(prefs_, "game", game_);
  }

  void clearData() override {
    App::clearData();
    startLevel(0);
  }

 private:
  void startLevel(uint8_t index) {
    if (!sokoban::start(game_, index)) return;
    level_ = index;
    LOG_INF(TAG, "Level %d", index + 1);
    dirty = true;
  }

  void move(sokoban::Direction d) {
    if (!sokoban::move(game_, d)) return;
    if (sokoban::isWon(game_)) {
      LOG_INF(TAG, "Level %d solved in %lu moves", game_.level + 1, static_cast<unsigned long>(game_.moves));
      board::beep(880, 120);
      board::beep(1320, 300);
      board::vibrate(80);
    }
    dirty = true;
  }

  void drawBoard() {
    const sokoban::Level& map = *sokoban::level(game_.level);
    const int cell = std::min({kAreaW / map.width, kAreaH / map.height, kMaxCell});
    const int x0 = ui::cx() - cell * map.width / 2, y0 = kAreaY - cell * map.height / 2;
    auto& c = ui::canvas();
    for (uint8_t i = 0; i < map.width * map.height; ++i) {
      const int x = x0 + i % map.width * cell, y = y0 + i / map.width * cell;
      const char t = sokoban::terrain(game_, i);
      if (t == '#') {
        c.fillRect(x + 1, y + 1, cell - 2, cell - 2, kWall);
        continue;
      }
      c.fillRect(x, y, cell, cell, kFloor);
      if (t == '.') c.fillCircle(x + cell / 2, y + cell / 2, cell / 5, kGoal);
      if (sokoban::hasBox(game_, i)) {
        c.fillRoundRect(x + 3, y + 3, cell - 6, cell - 6, 4, t == '.' ? kBoxDone : kBox);
      } else if (i == game_.player) {
        drawCat(x, y, cell);
      }
    }
  }

  Preferences prefs_;
  sokoban::Game game_;
  uint8_t level_ = 0;
  bool askRestart_ = false;
};

}  // namespace

App& sokobanApp() {
  static SokobanApp a;
  return a;
}
