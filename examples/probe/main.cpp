#include <BoardSupport.h>
#include <Logging.h>
#include <M5Unified.h>

static constexpr const char* TAG = "PROBE";
static constexpr uint32_t kMicPauseMs = 300;

static M5Canvas canvas(&M5.Display);
static String i2cList;
static String lastEvent = "touch, swipe or press a button";
static int touchX = -1, touchY = -1;
static uint32_t micRestartAt = 0;

static void scanI2c() {
  bool found[128] = {};
  M5.In_I2C.scanID(found);
  i2cList = "i2c:";
  for (int a = 0; a < 128; ++a) {
    if (found[a]) i2cList += " " + String(a, HEX);
  }
  LOG_INF(TAG, "I2C scan:%s", i2cList.c_str() + 4);
}

static void event(const char* text) {
  lastEvent = text;
  LOG_INF(TAG, "Event: %s", text);
}

static void handleInput(const board::Input& in) {
  if (in.tap || in.hold || in.repeat) {
    touchX = in.x;
    touchY = in.y;
  }
  if (in.tap) event("tap");
  if (in.hold) event("touch hold");
  if (in.swipeLeft) event("swipe left");
  if (in.swipeRight) event("swipe right");
  if (in.swipeUp) event("swipe up");
  if (in.swipeDown) event("swipe down");
  if (in.red) event("red click");
  if (in.redDouble) event("red double click");
  if (in.green) {
    event("green: vibrate");
    board::vibrate(200);
  }
  if (in.blue) {
    event("blue: beep");
    board::micEnd();
    board::beep(1500, 150);
    micRestartAt = millis() + kMicPauseMs;
  }
  if (micRestartAt && static_cast<int32_t>(millis() - micRestartAt) >= 0) {
    micRestartAt = 0;
    board::micBegin();
  }
}

// The full 466x466 push takes about 32 ms, so the text screen updates at 2 Hz.
// Markers draw directly on the display every loop.
static void drawInfoScreen(float ax, float ay, float az) {
  const int w = M5.Display.width(), h = M5.Display.height(), cx = w / 2;
  const time_t now = time(nullptr);
  tm utc;
  gmtime_r(&now, &utc);
  const int level = board::micLevel();

  canvas.fillScreen(TFT_BLACK);
  canvas.drawCircle(cx, h / 2, cx - 2, TFT_DARKGREY);
  canvas.setTextColor(TFT_YELLOW);
  canvas.drawString("Papwick probe", cx, 70);
  canvas.setTextColor(TFT_WHITE);

  char line[64];
  snprintf(line, sizeof(line), "board %d  %dx%d", static_cast<int>(M5.getBoard()), w, h);
  canvas.drawString(line, cx, 110);
  canvas.drawString(i2cList, cx, 140);
  snprintf(line, sizeof(line), "bat %d%% %dmV %s", board::batteryLevel(), M5.Power.getBatteryVoltage(),
           board::onUsb() ? "usb" : "");
  canvas.drawString(line, cx, 170);
  strftime(line, sizeof(line), "utc %Y-%m-%d %H:%M:%S", &utc);
  canvas.drawString(line, cx, 200);
  snprintf(line, sizeof(line), "imu %d  %.2f %.2f %.2f", static_cast<int>(M5.Imu.getType()), ax, ay, az);
  canvas.drawString(line, cx, 230);
  snprintf(line, sizeof(line), "touch %d,%d", touchX, touchY);
  canvas.drawString(line, cx, 260);
  snprintf(line, sizeof(line), "mic %d", level);
  canvas.drawString(line, cx, 290);
  if (level > 0) canvas.fillRect(cx - 100, 305, min(200, level / 50), 8, TFT_GREEN);
  canvas.setTextColor(TFT_CYAN);
  canvas.drawString(lastEvent, cx, 340);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("blue beep, green vibrate", cx, 375);
  canvas.pushSprite(0, 0);
}

void setup() {
  board::begin();
  board::waitForSerial(3000);
  LOG_INF(TAG, "Board %d, display %dx%d, PSRAM free %u", static_cast<int>(M5.getBoard()), M5.Display.width(),
          M5.Display.height(), ESP.getFreePsram());
  LOG_INF(TAG, "IMU type %d, RTC %d, touch %d", static_cast<int>(M5.Imu.getType()), M5.Rtc.isEnabled(),
          M5.Touch.isEnabled());

  if (!canvas.createSprite(M5.Display.width(), M5.Display.height())) {
    M5.Display.drawString("sprite alloc failed", 120, 233);
    LOG_ERR(TAG, "Sprite alloc failed");
    while (true) delay(1000);
  }
  canvas.setTextDatum(middle_center);
  canvas.setTextSize(2);

  board::setBrightness(160);
  board::setVolume(128);
  board::syncClock();
  scanI2c();
  board::micBegin();
}

void loop() {
  static int dotX = -1, dotY = -1, ringX = -1, ringY = -1;
  static uint32_t lastInfo = 0;

  M5.update();
  handleInput(board::poll());
  board::updateAudio();

  float ax = 0, ay = 0, az = 0;
  M5.Imu.getAccel(&ax, &ay, &az);

  if (millis() - lastInfo >= 500) {
    lastInfo = millis();
    drawInfoScreen(ax, ay, az);
    dotX = ringX = -1;
  }

  const int cx = M5.Display.width() / 2, cy = M5.Display.height() / 2;
  const int nx = cx - static_cast<int>(ax * 120), ny = cy + static_cast<int>(ay * 120);
  M5.Display.startWrite();
  if (dotX != nx || dotY != ny) {
    if (dotX >= 0) M5.Display.fillCircle(dotX, dotY, 8, TFT_BLACK);
    M5.Display.fillCircle(nx, ny, 8, TFT_ORANGE);
    dotX = nx;
    dotY = ny;
  }
  if (touchX >= 0 && (ringX != touchX || ringY != touchY)) {
    if (ringX >= 0) M5.Display.drawCircle(ringX, ringY, 20, TFT_BLACK);
    M5.Display.drawCircle(touchX, touchY, 20, TFT_MAGENTA);
    ringX = touchX;
    ringY = touchY;
  }
  M5.Display.endWrite();
}
