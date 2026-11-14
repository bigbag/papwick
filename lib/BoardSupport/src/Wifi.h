#pragma once

#include <cstdint>

// The radio uses about 50 KB of internal RAM and much battery current. Turn it on only for a task and call
// shutdown() at the end.
namespace board::wifi {

struct Network {
  char ssid[33];
  int8_t rssi;
  bool secured;
};

bool startScan();
// Returns -1 while the scan runs. Then writes the networks without duplicates, strongest first.
int scanResults(Network* out, int maxCount);

bool startConnect(const char* ssid, const char* password);
bool connected();

// Starts an access point at 192.168.4.1. The password needs 8 characters or more.
bool startAp(const char* ssid, const char* password);

void shutdown();

}  // namespace board::wifi
