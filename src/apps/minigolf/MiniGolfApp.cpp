#include <BoardSupport.h>
#include <Golf.h>
#include <I18n.h>
#include <Logging.h>
#include <esp_heap_caps.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <tuple>

#include "App.h"

namespace {

constexpr const char* TAG = "GOLF";
constexpr uint32_t kFrameMs = 25;
// Ready and Play go back to the menu after this time without input. In Play, a tilt change of kHandTilt is input.
// A hand moves the device more than this. A device on a table does not, also while the ball rolls on it.
constexpr uint32_t kIdleMs = 60000;
constexpr float kHandTilt = 0.02f;
// The ball sinks for kSinkMs. The result shows at kResultMs.
constexpr uint32_t kSinkMs = 300, kResultMs = 800, kFlashMs = 1000, kBounceGapMs = 120;
// The ball patch holds the ball, its shadow and the old ball when the ball moves 20 px or less.
constexpr int kPatch = 64, kMillPatch = 120, kMaxStep = 20;
constexpr int kHudW = 150, kHudH = 30, kHudY = 30;
// The weight of a new tilt sample. It smooths the sensor noise.
constexpr float kSmooth = 0.35f;
// The calibration ignores the shake of the button push for kSettleMs, then averages the tilt until kMeasureMs.
constexpr uint32_t kSettleMs = 500, kMeasureMs = 1500;
// The dot of the calibration screen moves this far for 1 g, up to the ring.
constexpr float kDotGain = 250;
constexpr int kRing = 50;
constexpr float kPi = 3.14159265f;
constexpr golf::Rgb kBall = {244, 244, 236}, kWood = {150, 96, 54}, kHub = {200, 200, 210};

enum class Screen : uint8_t { Menu, Calibrate, Ready, Play, Result };

// The tilt that the sensor reads on a flat surface.
struct Level {
  float x = 0, y = 0;
  bool valid() const { return std::fabs(x) <= 0.5f && std::fabs(y) <= 0.5f; }
};

golf::Rgb unpack(uint16_t p) {
  const uint16_t v = static_cast<uint16_t>(p >> 8 | p << 8);
  return {static_cast<uint8_t>((v >> 11) << 3), static_cast<uint8_t>(((v >> 5) & 63) << 2),
          static_cast<uint8_t>((v & 31) << 3)};
}

uint16_t blend(uint16_t p, golf::Rgb c, float t) {
  const golf::Rgb b = unpack(p);
  auto m = [t](uint8_t x, uint8_t y) { return static_cast<uint8_t>(x + (y - x) * t); };
  return golf::pixel({m(b.r, c.r), m(b.g, c.g), m(b.b, c.b)});
}

uint16_t color(golf::Rgb c) { return lgfx::color565(c.r, c.g, c.b); }

golf::Rgb scale(golf::Rgb c, float k) {
  auto v = [k](uint8_t x) { return static_cast<uint8_t>(std::clamp(x * k, 0.0f, 255.0f)); };
  return {v(c.r), v(c.g), v(c.b)};
}

void formatTime(char* out, size_t n, uint32_t ms) {
  snprintf(out, n, "%lu:%02lu.%lu", static_cast<unsigned long>(ms / 60000), static_cast<unsigned long>(ms / 1000 % 60),
           static_cast<unsigned long>(ms / 100 % 10));
}

void star(int x, int y, int r, uint16_t color) {
  float px[10], py[10];
  for (int i = 0; i < 10; ++i) {
    const float a = -kPi / 2 + i * kPi / 5, k = i % 2 ? r * 0.42f : r;
    px[i] = x + k * cosf(a);
    py[i] = y + k * sinf(a);
  }
  for (int i = 0; i < 10; ++i) ui::canvas().fillTriangle(x, y, px[i], py[i], px[(i + 1) % 10], py[(i + 1) % 10], color);
}

void stars(int y, int r, int filled) {
  for (int k = 0; k < 3; ++k) star(ui::cx() + (k - 1) * r * 5 / 2, y, r, k < filled ? ui::kAmber : ui::kDark);
}

class MiniGolfApp : public App {
 public:
  const AppInfo& info() const override {
    static const AppInfo i = {"minigolf", tr(MINI_GOLF), lgfx::color565(110, 190, 110), icon::flag};
    return i;
  }

