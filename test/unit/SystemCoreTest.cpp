#include <cstring>

#include "Device.h"
#include "test_utils.h"

namespace {

using namespace sys;

void testDevice(TestUtils::TestRunner& r) {
  Device d;
  r.expectTrue(isValid(d), "default device settings are valid");
  d.zone = kZoneCount;
  r.expectTrue(!isValid(d), "zone past the table is invalid");
  d = Device{};
  d.units = 2;
  r.expectTrue(!isValid(d), "unknown units are invalid");
  d = Device{};
  memset(&d.vibration, 2, 1);
  r.expectTrue(!isValid(d), "vibration byte other than 0 or 1 is invalid");
  r.expectTrue(isValidName("") && isValidName("Kitchen 2") && isValidName("123456789012345"), "valid names");
  r.expectFalse(isValidName("1234567890123456"), "a name of 16 characters is too long");
  r.expectFalse(isValidName(" Kitchen") || isValidName("Kitchen "), "spaces at the ends are invalid");
  r.expectFalse(isValidName("\xD0\x9A\xD1\x83") || isValidName("a\tb"), "non-ASCII and control characters are invalid");
  d = Device{};
  memset(d.name, 'x', sizeof(d.name));
  r.expectFalse(isValid(d), "a name without NUL is invalid");
  r.expectTrue(daysInMonth(2, 2024) == 29 && daysInMonth(2, 2026) == 28 && daysInMonth(2, 2100) == 28 &&
                   daysInMonth(2, 2000) == 29 && daysInMonth(4, 2026) == 30 && daysInMonth(12, 2026) == 31,
               "days in month with leap years");

  char buf[16];
  tzLabel(330, buf, sizeof(buf));
  r.expectTrue(strcmp(buf, "UTC+5:30") == 0, "label of UTC+5:30");
  tzLabel(-210, buf, sizeof(buf));
  r.expectTrue(strcmp(buf, "UTC-3:30") == 0, "label of UTC-3:30");

  posixTz(330, buf, sizeof(buf));
  r.expectTrue(strcmp(buf, "UTC-5:30") == 0, "POSIX TZ of UTC+5:30 has the inverted sign");
  posixTz(-210, buf, sizeof(buf));
  r.expectTrue(strcmp(buf, "UTC+3:30") == 0, "POSIX TZ of UTC-3:30 has the inverted sign");

  bool sorted = true;
  for (int i = 1; i < kZoneCount; ++i) sorted = sorted && kZones[i - 1] < kZones[i];
  r.expectTrue(sorted && kZones[0] == -720 && kZones[kZoneCount - 1] == 840, "zones go from UTC-12 to UTC+14");
  r.expectTrue(kZones[zoneFor(330)] == 330 && kZones[zoneFor(345)] == 345, "India and Nepal offsets are zones");
  r.expectTrue(kZones[Device{}.zone] == 0, "default zone is UTC");
  r.expectTrue(kZones[zoneFor(17)] == 0, "unknown offset maps to UTC");
}

void testNetworks(TestUtils::TestRunner& r) {
  NetworkList l;
  r.expectTrue(isValid(l), "empty list is valid");
  r.expectTrue(!addNetwork(l, "", "password"), "empty SSID is rejected");
  r.expectTrue(!addNetwork(l, "home", "short"), "password below 8 characters is rejected");
  r.expectTrue(addNetwork(l, "open", ""), "open network without password is accepted");
  r.expectTrue(addNetwork(l, "home", "password1") && l.count == 2 && strcmp(l.items[0].ssid, "home") == 0,
               "new network goes first");
  r.expectTrue(addNetwork(l, "open", "password2") && l.count == 2 && strcmp(l.items[0].password, "password2") == 0,
               "same SSID replaces the old entry and goes first");
  addNetwork(l, "a", "");
  addNetwork(l, "b", "");
  addNetwork(l, "c", "");
  r.expectTrue(l.count == kMaxNetworks && strcmp(l.items[0].ssid, "c") == 0 && strcmp(l.items[3].ssid, "open") == 0,
               "full list drops the last network");
  promoteNetwork(l, 2);
  r.expectTrue(strcmp(l.items[0].ssid, "a") == 0 && strcmp(l.items[1].ssid, "c") == 0, "promote moves to first");
  removeNetwork(l, 0);
  r.expectTrue(l.count == 3 && strcmp(l.items[0].ssid, "c") == 0 && isValid(l), "remove closes the gap");
  l.items[1].ssid[0] = '\0';
  r.expectTrue(!isValid(l), "entry without SSID is invalid");
}

}  // namespace

int main() {
  TestUtils::TestRunner runner("SystemCore");
  testDevice(runner);
  testNetworks(runner);
  return runner.exitCode();
}
