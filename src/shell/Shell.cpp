#include "Shell.h"

#include <BoardSupport.h>
#include <I18n.h>
#include <Logging.h>
#include <Preferences.h>

#include "Net.h"

namespace {

constexpr const char* TAG = "SHELL";
constexpr uint32_t kFrameMs = 50, kIdleMs = 10;
constexpr uint32_t kResyncSec = 7 * 24 * 3600;
constexpr int kSyncMinBattery = 20;

Preferences prefs;
sys::Device dev;
// The settings that NVS holds. A failed write returns dev to them.
sys::Device savedDev;
App* const* appList = nullptr;
int appCount = 0;
App* current = nullptr;
// The power question has its own redraw flag, so a background app cannot redraw it in each loop.
bool powerMenu = false, menuDirty = false, menuUsb = false;
uint32_t lastDrawMs = 0;

const char* resetReason() {
  static const char* const names[] = {
      "unknown",       "power-on", "external",     "software",  "panic", "interrupt watchdog",
      "task watchdog", "watchdog", "deep sleep",   "brownout",  "SDIO",  "USB",
      "JTAG",          "eFuse",    "power glitch", "CPU lockup"};
  const unsigned r = esp_reset_reason();
  return r < sizeof(names) / sizeof(*names) ? names[r] : "unknown";
}

const char* powerQuestion() { return menuUsb ? tr(SCREEN_OFF_Q) : tr(POWER_OFF_Q); }

void powerOff() {
  powerMenu = false;
  board::stopAudio();
  // With USB, the M5PM1 cuts only the display and the ESP32 continues to run. The app stays open.
  if (board::onUsb()) {
    current->dirty = true;
    board::setLight(board::Light::Off);
    return;
  }
  current->stop();
  ui::clear();
  ui::text(tr(POWER_OFF), ui::cx(), ui::cy(), &fonts::DejaVu24, ui::kGrey);
  ui::push();
  delay(500);
  board::powerOff();
}

void autoSync() {
  const uint32_t now = time(nullptr);
  const bool stale = dev.syncedAt == 0 || now < dev.syncedAt || now - dev.syncedAt > kResyncSec;
  // Wi-Fi current peaks on a low battery can reset the device.
  const bool power = board::onUsb() || board::batteryLevel() >= kSyncMinBattery;
  if (stale && power && net::networks().count > 0) net::startSync();
}

}  // namespace

void App::clearData() {
  Preferences p;
  if (!p.begin(info().id, false) || !p.clear()) LOG_ERR(TAG, "Clear '%s' failed", info().id);
  p.end();
}

namespace shell {

void begin(App* const* apps, int count) {
  appList = apps;
  appCount = count;
  board::begin();
  board::imuOff();
  // On battery no serial host can connect, so the wait only delays the first frame.
  if (board::onUsb()) board::waitForSerial(1500);
  LOG_INF(TAG, "Papwick %s, built %s, reset: %s", PAPWICK_VERSION, __DATE__, resetReason());
  LOG_INF(TAG, "Board %d, display %dx%d, PSRAM free %u", static_cast<int>(M5.getBoard()), M5.Display.width(),
          M5.Display.height(), ESP.getFreePsram());
  if (!ui::begin()) {
    M5.Display.drawString(tr(OUT_OF_MEMORY), 160, 230);
    while (true) delay(1000);
  }
  if (!prefs.begin("sys", false)) LOG_ERR(TAG, "NVS open failed");
  sys::Device tmp;
  if (shell::load(prefs, "device", tmp) && sys::isValid(tmp)) {
    dev = tmp;
  } else {
    LOG_INF(TAG, "No valid device settings in NVS");
  }
  savedDev = dev;
  net::begin();
  for (int i = 0; i < appCount; ++i) appList[i]->boot();
  LOG_INF(TAG, "Device: sleep %u, units %u, volume %u, vibration %u, brightness %u, zone %d min", dev.sleep, dev.units,
          dev.volume, dev.vibration, dev.brightness, sys::kZones[dev.zone]);
  applyDevice();
  board::syncClock();
  LOG_INF(TAG, "Battery %d%%, USB %d", board::batteryLevel(), board::onUsb());
  autoSync();
  open(launcherApp());
}

void loop() {
  M5.update();
  const ui::Input in = board::poll();
  const uint32_t offMs = current->keepAwake() || net::busy() ? 0 : sys::kSleepSec[dev.sleep] * 1000u;
  const bool wakeOnly = board::updateLight(in, offMs);
  if (wakeOnly) current->dirty = menuDirty = true;
  if (!wakeOnly) {
    if (in.redDouble) {
      powerMenu = menuDirty = true;
      menuUsb = board::onUsb();
    } else if (powerMenu) {
      if (in.green && board::onUsb() != menuUsb) {
        // The power source changed, so the question changes and needs a new answer.
        menuUsb = !menuUsb;
        menuDirty = true;
      } else if (in.green) {
        powerOff();
      } else if (in.blue || in.red) {
        powerMenu = false;
        current->dirty = true;
      }
    } else if (in.red) {
      if (current == &launcherApp()) {
        board::setLight(board::Light::Off);
      } else {
        home();
      }
    } else {
      current->update(in);
    }
  }
  for (int i = 0; i < appCount; ++i) appList[i]->background();
  board::updateAudio();
  board::updateChargeLed();
  net::updateSync();

  const uint32_t ms = millis();
  if (board::light() == board::Light::Off) {
    delay(20);
  } else if (powerMenu ? menuDirty : current->dirty) {
    lastDrawMs = ms;
    if (powerMenu) {
      menuDirty = false;
      ui::confirmDraw(powerQuestion(), tr(YES));
    } else {
      current->dirty = false;
      current->draw();
    }
    ui::push();
  } else if (ms - lastDrawMs >= kFrameMs) {
    lastDrawMs = ms;
    if (!powerMenu) current->animate();
  } else {
    // The idle task halts the CPU until the next interrupt. A busy loop keeps the CPU at full power.
    delay(kIdleMs);
  }
}

void open(App& app) {
  if (current) current->stop();
  LOG_INF(TAG, "Open %s", app.info().name);
  current = &app;
  app.start();
  app.dirty = true;
}

void home() { open(launcherApp()); }

App* const* apps(int& count) {
  count = appCount;
  return appList;
}

sys::Device& device() { return dev; }

const char* deviceName() { return dev.name[0] ? dev.name : board::wifi::deviceName(); }

bool saveDevice() {
  if (prefs.putBytes("device", &dev, sizeof(dev)) == sizeof(dev)) {
    savedDev = dev;
    return true;
  }
  LOG_ERR(TAG, "Save device settings failed");
  // The screen then shows the settings that the device keeps after a restart, and a retry writes again.
  dev = savedDev;
  return false;
}

void applyDevice() {
  board::setBrightness(sys::kBrightness[dev.brightness]);
  board::setVolume(sys::kVolumes[dev.volume]);
  board::setVibration(dev.vibration);
  char tz[16];
  sys::posixTz(sys::kZones[dev.zone], tz, sizeof(tz));
  board::setTimeZone(tz);
}

void resetAll() {
  LOG_WRN(TAG, "Reset all data");
  for (int i = 0; i < appCount; ++i) appList[i]->clearData();
  net::clear();
  if (!prefs.clear()) LOG_ERR(TAG, "NVS clear failed");
  ESP.restart();
}

}  // namespace shell