  void boot() override {
    if (!prefs_.begin(info().id, false)) LOG_ERR(TAG, "NVS open failed");
    if (!shell::load(prefs_, "progress", progress_) || !progress_.valid()) progress_ = {};
    calibrated_ = shell::load(prefs_, "level", level_) && level_.valid();
    if (!calibrated_) level_ = {};
  }

  void start() override {
    screen_ = Screen::Menu;
    hole_ = std::min<int>(hole_, progress_.selectable() - 1);
    noMemory_ = false;
  }

  void stop() override {
    setImu(false);
    heap_caps_free(bg_);
    bg_ = nullptr;
    painted_ = -1;
    ball_.deleteSprite();
    mill_.deleteSprite();
    hud_.deleteSprite();
  }

  bool keepAwake() const override { return screen_ != Screen::Menu && screen_ != Screen::Result; }

  uint32_t frameMs() const override { return keepAwake() ? kFrameMs : 50; }

  void update(const ui::Input& in) override {
    const uint32_t now = millis();
    if (in.any()) activeMs_ = now;
    switch (screen_) {
      case Screen::Menu:
        if (in.blue) {
          shell::home();
        } else if (in.green && calibrated_) {
          ready();
        } else if (in.green || in.hold) {
          calibrate();
        } else if (in.swipeLeft || in.swipeRight) {
          const int next = std::clamp(hole_ + (in.swipeLeft ? 1 : -1), 0, progress_.selectable() - 1);
          if (next != hole_) {
            hole_ = next;
            dirty = true;
          }
        }
        break;
      case Screen::Calibrate:
        if (in.blue || now - activeMs_ > kIdleMs) {
          menu();
        } else if (in.green && !measureMs_) {
          measureMs_ = now;
          sumX_ = sumY_ = 0;
          samples_ = 0;
          dirty = true;
        }
        break;
      case Screen::Ready:
        if (painted_ != hole_ && loading_) {
          paintHole();
        } else if (in.blue) {
          menu();
        } else if (in.green) {
          play();
        } else if (now - activeMs_ > kIdleMs) {
          menu();
        }
        break;
      case Screen::Play:
        if (in.blue) {
          menu();
        } else if (in.green) {
          play();
        } else if (game_.sunk() && now - sunkMs_ >= kResultMs) {
          finish();
        } else if (now - activeMs_ > kIdleMs) {
          menu();
        }
        break;
      case Screen::Result:
        if (in.blue) {
          menu();
        } else if (in.green) {
          hole_ = std::min(hole_ + 1, golf::kHoles - 1);
          ready();
        }
        break;
    }
  }

  void draw() override {
    switch (screen_) {
      case Screen::Menu:
        drawMenu();
        break;
      case Screen::Calibrate:
        ui::clear();
        ui::text(tr(CALIBRATION), ui::cx(), 70, &fonts::DejaVu24);
        ui::text(tr(PLACE_FLAT), ui::cx(), 130, &fonts::DejaVu18);
        if (measureMs_) {
          ui::text(tr(WAIT), ui::cx(), ui::kHintsY, &fonts::DejaVu18, ui::kAmber);
        } else {
          ui::hints(ui::kHintsY, tr(BACK), tr(CALIBRATE));
        }
        break;
      case Screen::Ready:
        if (painted_ != hole_) {
          ui::clear();
          char line[24];
          snprintf(line, sizeof(line), tr(FMT_LEVEL), hole_ + 1);
          ui::text(line, ui::cx(), ui::cy(), &fonts::DejaVu40);
          loading_ = true;
          return;
        }
        compose(ui::canvas(), 0, 0);
        drawReady();
        break;
      case Screen::Play:
        compose(ui::canvas(), 0, 0);
        hudText_[0] = '\0';
        break;
      case Screen::Result:
        compose(ui::canvas(), 0, 0);
        drawResult();
        break;
    }
  }

