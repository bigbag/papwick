#include <BoardSupport.h>
#include <I18n.h>
#include <Logging.h>
#include <esp_heap_caps.h>
#include <nvs.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>

#include "Net.h"
#include "Shell.h"
#include "WifiSetup.h"

namespace {

constexpr const char* TAG = "SETTINGS";

// The option lists fill at first use, after start-up, so they take the texts of the current language.
const char* const* sleepLabels() {
  static const char* const l[] = {tr(SLEEP_30S), tr(SLEEP_1MIN), tr(SLEEP_2MIN), tr(SLEEP_5MIN), tr(NEVER)};
  static_assert(sizeof(l) / sizeof(*l) == sizeof(sys::kSleepSec) / sizeof(*sys::kSleepSec), "sleep labels");
  return l;
}

const char* const* unitLabels() {
  static const char* const l[] = {tr(METRIC), tr(IMPERIAL)};
  return l;
}

const char* const* volumeLabels() {
  static const char* const l[] = {tr(OFF), tr(LOW), tr(MEDIUM), tr(HIGH)};
  static_assert(sizeof(l) / sizeof(*l) == sizeof(sys::kVolumes), "volume labels");
  return l;
}

const char* const* lightLabels() {
  static const char* const l[] = {tr(LOW), tr(MEDIUM), tr(HIGH), tr(MAX)};
  static_assert(sizeof(l) / sizeof(*l) == sizeof(sys::kBrightness), "brightness labels");
  return l;
}

enum Page : uint8_t { DevicePage, TimePage, WifiPage, AppsPage, AboutPage, kPageCount };
constexpr StrId kPageNames[] = {StrId::STR_DEVICE, StrId::STR_TIME, StrId::STR_WIFI, StrId::STR_APPS, StrId::STR_ABOUT};

enum class Mode : uint8_t { Pages, AppPage, TimeZone, SetTime, WifiSetup, Confirm };
enum class Ask : uint8_t { Forget, ClearData, ResetAll };
constexpr StrId kQuestions[] = {StrId::STR_FORGET_Q, StrId::STR_CLEAR_DATA_Q, StrId::STR_RESET_ALL_Q};

constexpr StrId kTimeLabels[] = {StrId::STR_HOUR, StrId::STR_MINUTE, StrId::STR_DAY, StrId::STR_MONTH, StrId::STR_YEAR};
constexpr int kTimeSteps = 5;

size_t usedEntries(const char* ns) {
  nvs_handle_t h;
  size_t n = 0;
  if (nvs_open(ns, NVS_READONLY, &h) != ESP_OK) return 0;
  nvs_get_used_entry_count(h, &n);
  nvs_close(h);
  return n;
}

App& appAt(int i) {
  int n;
  return *shell::apps(n)[i];
}

class Settings : public App {
 public:
  const AppInfo& info() const override {
    static const AppInfo i = {"settings", tr(SETTINGS), ui::kGrey, icon::gear};
    return i;
  }

  void start() override {
    mode_ = Mode::Pages;
    slot_ = DevicePage;
  }

  void stop() override {
    if (mode_ == Mode::WifiSetup) wifisetup::end();
  }

  bool keepAwake() const override { return mode_ == Mode::WifiSetup; }

  void showApp(int i) {
    app_ = i;
    slot_ = AppsPage + i / ui::kMaxRows;
    mode_ = Mode::AppPage;
    dirty = true;
  }

  void update(const ui::Input& in) override {
    if (net::syncState() != shownSync_) dirty = true;
    switch (mode_) {
      case Mode::Pages:
        updatePages(in);
        break;
      case Mode::AppPage:
        updateAppPage(in);
        break;
      case Mode::TimeZone:
      case Mode::SetTime:
        updatePicker(in);
        break;
      case Mode::WifiSetup:
        if (!wifisetup::update(in, dirty)) show(Mode::Pages);
        break;
      case Mode::Confirm:
        if (in.green) answer();
        if (in.blue) show(ask_ == Ask::ClearData ? Mode::AppPage : Mode::Pages);
        break;
    }
  }

  void draw() override {
    shownSync_ = net::syncState();
    switch (mode_) {
      case Mode::Pages: {
        ui::clear();
        ui::Row rows[ui::kMaxRows];
        const int n = pageRows(rows);
        ui::rowsDraw(title(), rows, n);
        ui::pageDots(slot_, slotCount());
        ui::hints(ui::kHintsY, tr(BACK), nullptr);
        break;
      }
      case Mode::AppPage: {
        ui::clear();
        ui::Row rows[ui::kMaxRows];
        const int n = appRows(rows);
        snprintf(entries_, sizeof(entries_), tr(FMT_ENTRIES), usedEntries(appAt(app_).info().id));
        ui::rowsDraw(appAt(app_).info().name, rows, n);
        ui::hints(ui::kHintsY, tr(BACK), nullptr);
        break;
      }
      case Mode::TimeZone:
      case Mode::SetTime:
        ui::clear();
        ui::pickerDraw(picker_);
        break;
      case Mode::WifiSetup:
        wifisetup::draw();
        break;
      case Mode::Confirm:
        ui::confirmDraw(i18n::get(kQuestions[static_cast<int>(ask_)]), tr(YES));
        break;
    }
  }

