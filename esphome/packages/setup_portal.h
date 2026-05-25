#pragma once

// =============================================================================
// setup_portal.h – Minimaler WiFi-AP für das ESPHome-Setup-Portal.
// Startet einen WPA2-gesicherten Access Point, damit der Nutzer die
// Master-MAC und Gerätekonfiguration per Webinterface eingeben kann.
// Nutzung: SmartHomeSetupPortal::start("SSID", "Passwort");
//          SmartHomeSetupPortal::stop();
// =============================================================================

#include <cstring>

#include "esp_wifi.h"

namespace SmartHomeSetupPortal {

// active wird nur aus dem ESPHome-Loop-Kontext aufgerufen (single-threaded).
// Kein Mutex noetig.
inline bool active = false;

inline void start(const char *ssid, const char *password) {
  if (active) {
    return;
  }

  wifi_config_t ap_config = {};
  std::strncpy(reinterpret_cast<char *>(ap_config.ap.ssid), ssid, sizeof(ap_config.ap.ssid) - 1);
  reinterpret_cast<char *>(ap_config.ap.ssid)[sizeof(ap_config.ap.ssid) - 1] = '\0';
  ap_config.ap.ssid_len = std::strlen(ssid);
  ap_config.ap.channel = 1;  // Kanal 1 (2412 MHz), minimales Kollisionsrisiko im Setup.
  ap_config.ap.max_connection = 4;  // Max. 4 Clients (ESP32-AP-Limit).
  if (password != nullptr && strlen(password) >= 8) {  // WPA2-Mindestlaenge 8 Zeichen, sonst Open.
    std::strncpy(reinterpret_cast<char *>(ap_config.ap.password), password, sizeof(ap_config.ap.password) - 1);
    reinterpret_cast<char *>(ap_config.ap.password)[sizeof(ap_config.ap.password) - 1] = '\0';
    ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
  } else {
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
  }

  esp_wifi_set_mode(WIFI_MODE_APSTA);
  esp_wifi_set_config(WIFI_IF_AP, &ap_config);
  esp_wifi_start();
  // Rueckgabewerte ignoriert: Setup-Portal ist best-effort.
  // Bei Fehlschlag bleibt active=true, aber WLAN-AP startet nicht.
  active = true;
}

inline void stop() {
  if (!active) {
    return;
  }

  esp_wifi_set_mode(WIFI_MODE_STA);
  active = false;
}

}  // namespace SmartHomeSetupPortal