  void animate() override {
    if (screen_ == Screen::Calibrate) {
      measure(readTilt());
      return;
    }
    if (screen_ != Screen::Play || painted_ != hole_) return;
    readTilt();
    const uint32_t now = millis();
    const uint32_t ms = now - stepMs_;
    stepMs_ = now;
    const golf::Event e = game_.step(tiltX_ - level_.x, tiltY_ - level_.y, ms);
    if (std::hypot(tiltX_ - activeX_, tiltY_ - activeY_) > kHandTilt) {
      activeMs_ = now;
      activeX_ = tiltX_;
      activeY_ = tiltY_;
    }
    if (e == golf::Event::Sunk) sink(now);
    sound(e, now);
    const float oldX = shownX_, oldY = shownY_;
    const bool moved = game_.x() != oldX || game_.y() != oldY;
    shownX_ = game_.x();
    shownY_ = game_.y();
    const golf::Course& c = golf::course(hole_);
    if (c.mill.speed != 0) push(mill_, c.mill.x, c.mill.y);
    if (moved && std::hypot(shownX_ - oldX, shownY_ - oldY) > kMaxStep) push(ball_, oldX, oldY);
    if (moved || (game_.sunk() && now - sunkMs_ <= kSinkMs + kFrameMs)) push(ball_, shownX_, shownY_);
    drawHud(now);
  }

  void clearData() override {
    App::clearData();
    progress_ = {};
    level_ = {};
    calibrated_ = false;
    hole_ = 0;
    dirty = true;
  }

 private:
  void menu() {
    setImu(false);
    screen_ = Screen::Menu;
    dirty = true;
  }

  void calibrate() {
    if (!allocate()) {
      noMemory_ = true;
      menu();
      return;
    }
    setImu(true);
    measureMs_ = 0;
    activeMs_ = millis();
    screen_ = Screen::Calibrate;
    dirty = true;
  }

  // Averages the tilt after the green push. Then saves the level and opens the menu. Shows the tilt as a dot.
  void measure(bool read) {
    const uint32_t now = millis();
    if (read && measureMs_ && now - measureMs_ >= kSettleMs) {
      sumX_ += tiltX_;
      sumY_ += tiltY_;
      ++samples_;
    }
    if (measureMs_ && now - measureMs_ >= kMeasureMs) {
      const Level l = {sumX_ / std::max(samples_, 1), sumY_ / std::max(samples_, 1)};
      LOG_INF(TAG, "Calibration %.3f %.3f from %d samples", l.x, l.y, samples_);
      if (samples_ > 0 && l.valid()) {
        level_ = l;
        calibrated_ = true;
        shell::save(prefs_, "level", level_);
        board::beep(1320, 120);
        menu();
        return;
      }
      // The sensor gave no reading, the surface is not flat, or the device moved.
      board::beep(220, 220);
      measureMs_ = 0;
      dirty = true;
    }
    const float dx = (tiltX_ - level_.x) * kDotGain, dy = (tiltY_ - level_.y) * kDotGain;
    const float k = std::min(1.0f, kRing / std::max(1.0f, std::hypot(dx, dy)));
    mill_.fillScreen(TFT_BLACK);
    mill_.drawCircle(kMillPatch / 2, kMillPatch / 2, kRing + golf::kBallR, ui::kGrey);
    mill_.drawCircle(kMillPatch / 2, kMillPatch / 2, golf::kBallR + 2, ui::kGrey);
    mill_.fillSmoothCircle(kMillPatch / 2 + dx * k, kMillPatch / 2 + dy * k, golf::kBallR, TFT_WHITE);
    mill_.pushSprite(ui::cx() - kMillPatch / 2, ui::cy() + 20 - kMillPatch / 2);
  }

