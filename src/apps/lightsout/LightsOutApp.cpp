#include <BoardSupport.h>
#include <I18n.h>
#include <LightsOut.h>
#include <Logging.h>
#include <esp_random.h>

#include <algorithm>
#include <cstdio>

#include "App.h"

namespace {

constexpr const char* TAG = "LIGHTS";
constexpr int kCell = 56, kGap = 6;
constexpr int kGrid = lightsout::kSide * kCell + (lightsout::kSide - 1) * kGap;
constexpr uint16_t kLit = lgfx::color565(250, 215, 120);
constexpr uint16_t kUnlit = lgfx::color565(48, 48, 54);

class LightsOutApp : public App {
 public:
  const AppInfo& info() const override {
    static const AppInfo i = {"lights", tr(LIGHTS_OUT), lgfx::color565(245, 225, 140), icon::lights};
    return i;
  }

  void boot() override {
    if (!prefs_.begin(info().id, false)) LOG_ERR(TAG, "NVS open failed");
    if (!shell::load(prefs_, "game", game_) || !lightsout::valid(game_)) newGame();
  }

  void start() override { askNew_ = false; }

  void stop() override { shell::save(prefs_, "game", game_); }

  void update(const ui::Input& in) override {
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
      if (lightsout::isWon(game_)) {
        newGame();
      } else {
        askNew_ = true;
      }
      dirty = true;
      return;
    }
    if (!in.tap || !ui::Rect{left(), top(), kGrid, kGrid}.hit(in.x, in.y)) return;
    const int x = std::clamp(in.x - left(), 0, kGrid - 1), y = std::clamp(in.y - top(), 0, kGrid - 1);
    const int pitch = kCell + kGap;
    if (x % pitch >= kCell || y % pitch >= kCell) return;
    if (!lightsout::toggle(game_, static_cast<uint8_t>(y / pitch * lightsout::kSide + x / pitch))) return;
    board::vibrate(20);
    if (lightsout::isWon(game_)) {
      LOG_INF(TAG, "Solved in %lu moves", static_cast<unsigned long>(game_.moves));
      board::beep(880, 120);
      board::beep(1320, 300);
    }
    dirty = true;
  }

  void draw() override {
    if (askNew_) {
      ui::confirmDraw(tr(NEW_GAME_Q), tr(NEW_GAME));
      return;
    }
    ui::clear();
    char line[32];
    snprintf(line, sizeof(line), tr(FMT_MOVES), static_cast<unsigned long>(game_.moves));
    ui::text(line, ui::cx(), 55, &fonts::DejaVu18);
    auto& c = ui::canvas();
    for (uint8_t i = 0; i < lightsout::kCells; ++i) {
      const int x = left() + i % lightsout::kSide * (kCell + kGap), y = top() + i / lightsout::kSide * (kCell + kGap);
      c.fillRoundRect(x, y, kCell, kCell, 8, (game_.lights >> i) & 1 ? kLit : kUnlit);
    }
    if (lightsout::isWon(game_)) ui::banner(tr(SOLVED), ui::cy(), ui::kAmber);
    ui::hints(410, tr(BACK), tr(NEW_GAME));
  }

  void clearData() override {
    App::clearData();
    newGame();
  }

 private:
  static int left() { return ui::cx() - kGrid / 2; }
  static int top() { return ui::cy() - kGrid / 2; }

  void newGame() {
    game_ = lightsout::generate(esp_random());
    LOG_INF(TAG, "New game");
  }

  Preferences prefs_;
  lightsout::Game game_;
  bool askNew_ = false;
};

}  // namespace

App& lightsOutApp() {
  static LightsOutApp a;
  return a;
}