 private:
  void show(Mode m) {
    mode_ = m;
    dirty = true;
  }

  void confirm(Ask a) {
    ask_ = a;
    show(Mode::Confirm);
  }

  // The apps use as many pages as necessary. slot_ counts all pages from 0.
  static int appPages() {
    int n;
    shell::apps(n);
    return std::max(1, (n + ui::kMaxRows - 1) / ui::kMaxRows);
  }

  static int slotCount() { return kPageCount - 1 + appPages(); }

  Page page() const {
    if (slot_ < AppsPage) return static_cast<Page>(slot_);
    return slot_ < AppsPage + appPages() ? AppsPage : AboutPage;
  }

  int firstApp() const { return (slot_ - AppsPage) * ui::kMaxRows; }

  const char* title() {
    if (page() != AppsPage || appPages() == 1) return i18n::get(kPageNames[page()]);
    snprintf(title_, sizeof(title_), tr(FMT_APPS_PAGE), slot_ - AppsPage + 1, appPages());
    return title_;
  }

  int pageRows(ui::Row* rows) {
    sys::Device& d = shell::device();
    switch (page()) {
      case DevicePage:
        rows[0] = {tr(BRIGHTNESS), lightLabels(), 4, &d.brightness};
        rows[1] = {tr(SLEEP), sleepLabels(), 5, &d.sleep};
        rows[2] = {tr(SOUND), volumeLabels(), 4, &d.volume};
        rows[3] = {tr(VIBRATION), ui::onOffLabels(), 2, reinterpret_cast<uint8_t*>(&d.vibration)};
        rows[4] = {tr(UNITS), unitLabels(), 2, &d.units};
        return 5;
      case TimePage: {
        const time_t now = time(nullptr);
        tm local;
        localtime_r(&now, &local);
        strftime(clock_, sizeof(clock_), "%H:%M", &local);
        sys::tzLabel(sys::kZones[d.zone], tz_, sizeof(tz_));
        rows[0] = {tr(ZONE), nullptr, 0, nullptr, tz_};
        rows[1] = {tr(SET_TIME), nullptr, 0, nullptr, clock_};
        rows[2] = {tr(SYNC_NOW), nullptr, 0, nullptr,
                   net::syncState() == net::Sync::Idle ? syncedText() : net::syncText()};
        return 3;
      }
      case WifiPage: {
        const sys::NetworkList& l = net::networks();
        rows[0] = {tr(ADD_NETWORK), nullptr, 0, nullptr, ">"};
        for (int i = 0; i < l.count; ++i) rows[i + 1] = {l.items[i].ssid, nullptr, 0, nullptr, tr(FORGET)};
        return l.count + 1;
      }
      case AppsPage: {
        int n;
        App* const* apps = shell::apps(n);
        const int count = std::min(n - firstApp(), ui::kMaxRows);
        for (int i = 0; i < count; ++i) rows[i] = {apps[firstApp() + i]->info().name, nullptr, 0, nullptr, ">"};
        return count;
      }
      default:
        snprintf(battery_, sizeof(battery_), "%d%%", board::batteryLevel());
        snprintf(memory_, sizeof(memory_), "%u KB", heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024);
        rows[0] = {tr(VERSION), nullptr, 0, nullptr, PAPWICK_VERSION};
        rows[1] = {tr(BUILT), nullptr, 0, nullptr, __DATE__};
        rows[2] = {tr(BATTERY), nullptr, 0, nullptr, battery_};
        rows[3] = {tr(FREE_RAM), nullptr, 0, nullptr, memory_};
        rows[4] = {tr(RESET_ALL), nullptr, 0, nullptr, ">"};
        return 5;
    }
  }

  const char* syncedText() {
    const time_t at = shell::device().syncedAt;
    if (at == 0) return tr(NOT_SYNCED);
    tm local;
    localtime_r(&at, &local);
    strftime(synced_, sizeof(synced_), "%d.%m %H:%M", &local);
    return synced_;
  }

  void updatePages(const ui::Input& in) {
    if (in.blue) {
      shell::home();
      return;
    }
    if (in.swipeLeft || in.swipeRight) {
      slot_ = (slot_ + (in.swipeLeft ? 1 : slotCount() - 1)) % slotCount();
      dirty = true;
      return;
    }
    ui::Row rows[ui::kMaxRows];
    const int i = ui::rowsUpdate(rows, pageRows(rows), in);
    if (i < 0) return;
    dirty = true;
    switch (page()) {
      case DevicePage:
        shell::saveDevice();
        shell::applyDevice();
        if (rows[i].options == volumeLabels()) board::beep(880, 120);
        if (rows[i].options == ui::onOffLabels()) board::vibrate(150);
        LOG_INF(TAG, "%s: %s", rows[i].label, rows[i].options[*rows[i].value]);
        break;
      case TimePage:
        if (i == 0) {
          picker_ = {tr(TIME_ZONE), nullptr, static_cast<float>(shell::device().zone), 0, sys::kZoneCount - 1, 1, 0,
                     zoneName};
          show(Mode::TimeZone);
        } else if (i == 1) {
          const time_t now = time(nullptr);
          localtime_r(&now, &time_);
          timeStep_ = 0;
          setTimePicker();
          show(Mode::SetTime);
        } else {
          net::startSync();
        }
        break;
      case WifiPage:
        if (i == 0) {
          wifisetup::begin();
          show(Mode::WifiSetup);
        } else {
          // A running sync reorders the list, so it could move the network away from network_.
          net::cancel();
          network_ = i - 1;
          confirm(Ask::Forget);
        }
        break;
      case AppsPage:
        showApp(firstApp() + i);
        break;
      default:
        if (i == 4) confirm(Ask::ResetAll);
        break;
    }
  }