  void ready() {
    if (!allocate()) {
      noMemory_ = true;
      menu();
      return;
    }
    noMemory_ = false;
    setImu(false);
    game_.start(golf::course(hole_));
    shownX_ = game_.x();
    shownY_ = game_.y();
    loading_ = false;
    activeMs_ = millis();
    screen_ = Screen::Ready;
    dirty = true;
  }

  void play() {
    setImu(true);
    game_.start(golf::course(hole_));
    shownX_ = game_.x();
    shownY_ = game_.y();
    stepMs_ = activeMs_ = millis();
    record_ = false;
    LOG_INF(TAG, "Level %d", hole_ + 1);
    screen_ = Screen::Play;
    dirty = true;
  }

  // Saves the time when the ball drops. A button push during the sink animation must not lose it.
  void sink(uint32_t now) {
    sunkMs_ = now;
    record_ = progress_.record(hole_, game_.timeMs());
    shell::save(prefs_, "progress", progress_);
    LOG_INF(TAG, "Level %d in %lu ms%s", hole_ + 1, static_cast<unsigned long>(game_.timeMs()),
            record_ ? ", best" : "");
  }

  void finish() {
    setImu(false);
    if (record_) board::beep(1760, 200);
    screen_ = Screen::Result;
    dirty = true;
  }

  // A failed start leaves imuOn_ false, so the next level or calibration tries again.
  void setImu(bool on) {
    if (on == imuOn_) return;
    if (on && !board::imuOn()) return;
    if (!on) board::imuOff();
    imuOn_ = on;
    haveTilt_ = false;
  }

  bool readTilt() {
    float x, y;
    if (!board::tilt(x, y)) {
      // Without a reading the ball must not roll on with the last tilt.
      tiltX_ = level_.x;
      tiltY_ = level_.y;
      haveTilt_ = false;
      return false;
    }
    tiltX_ = haveTilt_ ? tiltX_ + (x - tiltX_) * kSmooth : x;
    tiltY_ = haveTilt_ ? tiltY_ + (y - tiltY_) * kSmooth : y;
    haveTilt_ = true;
    return true;
  }

  void sound(golf::Event e, uint32_t now) {
    switch (e) {
      case golf::Event::Bounce:
        if (now - bounceMs_ < kBounceGapMs) break;
        bounceMs_ = now;
        board::beep(1400, 12, 0);
        board::vibrate(15);
        break;
      case golf::Event::Water:
        flashMs_ = now;
        board::beep(220, 220);
        board::vibrate(150);
        break;
      case golf::Event::Sunk:
        board::beep(880, 120);
        board::beep(1320, 300);
        board::vibrate(80);
        break;
      case golf::Event::None:
        break;
    }
  }

  bool allocate() {
    if (!bg_) bg_ = static_cast<uint16_t*>(heap_caps_malloc(golf::kSize * golf::kSize * 2, MALLOC_CAP_SPIRAM));
    for (auto [s, w, h] : {std::make_tuple(&ball_, kPatch, kPatch), std::make_tuple(&mill_, kMillPatch, kMillPatch),
                           std::make_tuple(&hud_, kHudW, kHudH)}) {
      if (s->getBuffer()) continue;
      s->setColorDepth(16);
      s->createSprite(w, h);
    }
    if (bg_ && ball_.getBuffer() && mill_.getBuffer() && hud_.getBuffer()) return true;
    LOG_ERR(TAG, "Buffer alloc failed");
    stop();
    return false;
  }

  void paintHole() {
    const uint32_t t0 = millis();
    if (!golf::paint(golf::course(hole_), bg_)) {
      LOG_ERR(TAG, "Paint alloc failed");
      noMemory_ = true;
      menu();
      return;
    }
    painted_ = hole_;
    LOG_INF(TAG, "Level %d painted in %lu ms", hole_ + 1, static_cast<unsigned long>(millis() - t0));
    activeMs_ = millis();
    dirty = true;
  }

