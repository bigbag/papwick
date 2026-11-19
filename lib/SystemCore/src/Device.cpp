#include "Device.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace sys {

namespace {

template <typename T, int N>
constexpr int count(const T (&)[N]) {
  return N;
}

void writeOffset(const char* prefix, int minutes, char* buf, size_t n) {
  const int m = std::abs(minutes);
  const char sign = minutes < 0 ? '-' : '+';
  if (m % 60) {
    snprintf(buf, n, "%s%c%d:%02d", prefix, sign, m / 60, m % 60);
  } else {
    snprintf(buf, n, "%s%c%d", prefix, sign, m / 60);
  }
}

bool terminated(const char* s, size_t n) { return memchr(s, '\0', n) != nullptr; }

}  // namespace

bool isValid(const Device& d) {
  // A bool read from flash can hold any byte, and the settings row uses it as an index.
  uint8_t vibration;
  memcpy(&vibration, &d.vibration, 1);
  return d.version == kDeviceVersion && vibration <= 1 && d.sleep < count(kSleepSec) && d.units <= 1 &&
         d.volume < count(kVolumes) && d.brightness < count(kBrightness) && d.zone < kZoneCount &&
         terminated(d.name, sizeof(d.name)) && isValidName(d.name);
}

bool isValidName(const char* s) {
  const size_t n = strlen(s);
  if (n >= sizeof(Device::name) || (n > 0 && (s[0] == ' ' || s[n - 1] == ' '))) return false;
  for (size_t i = 0; i < n; ++i) {
    if (s[i] < 0x20 || s[i] > 0x7E) return false;
  }
  return true;
}

int daysInMonth(int month, int year) {
  static constexpr uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  return month == 2 && leap ? 29 : days[month - 1];
}

void tzLabel(int minutes, char* buf, size_t n) { writeOffset("UTC", minutes, buf, n); }

void posixTz(int minutes, char* buf, size_t n) { writeOffset("UTC", -minutes, buf, n); }

bool isValid(const NetworkList& l) {
  if (l.version != NetworkList{}.version || l.count > kMaxNetworks) return false;
  for (int i = 0; i < l.count; ++i) {
    const Network& w = l.items[i];
    if (!terminated(w.ssid, sizeof(w.ssid)) || !terminated(w.password, sizeof(w.password)) || !w.ssid[0]) return false;
  }
  return true;
}

bool addNetwork(NetworkList& l, const char* ssid, const char* password) {
  const size_t ssidLen = strlen(ssid), passLen = strlen(password);
  if (ssidLen == 0 || ssidLen >= sizeof(Network::ssid) || (passLen > 0 && passLen < 8) ||
      passLen >= sizeof(Network::password)) {
    return false;
  }
  for (int i = 0; i < l.count; ++i) {
    if (strcmp(l.items[i].ssid, ssid) == 0) {
      removeNetwork(l, i);
      break;
    }
  }
  if (l.count == kMaxNetworks) --l.count;
  memmove(&l.items[1], &l.items[0], l.count * sizeof(Network));
  l.items[0] = {};
  memcpy(l.items[0].ssid, ssid, ssidLen);
  memcpy(l.items[0].password, password, passLen);
  ++l.count;
  return true;
}

void promoteNetwork(NetworkList& l, int i) {
  if (i <= 0 || i >= l.count) return;
  const Network w = l.items[i];
  memmove(&l.items[1], &l.items[0], i * sizeof(Network));
  l.items[0] = w;
}

void removeNetwork(NetworkList& l, int i) {
  if (i < 0 || i >= l.count) return;
  memmove(&l.items[i], &l.items[i + 1], (l.count - i - 1) * sizeof(Network));
  l.items[--l.count] = {};
}

void appOrder(const char* saved, const char* const* ids, int count, int* order) {
  int n = 0;
  auto used = [&](int i) { return std::find(order, order + n, i) != order + n; };
  for (const char* s = saved; *s;) {
    const size_t len = strcspn(s, ",");
    for (int i = 0; i < count; ++i) {
      if (!used(i) && strlen(ids[i]) == len && strncmp(ids[i], s, len) == 0) {
        order[n++] = i;
        break;
      }
    }
    s += len;
    if (*s) ++s;
  }
  for (int i = 0; i < count; ++i) {
    if (!used(i)) order[n++] = i;
  }
}

}  // namespace sys
