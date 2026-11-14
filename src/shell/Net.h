#pragma once

#include <Device.h>

// Saved Wi-Fi networks and the NTP time sync. The sync turns the radio on and off again at the end.
namespace net {

enum class Sync : uint8_t { Idle, Connecting, Ntp, Done, Failed, NoNetwork };

void begin();
sys::NetworkList& networks();
void saveNetworks();
void clear();
// Tries the saved networks in list order, 15 s each, then asks NTP for 10 s. Then it turns the radio off,
// unless keepConnected is true and a network answered.
void startSync(bool keepConnected = false);
// Call it in each loop.
void updateSync();
// Stops a running sync and turns the radio off.
void cancel();
Sync syncState();
bool busy();
const char* syncText();

}  // namespace net
