#pragma once

// =============================================================================
// setup_portal.h - Firmware-aehnliches Setup-Portal fuer ESPHome-Geraete.
// Nutzt ESPHomes Async-Webserver-Basis, damit der ESP32-C3/ESP-IDF-Buildpfad
// ohne Arduino-WebServer lauffaehig bleibt.
// =============================================================================

#include <cstring>
#include <functional>
#include <string>

#include "esp_wifi.h"
#include "esphome/components/web_server_idf/web_server_idf.h"

namespace SmartHomeSetupPortal {

using esphome::web_server_idf::AsyncWebHandler;
using esphome::web_server_idf::AsyncWebServer;
using esphome::web_server_idf::AsyncWebServerRequest;

inline bool active = false;
inline AsyncWebServer server(80);
inline std::string current_ssid;

using StringGetter = std::function<std::string()>;
using IntGetter = std::function<int()>;
using StringSetter = std::function<bool(const std::string &, std::string &)>;
using IntSetter = std::function<void(int)>;
using AppendHtml = std::function<void(std::string &, AsyncWebServerRequest *)>;
using SaveHandler = std::function<bool(AsyncWebServerRequest *, std::string &)>;
using ActionHandler = std::function<bool(AsyncWebServerRequest *, std::string &, std::string &, int &, bool &)>;

struct CommonConfig {
  const char *title = "Setup";
  const char *intro = "Geraet lokal konfigurieren.";
  const char *status_label = "status_interval";
  const char *sensor_label = "sensor_interval";
  int status_min = 5;
  int status_max = 65535;
  int sensor_min = 5;
  int sensor_max = 65535;
};

inline CommonConfig common;
inline StringGetter get_master_mac;
inline IntGetter get_status_interval;
inline IntGetter get_sensor_interval;
inline StringSetter set_master_mac;
inline IntSetter set_status_interval;
inline IntSetter set_sensor_interval;
inline AppendHtml append_device_fields;
inline AppendHtml append_device_actions;
inline SaveHandler save_device_fields;
inline ActionHandler handle_device_action;

inline std::string html_escape(const std::string &raw) {
  std::string out;
  out.reserve(raw.size() + 8);
  for (const char c : raw) {
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (c == '"') out += "&quot;";
    else if (c == '\'') out += "&#39;";
    else out += c;
  }
  return out;
}

inline bool parse_uint_field(const std::string &raw, int min_value, int max_value, int &out_value) {
  if (raw.empty()) return false;
  uint32_t value = 0;
  for (const char c : raw) {
    if (c < '0' || c > '9') return false;
    value = value * 10UL + (uint32_t) (c - '0');
    if (value > (uint32_t) max_value) return false;
  }
  if (value < (uint32_t) min_value) return false;
  out_value = (int) value;
  return true;
}

inline bool is_hex(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

inline bool validate_master_mac(const std::string &mac) {
  if (mac.size() != 17) return false;
  for (size_t i = 0; i < mac.size(); i++) {
    if ((i + 1) % 3 == 0) {
      if (mac[i] != ':') return false;
    } else if (!is_hex(mac[i])) {
      return false;
    }
  }
  return true;
}

inline int getter_int(const IntGetter &getter, int fallback) {
  return getter ? getter() : fallback;
}

inline std::string arg_or_empty(AsyncWebServerRequest *request, const char *name) {
  return request != nullptr && request->hasArg(name) ? request->arg(name) : std::string();
}

inline void append_shared_styles(std::string &page) {
  page += ":root{--bg:#070b14;--bg2:#0b1220;--card:#101827;--card2:#0c1320;--text:#edf3ff;--muted:#8ea0bf;--line:#1e2c45;--accent:#35c486;--accent2:#1d8a61;--danger:#ff6b6b;--danger2:#c94949;--ok:#91f0c5;--error:#ffb1b1;}";
  page += "*{box-sizing:border-box}html,body{margin:0;padding:0;min-height:100%;background:radial-gradient(circle at top,#15233d 0%,var(--bg) 56%,#04060d 100%);color:var(--text);font-family:\"Segoe UI\",Tahoma,sans-serif}";
  page += "body{padding:14px}.wrap{max-width:460px;margin:0 auto}.stack{display:grid;gap:12px}.card{background:linear-gradient(180deg,rgba(20,29,45,.96) 0%,rgba(12,19,32,.98) 100%);border:1px solid var(--line);border-radius:18px;padding:16px;box-shadow:0 18px 48px rgba(0,0,0,.34)}";
  page += ".eyebrow{font-size:.74rem;letter-spacing:.12em;text-transform:uppercase;color:var(--muted);margin-bottom:6px}.sub{margin:6px 0 0;color:var(--muted);font-size:.83rem;line-height:1.4}.status{display:grid;gap:4px;margin:14px 0 0;padding:11px 12px;border-radius:14px;border:1px solid #1f3a34;background:rgba(16,44,37,.9);color:var(--ok);font-size:.84rem;line-height:1.35}.status.error{border-color:#4d2428;background:rgba(60,19,22,.88);color:var(--error)}";
  page += ".status strong,.status code{color:var(--text)}h1{margin:0;font-size:1.28rem;line-height:1.2}h2{margin:0;font-size:.96rem}.section{display:grid;gap:12px;margin-top:16px}.section-head{display:flex;align-items:center;justify-content:space-between;gap:10px}.tag{display:inline-flex;align-items:center;padding:3px 8px;border-radius:999px;border:1px solid var(--line);background:rgba(255,255,255,.03);color:var(--muted);font-size:.72rem;text-transform:uppercase;letter-spacing:.08em}";
  page += ".field{display:grid;gap:6px}label{font-weight:700;font-size:.88rem;color:#d9e4f8}.hint{font-size:.76rem;line-height:1.3;color:var(--muted)}input,select{width:100%;min-height:44px;border-radius:12px;border:1px solid var(--line);background:#0a111d;color:var(--text);padding:0 12px;font-size:.96rem}input::placeholder{color:#617393}hr{border:0;border-top:1px solid var(--line);margin:16px 0 0}";
  page += ".actions{display:grid;gap:10px;margin-top:16px}.btn,.linkbtn{display:flex;align-items:center;justify-content:center;min-height:46px;padding:0 14px;border-radius:12px;border:1px solid transparent;font-size:.95rem;font-weight:700;text-decoration:none}.btn{width:100%;cursor:pointer}.btn-primary{background:linear-gradient(180deg,var(--accent) 0%,var(--accent2) 100%);color:#06140f}.btn-danger{background:linear-gradient(180deg,var(--danger) 0%,var(--danger2) 100%);color:#fff}.btn-secondary,.linkbtn{background:transparent;border-color:var(--line);color:var(--text)}";
  page += ".meta{display:grid;gap:4px;margin-top:10px;font-size:.78rem;color:var(--muted)}.meta code{color:var(--text)}.footer{margin-top:2px;font-size:.75rem;color:var(--muted)}";
}

inline void append_number_field(
    std::string &page, const char *id, const char *label, int min_value, int max_value, int value,
    const char *hint = nullptr) {
  page += "<div class=\"field\"><label for=\"";
  page += html_escape(id);
  page += "\">";
  page += html_escape(label);
  page += "</label><input id=\"";
  page += html_escape(id);
  page += "\" name=\"";
  page += html_escape(id);
  page += "\" type=\"number\" min=\"";
  page += std::to_string(min_value);
  page += "\" max=\"";
  page += std::to_string(max_value);
  page += "\" step=\"1\" inputmode=\"numeric\" value=\"";
  page += std::to_string(value);
  page += "\">";
  if (hint != nullptr) {
    page += "<div class=\"hint\">";
    page += html_escape(hint);
    page += "</div>";
  }
  page += "</div>";
}

inline std::string build_page(const std::string &info_text, const std::string &error_text,
                              AsyncWebServerRequest *source_request) {
  const std::string master_mac =
      source_request != nullptr && source_request->hasArg("master_mac")
          ? source_request->arg("master_mac")
          : (get_master_mac ? get_master_mac() : std::string());
  const int status_interval =
      source_request != nullptr && source_request->hasArg(common.status_label)
          ? atoi(source_request->arg(common.status_label).c_str())
          : getter_int(get_status_interval, common.status_min);
  const int sensor_interval =
      source_request != nullptr && source_request->hasArg(common.sensor_label)
          ? atoi(source_request->arg(common.sensor_label).c_str())
          : getter_int(get_sensor_interval, common.sensor_min);

  std::string page;
  page.reserve(12000);
  page += "<!doctype html><html lang=\"de\"><head><meta charset=\"utf-8\">";
  page += "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,viewport-fit=cover\">";
  page += "<title>" + html_escape(common.title) + "</title><style>";
  append_shared_styles(page);
  page += "</style></head><body><div class=\"wrap stack\"><form class=\"card\" method=\"post\" action=\"/save\" id=\"setupForm\" novalidate>";
  page += "<div class=\"eyebrow\">Provisioning</div><h1>" + html_escape(common.title) + "</h1>";
  page += "<div class=\"sub\">" + html_escape(common.intro) + "</div>";
  if (!error_text.empty()) {
    page += "<div class=\"status error\">" + html_escape(error_text) + "</div>";
  } else {
    page += "<div class=\"status\">" + html_escape(info_text);
    if (active) {
      page += "<span>AP <strong>" + html_escape(current_ssid) + "</strong></span>";
      page += "<span>URL <code>http://192.168.4.1/</code></span>";
    }
    page += "</div>";
  }

  page += "<section class=\"section\"><div class=\"section-head\"><h2>Node-Basis</h2><span class=\"tag\">global</span></div>";
  page += "<div class=\"field\"><label for=\"master_mac\">master_mac</label><input id=\"master_mac\" name=\"master_mac\" type=\"text\" maxlength=\"17\" autocapitalize=\"characters\" autocomplete=\"off\" spellcheck=\"false\" placeholder=\"AA:BB:CC:DD:EE:FF\" value=\"";
  page += html_escape(master_mac);
  page += "\"><div class=\"hint\">Auch per <code>?master_mac=...</code> oder <code>?mac=...</code>.</div></div>";
  append_number_field(page, common.status_label, common.status_label, common.status_min, common.status_max, status_interval);
  append_number_field(page, common.sensor_label, common.sensor_label, common.sensor_min, common.sensor_max, sensor_interval);

  if (append_device_fields) {
    page += "<hr><div class=\"section-head\"><h2>Geraet</h2><span class=\"tag\">device</span></div>";
    append_device_fields(page, source_request);
  }
  page += "<div class=\"actions\"><button class=\"btn btn-primary\" type=\"submit\">Speichern</button>";
  page += "<div class=\"footer\">Nach dem Speichern bleiben die Werte lokal im Geraet erhalten.</div></div></section></form>";
  if (append_device_actions) append_device_actions(page, source_request);
  page += "</div></body></html>";
  return page;
}

inline void send_html(AsyncWebServerRequest *request, int status_code, const std::string &html) {
  request->send(request->beginResponse(status_code, "text/html; charset=utf-8", html));
}

inline void send_page(AsyncWebServerRequest *request, const std::string &info_text, const std::string &error_text,
                      int status_code, AsyncWebServerRequest *source_request) {
  send_html(request, status_code, build_page(info_text, error_text, source_request));
}

inline void send_result_page(AsyncWebServerRequest *request, const std::string &title, const std::string &message,
                             bool error, int status_code) {
  std::string page;
  page.reserve(4200);
  page += "<!doctype html><html lang=\"de\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1,viewport-fit=cover\"><title>";
  page += html_escape(title);
  page += "</title><style>";
  append_shared_styles(page);
  page += "</style></head><body><div class=\"wrap stack\"><div class=\"card\"><div class=\"eyebrow\">Provisioning</div><h1>";
  page += html_escape(title);
  page += "</h1><div class=\"status";
  if (error) page += " error";
  page += "\">";
  page += html_escape(message);
  page += "</div><div class=\"actions\"><a class=\"linkbtn\" href=\"/\">Zurueck</a></div></div></div></body></html>";
  send_html(request, status_code, page);
}

inline void handle_root(AsyncWebServerRequest *request) {
  std::string query_mac;
  if (request->hasArg("master_mac")) query_mac = request->arg("master_mac");
  else if (request->hasArg("mac")) query_mac = request->arg("mac");

  if (!query_mac.empty()) {
    if (validate_master_mac(query_mac) && set_master_mac) {
      std::string err;
      if (set_master_mac(query_mac, err)) {
        send_page(request, "Master-MAC aus Query uebernommen.", "", 200, nullptr);
        return;
      }
    }
    send_page(request, "", "Die uebergebene Master-MAC ist ungueltig.", 200, nullptr);
    return;
  }
  send_page(request, "Setup-Portal aktiv.", "", 200, nullptr);
}

inline void handle_save(AsyncWebServerRequest *request) {
  if (request->hasArg("device_action")) {
    std::string title;
    std::string message;
    int status_code = 400;
    bool restart_required = false;
    const bool ok = handle_device_action &&
                    handle_device_action(request, title, message, status_code, restart_required);
    send_result_page(
        request, !title.empty() ? title : (ok ? "Aktion gespeichert" : "Aktion fehlgeschlagen"),
        !message.empty() ? message : (ok ? "Aktion ausgefuehrt." : "Aktion ist fehlgeschlagen."), !ok, status_code);
    return;
  }

  const std::string master_mac = request->arg("master_mac");
  if (!validate_master_mac(master_mac)) {
    send_result_page(request, "Eingabe ungueltig", "master_mac ist ungueltig. Erwartet wird AA:BB:CC:DD:EE:FF.", true, 400);
    return;
  }

  int status_interval = 0;
  if (!parse_uint_field(request->arg(common.status_label), common.status_min, common.status_max, status_interval)) {
    send_result_page(request, "Eingabe ungueltig", std::string(common.status_label) + " ist ungueltig.", true, 400);
    return;
  }

  int sensor_interval = 0;
  if (!parse_uint_field(request->arg(common.sensor_label), common.sensor_min, common.sensor_max, sensor_interval)) {
    send_result_page(request, "Eingabe ungueltig", std::string(common.sensor_label) + " ist ungueltig.", true, 400);
    return;
  }

  std::string device_error;
  if (save_device_fields && !save_device_fields(request, device_error)) {
    send_result_page(request, "Eingabe ungueltig", device_error, true, 400);
    return;
  }

  std::string mac_error;
  if (set_master_mac && !set_master_mac(master_mac, mac_error)) {
    send_result_page(request, "Speichern fehlgeschlagen", mac_error, true, 500);
    return;
  }
  if (set_status_interval) set_status_interval(status_interval);
  if (set_sensor_interval) set_sensor_interval(sensor_interval);

  send_result_page(request, "Setup gespeichert", "Einstellungen wurden lokal gespeichert.", false, 200);
}

class SetupHandler : public AsyncWebHandler {
 public:
  bool canHandle(AsyncWebServerRequest *request) const override {
    char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
    const auto url = request->url_to(url_buf);
    return url == "/" || url == "/save";
  }
  void handleRequest(AsyncWebServerRequest *request) override {
    char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
    const auto url = request->url_to(url_buf);
    if (url == "/save" && request->method() == HTTP_POST) {
      handle_save(request);
    } else {
      handle_root(request);
    }
  }
};

inline SetupHandler setup_handler;
inline bool handler_added = false;

inline void configure_common(
    const CommonConfig &config, StringGetter master_getter, StringSetter master_setter,
    IntGetter status_getter, IntSetter status_setter, IntGetter sensor_getter, IntSetter sensor_setter) {
  common = config;
  get_master_mac = master_getter;
  set_master_mac = master_setter;
  get_status_interval = status_getter;
  set_status_interval = status_setter;
  get_sensor_interval = sensor_getter;
  set_sensor_interval = sensor_setter;
}

inline void configure_device(AppendHtml fields, SaveHandler save, AppendHtml actions = nullptr,
                             ActionHandler action = nullptr) {
  append_device_fields = fields;
  save_device_fields = save;
  append_device_actions = actions;
  handle_device_action = action;
}

inline void clear_device() {
  append_device_fields = nullptr;
  save_device_fields = nullptr;
  append_device_actions = nullptr;
  handle_device_action = nullptr;
}

inline bool start(const char *ssid, const char *password) {
  if (active) return true;

  wifi_config_t ap_config = {};
  std::strncpy(reinterpret_cast<char *>(ap_config.ap.ssid), ssid, sizeof(ap_config.ap.ssid) - 1);
  reinterpret_cast<char *>(ap_config.ap.ssid)[sizeof(ap_config.ap.ssid) - 1] = '\0';
  ap_config.ap.ssid_len = std::strlen(ssid);
  ap_config.ap.channel = 1;
  ap_config.ap.max_connection = 4;
  if (password != nullptr && strlen(password) >= 8) {
    std::strncpy(reinterpret_cast<char *>(ap_config.ap.password), password, sizeof(ap_config.ap.password) - 1);
    reinterpret_cast<char *>(ap_config.ap.password)[sizeof(ap_config.ap.password) - 1] = '\0';
    ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
  } else {
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
  }

  if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK) return false;
  if (esp_wifi_set_config(WIFI_IF_AP, &ap_config) != ESP_OK) return false;
  if (esp_wifi_start() != ESP_OK) return false;
  current_ssid = ssid != nullptr ? ssid : "";
  if (!handler_added) {
    server.addHandler(&setup_handler);
    handler_added = true;
  }
  server.begin();
  active = true;
  return true;
}

inline void handle_client() {
  // No-op: AsyncWebServer wird intern von ESP-IDF/Arduino-Tasks bedient.
}

inline void stop() {
  if (!active) return;
  server.end();
  esp_wifi_set_mode(WIFI_MODE_STA);
  active = false;
  current_ssid.clear();
}

}  // namespace SmartHomeSetupPortal
