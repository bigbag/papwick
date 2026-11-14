#include "WifiSetup.h"

#include <BoardSupport.h>
#include <DNSServer.h>
#include <Logging.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_random.h>

#include <cstdio>

#include "Net.h"

namespace wifisetup {

namespace {

constexpr const char* TAG = "WIFISETUP";
constexpr const char* kUrl = "http://192.168.4.1";
constexpr uint32_t kReplyMs = 1500;
constexpr int kMaxFound = 16;

enum class Step : uint8_t { Scan, Connect, Lan, Join, Open, Saved, Sync, Failed };

Step step = Step::Scan;
board::wifi::Network found[kMaxFound];
int foundCount = 0;
char apName[20], apPass[9], lanUrl[32];
uint32_t savedAt = 0;
WebServer server(80);
// Answers all names with the device address. Then the phone sees a captive portal, opens the form page itself and
// does not move its traffic to mobile data.
DNSServer dns;

String escape(const char* s) {
  String out;
  for (; *s; ++s) {
    switch (*s) {
      case '&':
        out += "&amp;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      case '"':
        out += "&quot;";
        break;
      case '\'':
        out += "&#39;";
        break;
      default:
        out += *s;
    }
  }
  return out;
}

// The layout follows the papyrix-reader web page. The colors are the device colors: ui::kDark rows, ui::kGreen
// titles, ui::kAmber controls and ui::kRed errors.
constexpr const char* kStyle =
    "<style>*{box-sizing:border-box}"
    "body{margin:0;padding:24px 16px;background:#000;color:#fff;"
    "font:17px -apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif}"
    "main{max-width:26em;margin:auto}h2{color:#29ff42;font-weight:600;margin:0 0 16px}"
    ".card{background:#212121;border-radius:16px;padding:8px 16px 16px}"
    "label{display:block;color:#848484;font-size:.85em;margin:12px 0 6px}"
    "select,input{width:100%;padding:12px 16px;border:1px solid #424242;border-radius:24px;background:#000;"
    "color:#fff;font-size:1em}select:focus,input:focus{outline:none;border-color:#ffa600}"
    "button{width:100%;margin-top:20px;padding:12px;border:0;border-radius:24px;background:#ffa600;color:#000;"
    "font-size:1em;font-weight:600}p{line-height:1.5;margin:8px 0}.err{color:#ef1c21}a{color:#ffa600}</style>";

void sendPage(int code, const String& body) {
  server.send(code, "text/html",
              String("<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
                     "<title>Papwick Wi-Fi</title>") +
                  kStyle + "<main><h2>Papwick Wi-Fi</h2><div class=card>" + body + "</div></main>");
}

void handleForm() {
  LOG_INF(TAG, "HTTP form");
  String options;
  for (int i = 0; i < foundCount; ++i) {
    const String ssid = escape(found[i].ssid);
    options += "<option value=\"" + ssid + "\">" + ssid + (found[i].secured ? "" : " (open)") + "</option>";
  }
  sendPage(200, "<form method=post action=/save><label>Network</label><select name=s>" + options +
                    "</select><label>Other network name</label><input name=o maxlength=32>"
                    "<label>Password</label><input name=p type=password maxlength=64><button>Save</button></form>");
}

void handleSave() {
  const String other = server.arg("o");
  const String ssid = other.length() ? other : server.arg("s");
  if (!sys::addNetwork(net::networks(), ssid.c_str(), server.arg("p").c_str())) {
    sendPage(400,
             "<p class=err>Check the network name and the password: 8 to 64 characters, or empty for an open "
             "network.</p><p><a href=/>Back</a></p>");
    return;
  }
  net::saveNetworks();
  LOG_INF(TAG, "Saved network %s", ssid.c_str());
  sendPage(200, "<p>Saved. The device connects to the network now.</p>");
  step = Step::Saved;
  savedAt = millis();
}

void redirect() {
  LOG_INF(TAG, "HTTP redirect %s%s", server.hostHeader().c_str(), server.uri().c_str());
  server.sendHeader("Location", "http://" + server.client().localIP().toString() + "/", true);
  server.send(302, "text/plain", "");
}

Step startAp() {
  if (!board::wifi::startAp(apName, apPass)) return Step::Failed;
  if (!dns.start(53, "*", WiFi.softAPIP())) LOG_WRN(TAG, "DNS start failed");
  server.begin();
  return Step::Join;
}

void qr(const char* title, const char* data, const char* line) {
  constexpr int kSize = 230;
  ui::text(title, ui::cx(), 70, &fonts::DejaVu24, ui::kGreen);
  ui::canvas().qrcode(data, ui::cx() - kSize / 2, 100, kSize, 1, true);
  ui::text(line, ui::cx(), 360, &fonts::DejaVu18);
}

}  // namespace

void end() {
  net::cancel();
  dns.stop();
  server.stop();
  board::wifi::shutdown();
}

void begin() {
  // A running sync uses the radio that the scan restarts.
  net::cancel();
  const uint64_t mac = ESP.getEfuseMac();
  snprintf(apName, sizeof(apName), "Papwick-%04X", static_cast<unsigned>((mac >> 32) & 0xFFFF));
  snprintf(apPass, sizeof(apPass), "%08lu", static_cast<unsigned long>(esp_random() % 100000000));
  foundCount = 0;
  step = board::wifi::startScan() ? Step::Scan : Step::Failed;
  static bool routed = false;
  if (routed) return;
  routed = true;
  server.on("/", HTTP_GET, handleForm);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(redirect);
}

bool update(const ui::Input& in, bool& dirty) {
  const Step before = step;
  const bool ended = step == Step::Failed || (step == Step::Sync && !net::busy());
  if (in.blue || (in.green && ended)) {
    end();
    return false;
  }
  switch (step) {
    case Step::Scan:
      foundCount = board::wifi::scanResults(found, kMaxFound);
      if (foundCount < 0) break;
      if (net::networks().count > 0) {
        net::startSync(true);
        step = Step::Connect;
      } else {
        step = startAp();
      }
      break;
    case Step::Connect:
      if (board::wifi::connected()) {
        snprintf(lanUrl, sizeof(lanUrl), "http://%s", WiFi.localIP().toString().c_str());
        server.begin();
        step = Step::Lan;
      } else if (!net::busy()) {
        step = startAp();
      }
      break;
    case Step::Lan:
      server.handleClient();
      if (in.green) {
        end();
        step = startAp();
      }
      break;
    case Step::Join:
      server.handleClient();
      if (WiFi.softAPgetStationNum() > 0) step = Step::Open;
      break;
    case Step::Open:
      server.handleClient();
      break;
    case Step::Saved:
      server.handleClient();
      if (millis() - savedAt >= kReplyMs) {
        end();
        net::startSync();
        step = Step::Sync;
      }
      break;
    case Step::Sync: {
      static net::Sync shown = net::Sync::Idle;
      if (net::syncState() != shown) {
        shown = net::syncState();
        dirty = true;
      }
      break;
    }
    case Step::Failed:
      break;
  }
  if (step != before) dirty = true;
  return true;
}

void draw() {
  ui::clear();
  char line[48];
  switch (step) {
    case Step::Scan:
      ui::text("Looking for networks", ui::cx(), ui::cy(), &fonts::DejaVu24);
      break;
    case Step::Connect:
      ui::text("Connecting to", ui::cx(), ui::cy() - 20, &fonts::DejaVu24);
      ui::text(net::networks().items[0].ssid, ui::cx(), ui::cy() + 20, &fonts::DejaVu24, ui::kGreen);
      break;
    case Step::Lan:
      qr("Open on phone", lanUrl, lanUrl);
      ui::text(net::networks().items[0].ssid, ui::cx(), 388, &fonts::DejaVu18, ui::kGrey);
      ui::hints(ui::kHintsY, "cancel", "hotspot");
      return;
    case Step::Join:
      snprintf(line, sizeof(line), "WIFI:T:WPA;S:%s;P:%s;;", apName, apPass);
      qr("1. Join with phone", line, apName);
      snprintf(line, sizeof(line), "password %s", apPass);
      ui::text(line, ui::cx(), 388, &fonts::DejaVu18, ui::kGrey);
      break;
    case Step::Open:
      qr("2. Open the page", kUrl, kUrl);
      break;
    case Step::Saved:
    case Step::Sync:
      ui::text("Time sync", ui::cx(), ui::cy() - 30, &fonts::DejaVu24, ui::kGreen);
      ui::text(net::busy() || step == Step::Saved ? "connecting" : net::syncText(), ui::cx(), ui::cy() + 20,
               &fonts::DejaVu24);
      if (!net::busy() && step == Step::Sync) ui::hints(ui::kHintsY, nullptr, "done");
      return;
    case Step::Failed:
      ui::text("Wi-Fi start failed", ui::cx(), ui::cy(), &fonts::DejaVu24, ui::kRed);
      ui::hints(ui::kHintsY, nullptr, "done");
      return;
  }
  ui::hints(ui::kHintsY, "cancel", nullptr);
}

}  // namespace wifisetup
