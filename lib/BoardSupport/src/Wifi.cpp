#include "Wifi.h"

#include <Logging.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace board::wifi {

namespace {

constexpr const char* TAG = "WIFI";
constexpr int kMaxScan = 32;
bool rescanned = false;

bool isBlank(const char* s) {
  for (; *s; ++s) {
    if (!isspace(static_cast<unsigned char>(*s))) return false;
  }
  return true;
}

bool startMode(wifi_mode_t mode) {
  shutdown();
  WiFi.persistent(false);
  if (WiFi.mode(mode)) return true;
  LOG_ERR(TAG, "Mode %d failed", static_cast<int>(mode));
  shutdown();
  return false;
}

}  // namespace

bool startScan() {
  if (!startMode(WIFI_STA)) return false;
  if (WiFi.scanNetworks(true) == WIFI_SCAN_FAILED) {
    LOG_ERR(TAG, "Scan start failed");
    shutdown();
    return false;
  }
  rescanned = false;
  LOG_INF(TAG, "Scan started");
  return true;
}

int scanResults(Network* out, int maxCount) {
  int16_t found = WiFi.scanComplete();
  if (found == WIFI_SCAN_RUNNING) return -1;
  if (found == 0 && !rescanned) {
    // Same as papyrix-reader. A scan can return no networks. A second scan with longer channel times finds them.
    LOG_INF(TAG, "Scan found nothing, scan again");
    rescanned = true;
    if (WiFi.scanNetworks(true, false, false, 500) != WIFI_SCAN_FAILED) return -1;
  }
  if (found < 0) {
    LOG_ERR(TAG, "Scan failed: %d", found);
    found = 0;
  }

  Network unique[kMaxScan];
  int count = 0;
  for (int i = 0; i < found; ++i) {
    const String ssid = WiFi.SSID(i);
    if (isBlank(ssid.c_str())) continue;
    const int8_t rssi = WiFi.RSSI(i);
    const bool secured = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    Network* same = std::find_if(unique, unique + count, [&](const Network& n) { return ssid == n.ssid; });
    if (same != unique + count) {
      if (rssi > same->rssi) {
        same->rssi = rssi;
        same->secured = secured;
      }
      continue;
    }
    if (count == kMaxScan) continue;
    unique[count] = {{}, rssi, secured};
    snprintf(unique[count++].ssid, sizeof(Network::ssid), "%s", ssid.c_str());
  }
  WiFi.scanDelete();
  std::sort(unique, unique + count, [](const Network& a, const Network& b) { return a.rssi > b.rssi; });
  count = std::min(count, maxCount);
  std::copy(unique, unique + count, out);
  LOG_INF(TAG, "Scan found %d networks (raw %d)", count, found);
  return count;
}

bool startConnect(const char* ssid, const char* password) {
  if (!startMode(WIFI_STA)) return false;
  LOG_INF(TAG, "Connect to %s", ssid);
  if (WiFi.begin(ssid, password) != WL_CONNECT_FAILED) return true;
  LOG_ERR(TAG, "Connect start failed");
  shutdown();
  return false;
}

bool connected() { return WiFi.status() == WL_CONNECTED; }

bool startAp(const char* ssid, const char* password) {
  if (!startMode(WIFI_AP)) return false;
  // Same as papyrix-reader: HT20 gives more phones a stable link.
  if (esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20) != ESP_OK) LOG_WRN(TAG, "AP bandwidth HT20 failed");
  if (!WiFi.softAP(ssid, password)) {
    LOG_ERR(TAG, "AP start failed");
    shutdown();
    return false;
  }
  LOG_INF(TAG, "AP %s at %s", ssid, WiFi.softAPIP().toString().c_str());
  return true;
}

void shutdown() {
  if (WiFi.getMode() == WIFI_OFF) return;
  WiFi.scanDelete();
  if (!WiFi.mode(WIFI_OFF)) LOG_ERR(TAG, "Radio off failed");
  LOG_INF(TAG, "WiFi shut down");
}

const char* deviceName() {
  static char name[16] = "";
  if (!name[0]) snprintf(name, sizeof(name), "Papwick-%04X", static_cast<unsigned>((ESP.getEfuseMac() >> 32) & 0xFFFF));
  return name;
}

void setPowerSave(bool on) {
  if (!WiFi.setSleep(on)) LOG_ERR(TAG, "Power save %d failed", on);
}

}  // namespace board::wifi
