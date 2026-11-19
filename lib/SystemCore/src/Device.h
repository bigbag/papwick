#pragma once

#include <cstddef>
#include <cstdint>

namespace sys {

constexpr uint8_t kDeviceVersion = 4;
// Screen-off time in seconds per option. 0 means never.
constexpr uint16_t kSleepSec[] = {30, 60, 120, 300, 0};
constexpr uint8_t kVolumes[] = {0, 90, 170, 255};
constexpr uint8_t kBrightness[] = {40, 90, 160, 255};

// The UTC offsets in minutes that clocks use, daylight saving time offsets included. The user selects the offset of
// the clocks, so the zone does not follow daylight saving time.
inline constexpr int16_t kZones[] = {-720, -660, -600, -570, -540, -480, -420, -360, -300, -240, -210, -180, -150, -120,
                                     -60,  0,    60,   120,  180,  210,  240,  270,  300,  330,  345,  360,  390,  420,
                                     480,  525,  540,  570,  600,  630,  660,  720,  765,  780,  825,  840};
constexpr int kZoneCount = sizeof(kZones) / sizeof(*kZones);

// Returns the zone with this offset, or UTC.
constexpr uint8_t zoneFor(int minutes) {
  for (int i = 0; i < kZoneCount; ++i) {
    if (kZones[i] == minutes) return i;
  }
  return zoneFor(0);
}

// The option fields are indices into the tables above. units holds a fit::Units value.
struct Device {
  uint8_t version = kDeviceVersion;
  uint8_t sleep = 2;
  uint8_t units = 0;
  uint8_t volume = 2;
  bool vibration = true;
  uint8_t brightness = 2;
  // An index into kZones.
  uint8_t zone = zoneFor(0);
  // Unix time of the last NTP sync. 0 means never.
  uint32_t syncedAt = 0;
  // The name that other devices show. Empty gives "Papwick-XXXX".
  char name[16] = "";
};

bool isValid(const Device& d);
// True for 0 to 15 printable ASCII characters. The fonts have no other characters. Spaces at the ends are invalid.
bool isValidName(const char* s);
// month is 1 to 12.
int daysInMonth(int month, int year);

// Writes the offset for the user, for example "UTC+3" or "UTC-9:30".
void tzLabel(int minutes, char* buf, size_t n);
// Writes the POSIX TZ string, for example "UTC-5:30" for UTC+5:30.
void posixTz(int minutes, char* buf, size_t n);

constexpr int kMaxNetworks = 4;

struct Network {
  char ssid[33];
  char password[65];
};

struct NetworkList {
  uint8_t version = 1;
  uint8_t count = 0;
  Network items[kMaxNetworks] = {};
};

bool isValid(const NetworkList& l);
// Puts the network first. It replaces a network with the same SSID. A full list drops its last network.
// Returns false for an empty or too long SSID, or a password of 1 to 7 or more than 64 characters.
bool addNetwork(NetworkList& l, const char* ssid, const char* password);
// Moves the network at index i to the first position, so that the next connection tries it first.
void promoteNetwork(NetworkList& l, int i);
void removeNetwork(NetworkList& l, int i);

// Writes the indexes of ids into order, in the saved order. saved is a list of ids with commas. The ids that saved does
// not list come next, in their own order. Unknown and repeated ids in saved have no effect.
void appOrder(const char* saved, const char* const* ids, int count, int* order);

}  // namespace sys
