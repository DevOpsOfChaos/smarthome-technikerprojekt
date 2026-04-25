#pragma once

// ============================================================
// master_compat/config.h
// Lokale Konfiguration – NICHT in öffentliches Repo committen!
// Eintrag in .gitignore: firmware/src/basetypes/master_compat/config.h
// Vorlage: Diese Datei als config.h kopieren.
// ============================================================

// ---- WiFi ----
#define CONF_WIFI_SSID     "DEIN_SSID"
#define CONF_WIFI_PASS     "DEIN_PASSWORT"

// ---- MQTT ----
#define CONF_MQTT_HOST     "192.168.1.X"
#define CONF_MQTT_PORT     1883
// Optional: MQTT-Auth (leer lassen wenn kein Auth)
#define CONF_MQTT_USER     ""
#define CONF_MQTT_PASS     ""

// ---- Master-Identität ----
#define CONF_MASTER_ID     "MASTER-001"
#define CONF_FW_VERSION    "1.0.0"

// ---- Stage-1-Reste ----
// In Stage 2 registriert sich die echte net_zrl-Node per HELLO über ESP-NOW.
// Diese Werte werden vom Stage-2-Codepfad nicht mehr verwendet.
#define CONF_NODE_ID       "NET-ZRL-901"
#define CONF_NODE_NAME     "Rolladen Wohnzimmer"

// ---- Entwicklungs-Hooks ----
// Stage 2 verwendet keine Simulationskommandos mehr.
#define CONF_ENABLE_SIM_HOOKS  1