  // Pushes the patch of s centered at (x, y) with the background, the windmill bar and the ball.
  void push(M5Canvas& s, float x, float y) {
    const int x0 = std::clamp<int>(static_cast<int>(x) - s.width() / 2, 0, golf::kSize - s.width());
    const int y0 = std::clamp<int>(static_cast<int>(y) - s.height() / 2, 0, golf::kSize - s.height());
    compose(s, x0, y0);
    s.pushSprite(x0, y0);
    // A patch over the time band hides the time. Then drawHud() draws it again in the same frame.
    if (y0 < kHudY + kHudH / 2) hudText_[0] = '\0';
  }

  void compose(M5Canvas& s, int x0, int y0) {
    // M5GFX gives a 468 x 468 screen. The picture covers only kSize x kSize of it.
    const int w = std::min<int>(s.width(), golf::kSize - x0), h = std::min<int>(s.height(), golf::kSize - y0);
    if (w < s.width() || h < s.height()) s.fillScreen(TFT_BLACK);
    auto* out = static_cast<uint16_t*>(s.getBuffer());
    for (int j = 0; j < h; ++j) {
      memcpy(out + j * s.width(), bg_ + (y0 + j) * golf::kSize + x0, w * sizeof(uint16_t));
    }
    const golf::Course& c = golf::course(hole_);
    if (c.mill.speed != 0) drawMill(s, x0, y0, c.mill);
    if (screen_ != Screen::Result) drawBall(s, x0, y0);
  }

  // Darkens the pixels of s inside the shape. The shape is in screen coordinates.
  static void shadow(M5Canvas& s, int x0, int y0, const golf::Shape& shape, int left, int top, int right, int bottom) {
    auto* out = static_cast<uint16_t*>(s.getBuffer());
    for (int y = std::max(top, y0); y <= std::min<int>(bottom, y0 + s.height() - 1); ++y) {
      for (int x = std::max(left, x0); x <= std::min<int>(right, x0 + s.width() - 1); ++x) {
        const float d = golf::distance(shape, x + 0.5f, y + 0.5f);
        if (d < 0.5f) {
          uint16_t& p = out[(y - y0) * s.width() + x - x0];
          p = blend(p, {0, 0, 0}, 0.4f * std::min(1.0f, 0.5f - d));
        }
      }
    }
  }

  void drawMill(M5Canvas& s, int x0, int y0, const golf::Mill& m) {
    const float a = game_.millAngle();
    const float ux = cosf(a) * m.length, uy = sinf(a) * m.length;
    const golf::Shape bar = {golf::Shape::Capsule, m.x - ux + 4, m.y - uy + 4, m.x + ux + 4, m.y + uy + 4, 4, 0};
    shadow(s, x0, y0, bar, std::min(bar.x0, bar.x1) - 5, std::min(bar.y0, bar.y1) - 5, std::max(bar.x0, bar.x1) + 5,
           std::max(bar.y0, bar.y1) + 5);
    const float cx = m.x - x0, cy = m.y - y0;
    s.drawWideLine(cx - ux, cy - uy, cx + ux, cy + uy, 4, color(kWood));
    s.drawWideLine(cx - ux - 1, cy - uy - 1, cx + ux - 1, cy + uy - 1, 1.2f, color(scale(kWood, 1.35f)));
    s.fillSmoothCircle(cx, cy, 6, color(kHub));
    s.fillSmoothCircle(cx - 2, cy - 2, 2, TFT_WHITE);
  }

