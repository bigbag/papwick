#include "BoardSupport.h"

#include <Logging.h>
#include <M5Unified.h>
#include <esp_sntp.h>
#include <esp_system.h>
#include <sys/time.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace board {

namespace {

constexpr const char* TAG = "BOARD";

char timeZone[48] = "UTC0";

tm buildTime() {
  static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char month[4] = {};
  tm t = {};
  sscanf(__DATE__, "%3s %d %d", month, &t.tm_mday, &t.tm_year);
  sscanf(__TIME__, "%d:%d:%d", &t.tm_hour, &t.tm_min, &t.tm_sec);
  const char* found = strstr(months, month);
  t.tm_mon = found ? static_cast<int>(found - months) / 3 : 0;
  t.tm_year -= 1900;
  return t;
}

}  // namespace

void begin() {
  auto cfg = M5.config();
  cfg.internal_mic = true;
  cfg.internal_spk = true;
  cfg.fallback_board = m5::board_t::board_M5StopWatch;
  // poll() reads the red button itself.
  cfg.pmic_button = false;
  M5.begin(cfg);
  // shortcut: after a cold power-on, the display stays black. The M5IOE1 that powers the panel starts after
  // the display init. M5.begin() turns the panel power on, so one restart brings the display up like a warm
  // reset. Remove this when M5GFX waits for the M5IOE1 at a cold start.
  if (esp_reset_reason() == ESP_RST_POWERON) esp_restart();
  beginInput();
  Serial.begin(115200);
  // Without a reader on the PC, each log line blocks for up to 100 ms. Drop the line instead.
  Serial.setTxTimeoutMs(0);
}

void waitForSerial(unsigned long timeoutMs) {
  const unsigned long start = millis();
  while (!Serial && millis() - start < timeoutMs) delay(10);
}

void setTimeZone(const char* tz) {
  snprintf(timeZone, sizeof(timeZone), "%s", tz);
  setenv("TZ", timeZone, 1);
  tzset();
  LOG_INF(TAG, "Time zone %s", timeZone);
}

time_t syncClock() {
  const auto dt = M5.Rtc.getDateTime();
  if (dt.isValid()) {
    M5.Rtc.setSystemTimeFromRtc();
    // setSystemTimeFromRtc() sets TZ to GMT0 and restores it from a getenv() pointer that setenv() overwrote.
    setenv("TZ", timeZone, 1);
    tzset();
    LOG_INF(TAG, "RTC time %04d-%02d-%02d %02d:%02d:%02d UTC", dt.date.year, dt.date.month, dt.date.date, dt.time.hours,
            dt.time.minutes, dt.time.seconds);
    return time(nullptr);
  }

  // The build time is the local time of the build host. mktime() converts it with the time zone setting.
  tm t = buildTime();
  t.tm_isdst = 0;
  setClock(mktime(&t));
  LOG_WRN(TAG, "RTC had no valid time, set to build time %s %s", __DATE__, __TIME__);
  return time(nullptr);
}

void setClock(time_t t) {
  const timeval tv = {t, 0};
  settimeofday(&tv, nullptr);
  tm utc;
  gmtime_r(&t, &utc);
  if (!M5.Rtc.setDateTime(&utc)) LOG_ERR(TAG, "RTC write failed");
}

void startNtp() {
  sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
  // configTzTime() also sets TZ, so it gets the time zone of the settings.
  configTzTime(timeZone, "pool.ntp.org", "time.google.com");
  LOG_INF(TAG, "NTP started");
}

bool ntpDone() {
  if (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) return false;
  setClock(time(nullptr));
  stopNtp();
  LOG_INF(TAG, "NTP time set");
  return true;
}

void stopNtp() { esp_sntp_stop(); }

}  // namespace board
