#pragma once

#include <Device.h>
#include <Ui.h>

struct AppInfo {
  // The NVS namespace of the app data. 15 characters or less.
  const char* id;
  const char* name;
  uint16_t color;
  const char* glyph;
};

class App {
 public:
  virtual ~App() = default;
  virtual const AppInfo& info() const = 0;
  // Runs once at start-up, after the shell loads the device settings.
  virtual void boot() {}
  virtual void start() {}
  // Runs before the shell leaves the app and before power off.
  virtual void stop() {}
  virtual void update(const ui::Input& in) = 0;
  virtual void draw() = 0;
  // Runs each frame period when the screen is on and not dirty. Use it to push small sprites.
  virtual void animate() {}
  virtual bool keepAwake() const { return false; }
  // The app and Settings > Apps show the same rows. A tap changes the value, then the shell calls settingsChanged().
  virtual ui::Row* settings(int& count) {
    count = 0;
    return nullptr;
  }
  virtual void settingsChanged() {}
  // Clears the NVS namespace of the app. Override it to also reset the data in memory.
  virtual void clearData();
  bool dirty = true;
};

App& launcherApp();
App& settingsApp();

namespace shell {

void begin(App* const* apps, int count);
void loop();
void open(App& app);
void home();
// Opens Settings > Apps at the page of the app.
void openAppPage(App& app);
App* const* apps(int& count);
sys::Device& device();
// Returns false when the write fails.
bool saveDevice();
// Sends the brightness, volume, vibration and time zone to the hardware.
void applyDevice();
// Clears all saved data and restarts.
void resetAll();

}  // namespace shell
