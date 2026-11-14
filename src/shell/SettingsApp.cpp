#include <BoardSupport.h>
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

constexpr const char* kSleep[] = {"30 s", "1 min", "2 min", "5 min", "Never"};
constexpr const char* kUnits[] = {"Metric", "Imperial"};
constexpr const char* kVolume[] = {"Off", "Low", "Med", "High"};
constexpr const char* kOnOff[] = {"Off", "On"};
constexpr const char* kLight[] = {"Low", "Med", "High", "Max"};
static_assert(sizeof(kSleep) / sizeof(*kSleep) == sizeof(sys::kSleepSec) / sizeof(*sys::kSleepSec), "sleep labels");
static_assert(sizeof(kVolume) / sizeof(*kVolume) == sizeof(sys::kVolumes), "volume labels");
static_assert(sizeof(kLight) / sizeof(*kLight) == sizeof(sys::kBrightness), "brightness labels");

enum Page : uint8_t { DevicePage, TimePage, WifiPage, AppsPage, AboutPage, kPageCount };
constexpr const char* kPageNames[] = {"Device", "Time", "Wi-Fi", "Apps", "About"};

enum class Mode : uint8_t { Pages, AppPage, TimeZone, SetTime, WifiSetup, Confirm };
enum class Ask : uint8_t { Forget, ClearData, ResetAll };
constexpr const char* kQuestions[] = {"Forget network?", "Clear app data?", "Reset all data?"};

constexpr const char* kTimeLabels[] = {"Hour", "Minute", "Day", "Month", "Year"};
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
    static const AppInfo i = {"settings", "Settings", ui::kGrey, "SET"};
    return i;
  }

  void start() override {
    mode_ = Mode::Pages;
    page_ = DevicePage;
  }

  void stop() override {
    if (mode_ == Mode::WifiSetup) wifisetup::end();
  }

  bool keepAwake() const override { return mode_ == Mode::WifiSetup; }

  void showApp(int i) {
    app_ = i;
    page_ = AppsPage;
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
        ui::rowsDraw(kPageNames[page_], rows, n);
        ui::pageDots(page_, kPageCount);
        ui::hints(ui::kHintsY, "back", nullptr);
        break;
      }
      case Mode::AppPage: {
        ui::clear();
        ui::Row rows[ui::kMaxRows];
        const int n = appRows(rows);
        ui::rowsDraw(appAt(app_).info().name, rows, n);
        ui::hints(ui::kHintsY, "back", nullptr);
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
        ui::confirmDraw(kQuestions[static_cast<int>(ask_)], "yes");
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

  int pageRows(ui::Row* rows) {
    sys::Device& d = shell::device();
    switch (page_) {
      case DevicePage:
        rows[0] = {"Brightness", kLight, 4, &d.brightness};
        rows[1] = {"Sleep", kSleep, 5, &d.sleep};
        rows[2] = {"Sound", kVolume, 4, &d.volume};
        rows[3] = {"Vibration", kOnOff, 2, reinterpret_cast<uint8_t*>(&d.vibration)};
        rows[4] = {"Units", kUnits, 2, &d.units};
        return 5;
      case TimePage: {
        const time_t now = time(nullptr);
        tm local;
        localtime_r(&now, &local);
        strftime(clock_, sizeof(clock_), "%H:%M", &local);
        sys::tzLabel(sys::kZones[d.zone], tz_, sizeof(tz_));
        rows[0] = {"Zone", nullptr, 0, nullptr, tz_};
        rows[1] = {"Set time", nullptr, 0, nullptr, clock_};
        rows[2] = {"Sync now", nullptr, 0, nullptr,
                   net::syncState() == net::Sync::Idle ? syncedText() : net::syncText()};
        return 3;
      }
      case WifiPage: {
        const sys::NetworkList& l = net::networks();
        rows[0] = {"Add network", nullptr, 0, nullptr, ">"};
        for (int i = 0; i < l.count; ++i) rows[i + 1] = {l.items[i].ssid, nullptr, 0, nullptr, "forget"};
        return l.count + 1;
      }
      case AppsPage: {
        int n;
        App* const* apps = shell::apps(n);
        for (int i = 0; i < n && i < ui::kMaxRows; ++i) rows[i] = {apps[i]->info().name, nullptr, 0, nullptr, ">"};
        return n;
      }
      default:
        snprintf(battery_, sizeof(battery_), "%d%%", board::batteryLevel());
        snprintf(memory_, sizeof(memory_), "%u KB", heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024);
        rows[0] = {"Version", nullptr, 0, nullptr, __DATE__};
        rows[1] = {"Battery", nullptr, 0, nullptr, battery_};
        rows[2] = {"Free RAM", nullptr, 0, nullptr, memory_};
        rows[3] = {"Reset all", nullptr, 0, nullptr, ">"};
        return 4;
    }
  }

  const char* syncedText() {
    const time_t at = shell::device().syncedAt;
    if (at == 0) return "never";
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
      page_ = static_cast<Page>((page_ + (in.swipeLeft ? 1 : kPageCount - 1)) % kPageCount);
      dirty = true;
      return;
    }
    ui::Row rows[ui::kMaxRows];
    const int i = ui::rowsUpdate(rows, pageRows(rows), in);
    if (i < 0) return;
    dirty = true;
    switch (page_) {
      case DevicePage:
        shell::saveDevice();
        shell::applyDevice();
        if (rows[i].options == kVolume) board::beep(880, 120);
        if (rows[i].options == kOnOff) board::vibrate(150);
        LOG_INF(TAG, "%s: %s", rows[i].label, rows[i].options[*rows[i].value]);
        break;
      case TimePage:
        if (i == 0) {
          picker_ = {"Time zone", nullptr, static_cast<float>(shell::device().zone), 0, sys::kZoneCount - 1, 1,
                     0,           zoneName};
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
        showApp(i);
        break;
      default:
        if (i == 3) confirm(Ask::ResetAll);
        break;
    }
  }

  int appRows(ui::Row* rows) {
    App& a = appAt(app_);
    int n;
    const ui::Row* own = a.settings(n);
    n = std::min(n, ui::kMaxRows - 2);
    for (int i = 0; i < n; ++i) rows[i] = own[i];
    snprintf(entries_, sizeof(entries_), "%u entries", usedEntries(a.info().id));
    rows[n] = {"Clear data", nullptr, 0, nullptr, ">"};
    rows[n + 1] = {"Storage", nullptr, 0, nullptr, entries_};
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
    picker_ = {
        kTimeLabels[timeStep_], "", static_cast<float>(values[timeStep_]), mins[timeStep_], maxs[timeStep_], 1, 0};
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
  Page page_ = DevicePage;
  Ask ask_ = Ask::Forget;
  int app_ = 0, network_ = 0, timeStep_ = 0;
  net::Sync shownSync_ = net::Sync::Idle;
  ui::Picker picker_{};
  tm time_{};
  char tz_[16], clock_[8], synced_[16], battery_[8], memory_[12], entries_[16];
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
