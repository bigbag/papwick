#include "WifiSetup.h"

#include <BoardSupport.h>
#include <DNSServer.h>
#include <I18n.h>
#include <Logging.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_random.h>

#include <cstdio>

#include "Net.h"
#include "Shell.h"

namespace wifisetup {

namespace {

constexpr const char* TAG = "WIFISETUP";
constexpr const char* kUrl = "http://192.168.4.1";
constexpr uint32_t kReplyMs = 1500;
constexpr int kMaxFound = 16;

// Named: the user changed only the name.
enum class Step : uint8_t { Scan, Connect, Lan, Join, Open, Saved, Named, Sync, Failed };

Step step = Step::Scan;
board::wifi::Network found[kMaxFound];
int foundCount = 0;
char apPass[9], lanUrl[32];
const char* apName = "";
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
                     "<title>Papwick</title>") +
                  kStyle + "<main><h2>Papwick</h2><div class=card>" + body + "</div></main>");
}

void handleForm() {
  LOG_INF(TAG, "HTTP form");
  // The empty option keeps the networks, so a name change alone does not save a network.
  String options =
      String("<option value=\"\">") + (net::networks().count > 0 ? "No change" : "Select a network") + "</option>";
  for (int i = 0; i < foundCount; ++i) {
    const String ssid = escape(found[i].ssid);
    options += "<option value=\"" + ssid + "\">" + ssid + (found[i].secured ? "" : " (open)") + "</option>";
  }
  sendPage(200, "<form method=post action=/save><label>Device name</label><input name=n maxlength=15 value=\"" +
                    escape(shell::device().name) + "\" placeholder=\"" + escape(board::wifi::deviceName()) +
                    "\"><label>Network</label><select name=s>" + options +
                    "</select><label>Other network name</label><input name=o maxlength=32>"
                    "<label>Password</label><input name=p type=password maxlength=64><button>Save</button></form>");
}

void handleSave() {
  String name = server.arg("n");
  name.trim();
  if (!sys::isValidName(name.c_str())) {
    sendPage(400,
             "<p class=err>The device name can have up to 15 Latin letters, digits, spaces and signs.</p>"
             "<p><a href=/>Back</a></p>");
    return;
  }
  const String other = server.arg("o");
  const String ssid = other.length() ? other : server.arg("s");
  // The live list changes only after the name is saved.
  sys::NetworkList list = net::networks();
  if (ssid.length() && !sys::addNetwork(list, ssid.c_str(), server.arg("p").c_str())) {
    sendPage(400,
             "<p class=err>Check the network name and the password: 8 to 64 characters, or empty for an open "
             "network.</p><p><a href=/>Back</a></p>");
    return;
  }
  sys::Device& d = shell::device();
  if (name != d.name) {
    snprintf(d.name, sizeof(d.name), "%s", name.c_str());
    // A failed write restores the old name, so a retry with the same name writes again.
    if (!shell::saveDevice()) {
      sendPage(500, "<p class=err>The device cannot save the name. Try again.</p><p><a href=/>Back</a></p>");
      return;
    }
    LOG_INF(TAG, "Name %s", shell::deviceName());
  }
  if (!ssid.length()) {
    sendPage(200, "<p>Saved.</p>");
    step = Step::Named;
    return;
  }
  net::networks() = list;
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
  apName = board::wifi::deviceName();
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
  const bool ended = step == Step::Failed || step == Step::Named || (step == Step::Sync && !net::busy());
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
    case Step::Named:
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
      ui::text(tr(LOOKING_FOR_NETWORKS), ui::cx(), ui::cy(), &fonts::DejaVu24);
      break;
    case Step::Connect:
      ui::text(tr(CONNECTING_TO), ui::cx(), ui::cy() - 20, &fonts::DejaVu24);
      ui::text(net::networks().items[0].ssid, ui::cx(), ui::cy() + 20, &fonts::DejaVu24, ui::kGreen);
      break;
    case Step::Lan:
      qr(tr(OPEN_ON_PHONE), lanUrl, lanUrl);
      ui::text(net::networks().items[0].ssid, ui::cx(), 388, &fonts::DejaVu18, ui::kGrey);
      ui::hints(ui::kHintsY, tr(CANCEL), tr(HOTSPOT));
      return;
    case Step::Join:
      snprintf(line, sizeof(line), "WIFI:T:WPA;S:%s;P:%s;;", apName, apPass);
      qr(tr(JOIN_WITH_PHONE), line, apName);
      snprintf(line, sizeof(line), tr(FMT_PASSWORD), apPass);
      ui::text(line, ui::cx(), 388, &fonts::DejaVu18, ui::kGrey);
      break;
    case Step::Open:
      qr(tr(OPEN_THE_PAGE), kUrl, kUrl);
      break;
    case Step::Saved:
    case Step::Sync:
      ui::text(tr(TIME_SYNC), ui::cx(), ui::cy() - 30, &fonts::DejaVu24, ui::kGreen);
      ui::text(net::busy() || step == Step::Saved ? tr(SYNC_CONNECTING) : net::syncText(), ui::cx(), ui::cy() + 20,
               &fonts::DejaVu24);
      if (!net::busy() && step == Step::Sync) ui::hints(ui::kHintsY, nullptr, tr(DONE));
      return;
    case Step::Named:
      ui::text(tr(SAVED), ui::cx(), ui::cy() - 20, &fonts::DejaVu24, ui::kGreen);
      ui::text(shell::deviceName(), ui::cx(), ui::cy() + 20, &fonts::DejaVu24);
      ui::hints(ui::kHintsY, nullptr, tr(DONE));
      return;
    case Step::Failed:
      ui::text(tr(WIFI_START_FAILED), ui::cx(), ui::cy(), &fonts::DejaVu24, ui::kRed);
      ui::hints(ui::kHintsY, nullptr, tr(DONE));
      return;
  }
  ui::hints(ui::kHintsY, tr(CANCEL), nullptr);
}

}  // namespace wifisetup
