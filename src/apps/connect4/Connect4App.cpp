#include <BoardSupport.h>
#include <Connect4.h>
#include <I18n.h>
#include <Logging.h>

#include <cstdio>

#include "App.h"

namespace {

constexpr const char* TAG = "CONNECT4";
constexpr int kCell = 46, kTop = 112;
constexpr int kWidth = connect4::kCols * kCell, kHeight = connect4::kRows * kCell;
constexpr int kDepth[] = {1, 4, 8};
constexpr uint8_t kLevelCount = sizeof(kDepth) / sizeof(*kDepth);
constexpr uint16_t kFrame = lgfx::color565(58, 58, 66);
constexpr uint16_t kPlayerDisc = lgfx::color565(240, 190, 80);
constexpr uint16_t kDeviceDisc = lgfx::color565(220, 120, 140);

struct Connect4Settings {
  uint8_t version = 1;
  uint8_t level = 1;
};

class Connect4App : public App {
 public:
  const AppInfo& info() const override {
    static const AppInfo i = {"connect4", tr(CONNECT4), lgfx::color565(225, 165, 180), icon::discs};
    return i;
  }

  void boot() override {
    if (!prefs_.begin(info().id, false)) LOG_ERR(TAG, "NVS open failed");
    Connect4Settings s;
    if (shell::load(prefs_, "settings", s) && s.version == Connect4Settings{}.version && s.level < kLevelCount) {
      settings_ = s;
    }
    if (!shell::load(prefs_, "board", board_) || !connect4::isValid(board_)) board_ = connect4::Board{};
  }

  void start() override { askNew_ = false; }

  void stop() override { shell::save(prefs_, "board", board_); }

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
      if (board_.result != connect4::kNone) {
        newGame();
      } else {
        askNew_ = true;
      }
      dirty = true;
      return;
    }
    // The device moves after the frame with the move of the player is on the screen.
    if (board_.turn == connect4::kDevice && board_.result == connect4::kNone && !dirty) {
      play(connect4::bestMove(board_, kDepth[settings_.level]));
      return;
    }
    if (in.tap && board_.turn == connect4::kPlayer &&
        ui::Rect{left(), kTop - kCell, kWidth, kHeight + kCell}.hit(in.x, in.y)) {
      play((in.x - left()) / kCell);
    }
  }

  void draw() override {
    if (askNew_) {
      ui::confirmDraw(tr(NEW_GAME_Q), tr(NEW_GAME));
      return;
    }
    ui::clear();
    ui::text(status(), ui::cx(), 50, &fonts::DejaVu18, statusColor());
    char line[32];
    snprintf(line, sizeof(line), tr(FMT_TOTALS), board_.wins, board_.losses);
    ui::text(line, ui::cx(), 80, &fonts::DejaVu12, ui::kGrey);
    auto& c = ui::canvas();
    c.fillRoundRect(left() - 4, kTop - 4, kWidth + 8, kHeight + 8, 10, kFrame);
    int line4[4] = {-1, -1, -1, -1};
    if (board_.result == connect4::kPlayer || board_.result == connect4::kDevice) connect4::winLine(board_, line4);
    for (int row = 0; row < connect4::kRows; ++row) {
      for (int col = 0; col < connect4::kCols; ++col) {
        const int x = left() + col * kCell + kCell / 2, y = kTop + row * kCell + kCell / 2;
        const uint8_t cell = board_.cells[row][col];
        c.fillCircle(x, y, kCell / 2 - 4,
                     cell == connect4::kPlayer   ? kPlayerDisc
                     : cell == connect4::kDevice ? kDeviceDisc
                                                 : TFT_BLACK);
        for (int i : line4) {
          if (i == row * connect4::kCols + col) c.drawCircle(x, y, kCell / 2 - 2, TFT_WHITE);
        }
      }
    }
    ui::hints(410, tr(BACK), tr(NEW_GAME));
  }

  ui::Row* settings(int& count) override {
    static const char* const levels[kLevelCount] = {tr(EASY), tr(NORMAL), tr(HARD)};
    static ui::Row rows[] = {{tr(DIFFICULTY), levels, kLevelCount, &settings_.level}};
    count = 1;
    return rows;
  }

  void settingsChanged() override { shell::save(prefs_, "settings", settings_); }

  void clearData() override {
    App::clearData();
    settings_ = Connect4Settings{};
    board_ = connect4::Board{};
  }

 private:
  static int left() { return ui::cx() - kWidth / 2; }

  const char* status() const {
    switch (board_.result) {
      case connect4::kPlayer:
        return tr(YOU_WIN);
      case connect4::kDevice:
        return tr(DEVICE_WINS);
      case connect4::kDraw:
        return tr(DRAW);
      default:
        return board_.turn == connect4::kPlayer ? tr(YOUR_TURN) : tr(THINKING);
    }
  }

  uint16_t statusColor() const {
    if (board_.result == connect4::kNone) return board_.turn == connect4::kPlayer ? kPlayerDisc : kDeviceDisc;
    return TFT_WHITE;
  }

  void newGame() {
    connect4::newGame(board_);
    LOG_INF(TAG, "New game, %s first", board_.first == connect4::kPlayer ? "player" : "device");
  }

  void play(int col) {
    const uint8_t side = board_.turn;
    if (connect4::drop(board_, col) < 0) return;
    board::vibrate(15);
    if (board_.result != connect4::kNone) {
      LOG_INF(TAG, "Result %d, totals %u:%u:%u", board_.result, board_.wins, board_.losses, board_.draws);
      if (board_.result == connect4::kPlayer) {
        board::beep(880, 120);
        board::beep(1320, 300);
      } else if (board_.result == connect4::kDevice) {
        board::beep(330, 400);
      }
    } else if (side == connect4::kDevice) {
      board::beep(660, 40);
    }
    dirty = true;
  }

  Preferences prefs_;
  Connect4Settings settings_;
  connect4::Board board_;
  bool askNew_ = false;
};

}  // namespace

App& connect4App() {
  static Connect4App a;
  return a;
}
