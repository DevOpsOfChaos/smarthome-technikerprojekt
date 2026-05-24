#pragma once

#include <cstring>

#include "esp_wifi.h"

namespace SmartHomeSetupPortal {

inline bool active = false;

inline void start(const char *ssid, const char *password) {
  if (active) {
    return;
  }

  wifi_config_t ap_config = {};
  std::strncpy(reinterpret_cast<char *>(ap_config.ap.ssid), ssid, sizeof(ap_config.ap.ssid));
  ap_config.ap.ssid_len = std::strlen(ssid);
  ap_config.ap.channel = 1;
  ap_config.ap.max_connection = 4;
  if (password != nullptr && strlen(password) >= 8) {
    std::strncpy(reinterpret_cast<char *>(ap_config.ap.password), password, sizeof(ap_config.ap.password));
    ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
  } else {
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
  }

  esp_wifi_set_mode(WIFI_MODE_APSTA);
  esp_wifi_set_config(WIFI_IF_AP, &ap_config);
  esp_wifi_start();
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
