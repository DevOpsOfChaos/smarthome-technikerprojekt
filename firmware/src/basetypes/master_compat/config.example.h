// =============================================================================
// config.example.h – Vorlage fuer master_compat-Konfiguration
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/basetypes/master_compat/config.example.h
//
// Datei-Funktion:
//   Vorlage fuer die lokale Konfigurationsdatei config.h.
//   WLAN-Zugangsdaten, MQTT-Broker-Adresse, Master-Identitaet.
//   Diese Datei NICHT als config.h in das oeffentliche Repo committen
//   (config.h liegt in .gitignore – Zugangsdaten bleiben lokal).
//   Verwendung: config.example.h nach config.h kopieren und Werte eintragen.
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
// =============================================================================

#pragma once

// ---- WiFi ----
#define CONF_WIFI_SSID     "DEIN_SSID"
#define CONF_WIFI_PASS     "DEIN_PASSWORT"

// ---- MQTT ----
#define CONF_MQTT_HOST     "192.168.1.X"
#define CONF_MQTT_PORT     1883
// Optional: MQTT-Auth (leer lassen wenn kein Auth)
#define CONF_MQTT_USER     ""
#define CONF_MQTT_PASS     ""

// ---- Master-Identitaet ----
#define CONF_MASTER_ID     "MASTER-001"
#define CONF_FW_VERSION    "1.0.0"

// ---- Stage-1-Reste ----
// In Stage 2 registriert sich die echte net_zrl-Node per HELLO ueber ESP-NOW.
// Diese Werte werden vom Stage-2-Codepfad nicht mehr verwendet.
#define CONF_NODE_ID       "NET-ZRL-901"
#define CONF_NODE_NAME     "Rolladen Wohnzimmer"

// ---- Entwicklungs-Hooks ----
// Stage 2 verwendet keine Simulationskommandos mehr.
#define CONF_ENABLE_SIM_HOOKS  1
