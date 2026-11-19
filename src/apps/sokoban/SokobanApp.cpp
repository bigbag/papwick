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
    shell::load(prefs_, "parked", parked_);
  }

  void start() override {
    askRestart_ = false;
    openMenu();
  }

  void stop() override { shell::save(prefs_, "game", game_); }

  void update(const ui::Input& in) override {
    if (menu_) {
      updateMenu(in);
      return;
    }
    if (askRestart_) {
      if (in.green) startLevel(game_.level);
      if (in.green || in.blue) {
        askRestart_ = false;
        dirty = true;
      }
      return;
    }
    if (in.blue) {
      openMenu();
      return;
    }
    const bool won = sokoban::isWon(game_);
    if (in.green) {
      if (won) {
        openLevel((game_.level + 1) % sokoban::kLevelCount);
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
    // The menu shows the board that green opens.
    const sokoban::Game shown = menu_ ? gameFor(pick_) : game_;
    ui::clear();
    char line[48];
    snprintf(line, sizeof(line), tr(FMT_LEVEL), shown.level + 1);
    ui::text(line, ui::cx(), 50, &fonts::DejaVu18);
    snprintf(line, sizeof(line), tr(FMT_MOVES_PUSHES), static_cast<unsigned long>(shown.moves),
             static_cast<unsigned long>(shown.pushes));
    ui::text(line, ui::cx(), 80, &fonts::DejaVu12, ui::kGrey);
    drawBoard(shown);
    const bool won = sokoban::isWon(shown);
    if (menu_) {
      ui::hints(410, tr(BACK), shown.moves > 0 ? tr(RESUME) : tr(START));
      ui::pageDots(pick_, sokoban::kLevelCount);
    } else {
      if (won) {
        ui::banner(tr(SOLVED), kAreaY, ui::kAmber);
      } else {
        ui::text(tr(HOLD_TO_RESTART), ui::cx(), 388, &fonts::DejaVu18, ui::kGrey);
      }
      ui::hints(410, tr(BACK), won ? tr(NEXT_LEVEL) : tr(UNDO));
    }
  }

  void clearData() override {
    App::clearData();
    parked_ = {};
    startLevel(0);
  }

 private:
  // After a solved level, the menu selects the next level.
  void openMenu() {
    menu_ = true;
    pick_ = sokoban::isWon(game_) ? (game_.level + 1) % sokoban::kLevelCount : game_.level;
    dirty = true;
  }

  // The game that opens for a level: the unsolved game, the parked position of another level or a new game.
  sokoban::Game gameFor(uint8_t index) const {
    if (index == game_.level && !sokoban::isWon(game_)) return game_;
    sokoban::Game g;
    sokoban::resume(g, index, index == game_.level ? sokoban::Parked{} : parked_[index]);
    return g;
  }

  void openLevel(uint8_t index) {
    const sokoban::Game next = gameFor(index);
    if (index != game_.level) {
      parked_[game_.level] = sokoban::park(game_);
      shell::save(prefs_, "parked", parked_);
    }
    game_ = next;
    LOG_INF(TAG, "Level %d, %lu moves", index + 1, static_cast<unsigned long>(game_.moves));
    dirty = true;
  }

  void updateMenu(const ui::Input& in) {
    if (in.blue) {
      shell::home();
    } else if (in.green) {
      openLevel(pick_);
      menu_ = false;
      dirty = true;
    } else if (in.swipeLeft || in.swipeRight) {
      const int next = std::clamp(pick_ + (in.swipeLeft ? 1 : -1), 0, sokoban::kLevelCount - 1);
      if (next != pick_) {
        pick_ = next;
        dirty = true;
      }
    }
  }

  void startLevel(uint8_t index) {
    if (!sokoban::start(game_, index)) return;
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

  void drawBoard(const sokoban::Game& game) {
    const sokoban::Level& map = *sokoban::level(game.level);
    const int cell = std::min({kAreaW / map.width, kAreaH / map.height, kMaxCell});
    const int x0 = ui::cx() - cell * map.width / 2, y0 = kAreaY - cell * map.height / 2;
    auto& c = ui::canvas();
    for (uint8_t i = 0; i < map.width * map.height; ++i) {
      const int x = x0 + i % map.width * cell, y = y0 + i / map.width * cell;
      const char t = sokoban::terrain(game, i);
      if (t == '#') {
        c.fillRect(x + 1, y + 1, cell - 2, cell - 2, kWall);
        continue;
      }
      c.fillRect(x, y, cell, cell, kFloor);
      if (t == '.') c.fillCircle(x + cell / 2, y + cell / 2, cell / 5, kGoal);
      if (sokoban::hasBox(game, i)) {
        c.fillRoundRect(x + 3, y + 3, cell - 6, cell - 6, 4, t == '.' ? kBoxDone : kBox);
      } else if (i == game.player) {
        drawCat(x, y, cell);
      }
    }
  }

  Preferences prefs_;
  sokoban::Game game_;
  std::array<sokoban::Parked, sokoban::kLevelCount> parked_{};
  int pick_ = 0;
  bool menu_ = true, askRestart_ = false;
};

}  // namespace

App& sokobanApp() {
  static SokobanApp a;
  return a;
}
