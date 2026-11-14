#include "Game.h"

#include <BoardSupport.h>
#include <Game2048.h>
#include <I18n.h>
#include <Logging.h>
#include <Preferences.h>
#include <esp_random.h>

#include <cstddef>
#include <cstdio>
#include <cstring>

namespace {

constexpr const char* TAG = "2048";
constexpr int kCell = 62, kGap = 8;
constexpr int kBoard = g2048::kSize * kCell + (g2048::kSize + 1) * kGap;
constexpr int kTop = 95;

constexpr uint8_t kThemeCount = 2;

struct GameSettings {
  uint8_t version = 1;
  bool sound = true;
  bool vibration = true;
  uint8_t theme = 0;
};

struct TileStyle {
  uint16_t bg, fg;
};

// Each value has its own hue. "Soft" uses dark text, with a contrast of 6:1 or more.
TileStyle tileStyle(uint8_t exponent, uint8_t theme) {
  const uint16_t dark = lgfx::color565(60, 58, 50), text = lgfx::color565(40, 38, 34);
  constexpr int n = 12;
  static const TileStyle styles[kThemeCount][n] = {{{dark, TFT_WHITE},
                                                    {lgfx::color565(234, 227, 215), text},  // 2
                                                    {lgfx::color565(230, 224, 179), text},  // 4
                                                    {lgfx::color565(228, 172, 139), text},  // 8
                                                    {lgfx::color565(213, 151, 144), text},  // 16
                                                    {lgfx::color565(209, 159, 179), text},  // 32
                                                    {lgfx::color565(195, 169, 209), text},  // 64
                                                    {lgfx::color565(155, 176, 212), text},  // 128
                                                    {lgfx::color565(127, 184, 189), text},  // 256
                                                    {lgfx::color565(140, 186, 148), text},  // 512
                                                    {lgfx::color565(174, 189, 127), text},  // 1024
                                                    {lgfx::color565(219, 191, 87), text}},  // 2048
                                                   {{dark, TFT_WHITE},
                                                    {lgfx::color565(238, 228, 218), dark},      // 2
                                                    {lgfx::color565(237, 207, 140), dark},      // 4
                                                    {lgfx::color565(242, 160, 90), dark},       // 8
                                                    {lgfx::color565(210, 70, 60), TFT_WHITE},   // 16
                                                    {lgfx::color565(200, 50, 110), TFT_WHITE},  // 32
                                                    {lgfx::color565(150, 60, 190), TFT_WHITE},  // 64
                                                    {lgfx::color565(70, 90, 200), TFT_WHITE},   // 128
                                                    {lgfx::color565(20, 130, 150), TFT_WHITE},  // 256
                                                    {lgfx::color565(30, 120, 60), TFT_WHITE},   // 512
                                                    {lgfx::color565(140, 180, 40), dark},       // 1024
                                                    {lgfx::color565(245, 200, 40), dark}}};     // 2048
  const TileStyle* t = styles[theme < kThemeCount ? theme : 0];
  return exponent < n ? t[exponent] : t[0];
}

class Game : public App {
 public:
  const AppInfo& info() const override {
    static const AppInfo i = {"g2048", "2048", lgfx::color565(237, 224, 200), icon::tiles};
    return i;
  }

  void boot() override {
    if (!prefs_.begin(info().id, false)) LOG_ERR(TAG, "NVS open failed");
    GameSettings s;
    // An older firmware saved the settings without the theme. The theme then keeps its default.
    if (prefs_.isKey("settings") && prefs_.getBytes("settings", &s, sizeof(s)) >= offsetof(GameSettings, theme) &&
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
      ui::confirmDraw(tr(NEW_GAME_Q), tr(NEW_GAME));
      return;
    }
    ui::clear();
    char line[40];
    snprintf(line, sizeof(line), tr(FMT_SCORE), static_cast<unsigned long>(board_.score),
             static_cast<unsigned long>(board_.best));
    ui::text(line, ui::cx(), 62, &fonts::DejaVu18);
    auto& c = ui::canvas();
    const int x0 = ui::cx() - kBoard / 2;
    for (int r = 0; r < g2048::kSize; ++r) {
      for (int col = 0; col < g2048::kSize; ++col) {
        const uint8_t e = board_.cells[r][col];
        const int x = x0 + kGap + col * (kCell + kGap), y = kTop + kGap + r * (kCell + kGap);
        const TileStyle style = tileStyle(e, settings_.theme);
        c.fillRoundRect(x, y, kCell, kCell, 6, style.bg);
        if (e == 0) continue;
        snprintf(line, sizeof(line), "%lu", static_cast<unsigned long>(g2048::tileValue(e)));
        const size_t digits = strlen(line);
        const lgfx::IFont* font = digits <= 2 ? &fonts::DejaVu24 : (digits == 3 ? &fonts::DejaVu18 : &fonts::DejaVu12);
        ui::text(line, x + kCell / 2, y + kCell / 2, font, style.fg);
      }
    }
    if (!g2048::canMove(board_)) {
      ui::banner(tr(GAME_OVER), kTop + kBoard / 2, ui::kRed);
    } else if (winBanner_) {
      ui::banner("2048!", kTop + kBoard / 2, ui::kAmber);
    }
    ui::hints(410, tr(BACK), tr(NEW_GAME));
  }

  ui::Row* settings(int& count) override {
    static const char* const themes[kThemeCount] = {tr(SOFT), tr(BRIGHT)};
    static ui::Row rows[] = {{tr(SOUND), ui::onOffLabels(), 2, reinterpret_cast<uint8_t*>(&settings_.sound)},
                             {tr(VIBRATION), ui::onOffLabels(), 2, reinterpret_cast<uint8_t*>(&settings_.vibration)},
                             {tr(THEME), themes, kThemeCount, &settings_.theme}};
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
