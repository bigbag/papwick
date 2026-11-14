#pragma once

#include <Ui.h>

// Adds a Wi-Fi network from a phone. There is no on-screen keyboard: its keys are too small on the round screen.
// When a saved network answers, the device shows a QR code of its form page in that network. Otherwise it starts a
// hotspot and shows QR codes to join it and to open the form page.
namespace wifisetup {

void begin();
// Returns false when the setup ends. Blue cancels at each step.
bool update(const ui::Input& in, bool& dirty);
void draw();
// Stops the hotspot, the web page, the radio and a running sync.
void end();

}  // namespace wifisetup