  // A lit sphere with a soft edge and a shadow. While the ball sinks, it gets smaller and darker.
  void drawBall(M5Canvas& s, int x0, int y0) {
    float r = golf::kBallR, dark = 1;
    if (game_.sunk()) {
      const float t = screen_ == Screen::Play ? std::min(1.0f, (millis() - sunkMs_) / float(kSinkMs)) : 1;
      r *= 1 - t;
      dark = 1 - 0.6f * t;
      if (r < 0.5f) return;
    }
    const float bx = shownX_, by = shownY_;
    const golf::Shape drop = {golf::Shape::Circle, bx + 2.5f, by + 2.5f, 0, 0, r, 0};
    shadow(s, x0, y0, drop, bx - r, by - r, bx + r + 4, by + r + 4);
    auto* out = static_cast<uint16_t*>(s.getBuffer());
    const int top = std::max<int>(by - r - 1, y0), bottom = std::min<int>(by + r + 1, y0 + s.height() - 1);
    const int left = std::max<int>(bx - r - 1, x0), right = std::min<int>(bx + r + 1, x0 + s.width() - 1);
    for (int y = top; y <= bottom; ++y) {
      for (int x = left; x <= right; ++x) {
        const float dx = (x + 0.5f - bx) / r, dy = (y + 0.5f - by) / r, d2 = dx * dx + dy * dy;
        const float cover = std::clamp((1 - std::sqrt(d2)) * r + 0.5f, 0.0f, 1.0f);
        if (cover <= 0) continue;
        const float nz = std::sqrt(std::max(0.0f, 1 - d2));
        const float lit = 0.62f + 0.45f * (-0.55f * dx - 0.55f * dy + 0.63f * nz);
        const float spot = std::pow(std::max(0.0f, (-dx - dy) * 0.5f + nz * 0.7f), 24.0f);
        golf::Rgb c = scale(kBall, lit * dark);
        c = {static_cast<uint8_t>(c.r + (255 - c.r) * spot), static_cast<uint8_t>(c.g + (255 - c.g) * spot),
             static_cast<uint8_t>(c.b + (255 - c.b) * spot)};
        uint16_t& p = out[(y - y0) * s.width() + x - x0];
        p = blend(p, c, cover);
      }
    }
  }

  void drawHud(uint32_t now) {
    char text[16];
    formatTime(text, sizeof(text), game_.timeMs());
    const bool flash = flashMs_ != 0 && now - flashMs_ < kFlashMs;
    if (strcmp(text, hudText_) == 0 && flash == hudFlash_) return;
    strcpy(hudText_, text);
    hudFlash_ = flash;
    const int x0 = ui::cx() - kHudW / 2, y0 = kHudY - kHudH / 2;
    auto* out = static_cast<uint16_t*>(hud_.getBuffer());
    for (int j = 0; j < kHudH; ++j)
      memcpy(out + j * kHudW, bg_ + (y0 + j) * golf::kSize + x0, kHudW * sizeof(uint16_t));
    hud_.setFont(&fonts::DejaVu24);
    hud_.setTextDatum(middle_center);
    hud_.setTextColor(flash ? ui::kRed : TFT_WHITE);
    hud_.drawString(text, kHudW / 2, kHudH / 2);
    hud_.pushSprite(x0, y0);
  }

  int totalStars() const {
    int n = 0;
    for (int i = 0; i < golf::kHoles; ++i) {
      if (progress_.best[i]) n += golf::stars(progress_.best[i], golf::course(i).parMs);
    }
    return n;
  }

