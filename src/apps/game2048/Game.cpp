#include "Game.h"

#include <BoardSupport.h>
#include <Game2048.h>
#include <Logging.h>
#include <Preferences.h>
#include <esp_random.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr const char* TAG = "2048";
constexpr int kCell = 62, kGap = 8;
constexpr int kBoard = g2048::kSize * kCell + (g2048::kSize + 1) * kGap;
constexpr int kTop = 95;

constexpr const char* kOnOff[] = {"Off", "On"};

struct GameSettings {
  uint8_t version = 1;
  bool sound = true;
  bool vibration = true;
};

uint16_t tileColor(uint8_t exponent) {
  static const uint16_t colors[] = {
      lgfx::color565(60, 58, 50),    lgfx::color565(238, 228, 218), lgfx::color565(237, 224, 200),
      lgfx::color565(242, 177, 121), lgfx::color565(245, 149, 99),  lgfx::color565(246, 124, 95),
      lgfx::color565(246, 94, 59),   lgfx::color565(237, 207, 114), lgfx::color565(237, 204, 97),
      lgfx::color565(237, 200, 80),  lgfx::color565(237, 197, 63),  lgfx::color565(237, 194, 46)};
  constexpr int n = sizeof(colors) / sizeof(*colors);
  return exponent < n ? colors[exponent] : lgfx::color565(60, 58, 50);
}

class Game : public App {
 public:
  const AppInfo& info() const override {
    static const AppInfo i = {"g2048", "2048", lgfx::color565(237, 224, 200), "2048"};
    return i;
  }

  void boot() override {
    if (!prefs_.begin(info().id, false)) LOG_ERR(TAG, "NVS open failed");
    GameSettings s;
    if (prefs_.isKey("settings") && prefs_.getBytes("settings", &s, sizeof(s)) == sizeof(s) &&
        s.version == GameSettings{}.version)
      settings_ = s;
    g2048::Board b;
    if (prefs_.isKey("board") && prefs_.getBytes("board", &b, sizeof(b)) == sizeof(b) && g2048::isValid(b)) {
      board_ = b;
    } else {
      newGame();
    }
    LOG_INF(TAG, "Score %lu, best %lu", static_cast<unsigned long>(board_.score),
            static_cast<unsigned long>(board_.best));
  }

  void start() override { askNew_ = false; }

  void stop() override {
    if (prefs_.putBytes("board", &board_, sizeof(board_)) != sizeof(board_)) LOG_ERR(TAG, "Save board failed");
  }

  void update(const ui::Input& in) override {
    const bool over = !g2048::canMove(board_);
    if (askNew_) {
      if (in.green) newGame();
      if (in.green || in.blue) {
        askNew_ = false;
        dirty = true;
      }
      return;
    }
    if (in.blue) {
      shell::home();
      return;
    }
    if (in.green) {
      if (over) {
        newGame();
      } else {
        askNew_ = true;
      }
      dirty = true;
      return;
    }
    if (in.swipeLeft) move(g2048::Dir::Left);
    if (in.swipeRight) move(g2048::Dir::Right);
    if (in.swipeUp) move(g2048::Dir::Up);
    if (in.swipeDown) move(g2048::Dir::Down);
  }

  void draw() override {
    if (askNew_) {
      ui::confirmDraw("New game?", "new game");
      return;
    }
    ui::clear();
    char line[40];
    snprintf(line, sizeof(line), "%lu   best %lu", static_cast<unsigned long>(board_.score),
             static_cast<unsigned long>(board_.best));
    ui::text(line, ui::cx(), 62, &fonts::DejaVu18);
    auto& c = ui::canvas();
    const int x0 = ui::cx() - kBoard / 2;
    c.fillRoundRect(x0, kTop, kBoard, kBoard, 10, lgfx::color565(120, 110, 100));
    for (int r = 0; r < g2048::kSize; ++r) {
      for (int col = 0; col < g2048::kSize; ++col) {
        const uint8_t e = board_.cells[r][col];
        const int x = x0 + kGap + col * (kCell + kGap), y = kTop + kGap + r * (kCell + kGap);
        const uint16_t bg = tileColor(e);
        c.fillRoundRect(x, y, kCell, kCell, 6, bg);
        if (e == 0) continue;
        snprintf(line, sizeof(line), "%lu", static_cast<unsigned long>(g2048::tileValue(e)));
        const size_t digits = strlen(line);
        const lgfx::IFont* font = digits <= 2 ? &fonts::DejaVu24 : (digits == 3 ? &fonts::DejaVu18 : &fonts::DejaVu12);
        // White text has too little contrast on the light palette tiles. Only tiles past 2048 are dark.
        const uint16_t dark = tileColor(0);
        ui::text(line, x + kCell / 2, y + kCell / 2, font, bg == dark ? TFT_WHITE : dark);
      }
    }
    if (!g2048::canMove(board_)) {
      ui::text("Game over", ui::cx(), kTop + kBoard / 2, &fonts::DejaVu40, ui::kRed);
    } else if (winBanner_) {
      ui::text("2048!", ui::cx(), kTop + kBoard / 2, &fonts::DejaVu56, ui::kAmber);
    }
    ui::hints(410, "back", "new game");
  }

  ui::Row* settings(int& count) override {
    static ui::Row rows[] = {{"Sound", kOnOff, 2, reinterpret_cast<uint8_t*>(&settings_.sound)},
                             {"Vibration", kOnOff, 2, reinterpret_cast<uint8_t*>(&settings_.vibration)}};
    count = sizeof(rows) / sizeof(*rows);
    return rows;
  }

  void settingsChanged() override {
    if (prefs_.putBytes("settings", &settings_, sizeof(settings_)) != sizeof(settings_)) {
      LOG_ERR(TAG, "Save settings failed");
    }
  }

  void clearData() override {
    App::clearData();
    settings_ = GameSettings{};
    board_ = g2048::Board{};
    newGame();
  }

 private:
  void newGame() {
    board_.seed = esp_random() | 1;
    g2048::newGame(board_);
    winBanner_ = false;
    LOG_INF(TAG, "New game");
  }

  void move(g2048::Dir d) {
    const g2048::MoveResult r = g2048::move(board_, d);
    if (!r.moved) return;
    winBanner_ = r.reachedWin;
    if (r.merged && settings_.vibration) board::vibrate(25);
    if (r.reachedWin && settings_.sound) {
      board::beep(880, 120);
      board::beep(1320, 300);
    }
    if (!g2048::canMove(board_)) {
      LOG_INF(TAG, "Game over, score %lu", static_cast<unsigned long>(board_.score));
      if (settings_.sound) board::beep(330, 400);
    }
    dirty = true;
  }

  Preferences prefs_;
  GameSettings settings_;
  g2048::Board board_;
  bool askNew_ = false, winBanner_ = false;
};

}  // namespace

App& game2048App() {
  static Game a;
  return a;
}
