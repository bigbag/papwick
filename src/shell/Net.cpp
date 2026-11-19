#include "Net.h"

#include <BoardSupport.h>
#include <I18n.h>
#include <Logging.h>
#include <Preferences.h>

#include "Shell.h"

namespace net {

namespace {

constexpr const char* TAG = "NET";
constexpr uint32_t kConnectMs = 15000;
// lwIP waits up to 5 s before the first request, and 15 s for each server. Thus both servers get one try.
constexpr uint32_t kNtpMs = 40000;

Preferences prefs;
sys::NetworkList list;
Sync state = Sync::Idle;
int index = 0;
uint32_t deadline = 0;
bool keep = false;

bool due(uint32_t at) { return static_cast<int32_t>(millis() - at) >= 0; }

void tryNetwork(int i) {
  index = i;
  state = Sync::Connecting;
  deadline = millis() + kConnectMs;
  if (!board::wifi::startConnect(list.items[i].ssid, list.items[i].password)) deadline = millis();
}

void finish(Sync s) {
  board::stopNtp();
  if (!keep || !board::wifi::connected()) board::wifi::shutdown();
  state = s;
  LOG_INF(TAG, "Sync %s", syncText());
}

}  // namespace

void begin() {
  if (!prefs.begin("wifi", false)) LOG_ERR(TAG, "NVS open failed");
  sys::NetworkList tmp;
  if (shell::load(prefs, "list", tmp) && sys::isValid(tmp)) list = tmp;
  LOG_INF(TAG, "%u saved networks", list.count);
}

sys::NetworkList& networks() { return list; }

void saveNetworks() {
  if (prefs.putBytes("list", &list, sizeof(list)) != sizeof(list)) LOG_ERR(TAG, "Save networks failed");
}

void clear() {
  if (!prefs.clear()) LOG_ERR(TAG, "NVS clear failed");
  list = {};
}

void startSync(bool keepConnected) {
  if (busy()) return;
  keep = keepConnected;
  if (list.count == 0) {
    state = Sync::NoNetwork;
    return;
  }
  tryNetwork(0);
}

void updateSync() {
  if (state == Sync::Connecting) {
    if (board::wifi::connected()) {
      LOG_INF(TAG, "Connected to %s", list.items[index].ssid);
      if (index > 0) {
        sys::promoteNetwork(list, index);
        saveNetworks();
      }
      board::startNtp();
      state = Sync::Ntp;
      deadline = millis() + kNtpMs;
    } else if (due(deadline)) {
      LOG_WRN(TAG, "No connection to %s", list.items[index].ssid);
      if (index + 1 < list.count) {
        tryNetwork(index + 1);
      } else {
        finish(Sync::Failed);
      }
    }
  } else if (state == Sync::Ntp) {
    if (board::ntpDone()) {
      shell::device().syncedAt = static_cast<uint32_t>(time(nullptr));
      shell::saveDevice();
      finish(Sync::Done);
    } else if (due(deadline)) {
      finish(Sync::Failed);
    }
  }
}

void cancel() {
  if (!busy()) return;
  board::stopNtp();
  board::wifi::shutdown();
  state = Sync::Idle;
  LOG_INF(TAG, "Sync cancelled");
}

Sync syncState() { return state; }

bool busy() { return state == Sync::Connecting || state == Sync::Ntp; }

const char* syncText() {
  static const StrId names[] = {StrId::STR__COUNT,    StrId::STR_SYNC_CONNECTING, StrId::STR_SYNC_GETTING_TIME,
                                StrId::STR_SYNC_DONE, StrId::STR_SYNC_FAILED,     StrId::STR_SYNC_NO_WIFI};
  return i18n::get(names[static_cast<int>(state)]);
}

}  // namespace net