  void drawMenu() {
    ui::clear();
    ui::text(tr(MINI_GOLF), ui::cx(), 70, &fonts::DejaVu24);
    char line[48], par[16], best[16];
    snprintf(line, sizeof(line), tr(FMT_LEVEL), hole_ + 1);
    ui::text(line, ui::cx(), 160, &fonts::DejaVu40);
    const uint32_t bestMs = progress_.best[hole_], parMs = golf::course(hole_).parMs;
    stars(225, 20, bestMs ? golf::stars(bestMs, parMs) : 0);
    formatTime(par, sizeof(par), parMs);
    formatTime(best, sizeof(best), bestMs);
    if (bestMs) {
      snprintf(line, sizeof(line), tr(FMT_PAR_BEST), par, best);
    } else {
      snprintf(line, sizeof(line), tr(FMT_PAR), par);
    }
    ui::text(line, ui::cx(), 275, &fonts::DejaVu18, ui::kGrey);
    snprintf(line, sizeof(line), tr(FMT_STARS), totalStars(), 3 * golf::kHoles);
    ui::text(line, ui::cx(), 308, &fonts::DejaVu12, ui::kGrey);
    if (noMemory_) {
      ui::text(tr(OUT_OF_MEMORY), ui::cx(), 350, &fonts::DejaVu18, ui::kRed);
    } else {
      ui::text(tr(HOLD_TO_CALIBRATE), ui::cx(), 350, &fonts::DejaVu12);
    }
    ui::hints(ui::kHintsY, tr(BACK), calibrated_ ? tr(START) : tr(CALIBRATE));
    ui::pageDots(hole_, progress_.selectable());
  }

  void drawReady() {
    char line[48], par[16];
    formatTime(par, sizeof(par), golf::course(hole_).parMs);
    snprintf(line, sizeof(line), tr(FMT_LEVEL_PAR), hole_ + 1, par);
    ui::text(line, ui::cx(), kHudY, &fonts::DejaVu18);
    ui::canvas().fillRoundRect(ui::cx() - 100, 422, 200, 28, 14, TFT_BLACK);
    ui::hints(436, tr(BACK), tr(START));
  }

  void drawResult() {
    constexpr int kW = 300, kTop = 128, kH = 222;
    auto& c = ui::canvas();
    c.fillRoundRect(ui::cx() - kW / 2, kTop, kW, kH, 18, record_ ? ui::kAmber : ui::kGrey);
    c.fillRoundRect(ui::cx() - kW / 2 + 3, kTop + 3, kW - 6, kH - 6, 15, TFT_BLACK);
    char line[48];
    snprintf(line, sizeof(line), tr(FMT_LEVEL), hole_ + 1);
    ui::text(line, ui::cx(), 158, &fonts::DejaVu18, ui::kGrey);
    formatTime(line, sizeof(line), game_.timeMs());
    ui::text(line, ui::cx(), 200, &fonts::DejaVu40);
    stars(248, 18, golf::stars(game_.timeMs(), golf::course(hole_).parMs));
    if (record_) ui::text(tr(NEW_BEST), ui::cx(), 282, &fonts::DejaVu18, ui::kAmber);
    if (hole_ == golf::kHoles - 1) {
      snprintf(line, sizeof(line), tr(FMT_STARS), totalStars(), 3 * golf::kHoles);
      ui::text(line, ui::cx(), record_ ? 304 : 290, &fonts::DejaVu12, ui::kGrey);
    }
    ui::hints(326, tr(BACK), hole_ < golf::kHoles - 1 ? tr(NEXT_LEVEL) : tr(RETRY));
  }

  Preferences prefs_;
  golf::Progress progress_;
  golf::Game game_;
  Screen screen_ = Screen::Menu;
  int hole_ = 0, painted_ = -1;
  uint16_t* bg_ = nullptr;
  M5Canvas ball_{&M5.Display}, mill_{&M5.Display}, hud_{&M5.Display};
  bool imuOn_ = false, haveTilt_ = false, loading_ = false, record_ = false, noMemory_ = false, hudFlash_ = false;
  bool calibrated_ = false;
  Level level_;
  float tiltX_ = 0, tiltY_ = 0, shownX_ = 0, shownY_ = 0, sumX_ = 0, sumY_ = 0, activeX_ = 0, activeY_ = 0;
  int samples_ = 0;
  uint32_t stepMs_ = 0, activeMs_ = 0, sunkMs_ = 0, bounceMs_ = 0, flashMs_ = 0, measureMs_ = 0;
  char hudText_[16] = "";
};

}  // namespace

App& miniGolfApp() {
  static MiniGolfApp a;
  return a;
}