  int appRows(ui::Row* rows) {
    App& a = appAt(app_);
    int n;
    const ui::Row* own = a.settings(n);
    n = std::min(n, ui::kMaxRows - 2);
    for (int i = 0; i < n; ++i) rows[i] = own[i];
    rows[n] = {tr(CLEAR_DATA), nullptr, 0, nullptr, ">"};
    rows[n + 1] = {tr(STORAGE), nullptr, 0, nullptr, entries_};
    return n + 2;
  }

  void updateAppPage(const ui::Input& in) {
    if (in.blue) {
      show(Mode::Pages);
      return;
    }
    ui::Row rows[ui::kMaxRows];
    const int n = appRows(rows);
    const int i = ui::rowsUpdate(rows, n, in);
    if (i < 0) return;
    if (i < n - 2) {
      appAt(app_).settingsChanged();
      dirty = true;
    } else if (i == n - 2) {
      confirm(Ask::ClearData);
    }
  }

  void setTimePicker() {
    const int values[] = {time_.tm_hour, time_.tm_min, time_.tm_mday, time_.tm_mon + 1, time_.tm_year + 1900};
    const float mins[] = {0, 0, 1, 1, 2024};
    const float maxs[] = {23, 59, 31, 12, 2099};
    picker_ = {i18n::get(kTimeLabels[timeStep_]),
               "",
               static_cast<float>(values[timeStep_]),
               mins[timeStep_],
               maxs[timeStep_],
               1,
               0};
  }

  void storeTimePicker() {
    const int v = static_cast<int>(picker_.value);
    int* fields[] = {&time_.tm_hour, &time_.tm_min, &time_.tm_mday, &time_.tm_mon, &time_.tm_year};
    *fields[timeStep_] = timeStep_ == 3 ? v - 1 : (timeStep_ == 4 ? v - 1900 : v);
  }

  static const char* zoneName(int i) {
    static char label[16];
    sys::tzLabel(sys::kZones[i], label, sizeof(label));
    return label;
  }

  void updatePicker(const ui::Input& in) {
    if (ui::pickerUpdate(picker_, in)) dirty = true;
    if (mode_ == Mode::TimeZone) {
      if (in.green) {
        shell::device().zone = static_cast<uint8_t>(picker_.value);
        shell::saveDevice();
        shell::applyDevice();
        show(Mode::Pages);
      } else if (in.blue) {
        show(Mode::Pages);
      }
      return;
    }
    if (in.green) {
      storeTimePicker();
      if (++timeStep_ < kTimeSteps) {
        setTimePicker();
        dirty = true;
        return;
      }
      time_.tm_mday = std::min(time_.tm_mday, sys::daysInMonth(time_.tm_mon + 1, time_.tm_year + 1900));
      time_.tm_sec = 0;
      time_.tm_isdst = -1;
      board::setClock(mktime(&time_));
      LOG_INF(TAG, "Clock set by hand");
      show(Mode::Pages);
    } else if (in.blue) {
      if (timeStep_ == 0) {
        show(Mode::Pages);
        return;
      }
      --timeStep_;
      setTimePicker();
      dirty = true;
    }
  }

  void answer() {
    switch (ask_) {
      case Ask::Forget:
        sys::removeNetwork(net::networks(), network_);
        net::saveNetworks();
        show(Mode::Pages);
        break;
      case Ask::ClearData:
        appAt(app_).clearData();
        LOG_INF(TAG, "Cleared %s", appAt(app_).info().id);
        show(Mode::AppPage);
        break;
      case Ask::ResetAll:
        shell::resetAll();
        break;
    }
  }

  Mode mode_ = Mode::Pages;
  int slot_ = DevicePage;
  Ask ask_ = Ask::Forget;
  int app_ = 0, network_ = 0, timeStep_ = 0;
  net::Sync shownSync_ = net::Sync::Idle;
  ui::Picker picker_{};
  tm time_{};
  char title_[12], tz_[16], clock_[8], synced_[16], battery_[8], memory_[12], entries_[16];
};

Settings& instance() {
  static Settings a;
  return a;
}

}  // namespace

App& settingsApp() { return instance(); }

void shell::openAppPage(App& app) {
  int n;
  App* const* apps = shell::apps(n);
  for (int i = 0; i < n; ++i) {
    if (apps[i] != &app) continue;
    open(instance());
    instance().showApp(i);
  }
}
