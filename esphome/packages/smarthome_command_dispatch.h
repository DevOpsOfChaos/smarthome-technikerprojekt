// =============================================================================
// smarthome_command_dispatch.h
// =============================================================================
// Projekt:    Smarthome Technikerprojekt – alternative MQTT-Linie
// Zweck:      Gemeinsame Command-Dispatch-Funktionen fuer ESPHome-Lambdas
// Lizenz:     Proprietär – nur für Projektzwecke
// Pfad:       packages/smarthome_command_dispatch.h
//
// Datei-Funktion:
//   Stellt Hilfsfunktionen bereit, die den duplizierten Command-Handler-Code
//   aus allen Device-YAMLs eliminieren. Wird per esphome:includes eingebunden.
//
// Enthaltene Funktionen:
//   - smarthome_extract_request_id()    – request_id aus JSON extrahieren
//   - smarthome_validate_master_mac()   – MAC-Adresse im Format XX:XX:... validieren
//   - smarthome_is_hex()                – Hex-Ziffern-Prüfung
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-24
// Letzte Änderung: 2026-05-24
// =============================================================================

#pragma once

#include <string>
#include "esphome/core/log.h"

namespace smarthome {

// =============================================================================
// KANONISCHER COMMAND-HANDLER (Referenzimplementierung)
// =============================================================================
// Jedes Device-YAML implementiert diesen Handler im on_json_message-Lambda.
// Die markierten Abschnitte sind geraeteuebergreifend identisch:
//
//   on_json_message:
//     - topic: smarthome/device/${device_id}/command
//       then:
//         - lambda: |-
//             // === GEMEINSAM: request_id extrahieren ===
//             std::string request_id;
//             if (!smarthome::smarthome_extract_request_id(x, request_id)) {
//               id(contract_fault) = true;
//               id(contract_last_status_code) = -20;
//               id(contract_last_status_text) = "missing_request_id";
//               return;
//             }
//
//             std::string command = "";
//             if (x["command"].is<const char*>()) {
//               command = x["command"].as<std::string>();
//             }
//
//             // === GEMEINSAM: queue_ack-Lambda ===
//             auto queue_ack = [&](const char *status, int code, const char *source) {
//               id(ack_request_id) = request_id;
//               id(ack_status) = status;
//               id(ack_status_code) = code;
//               id(ack_source) = source;
//               id(flush_command_ack).execute();
//             };
//
//             // === GEMEINSAM: get_state ===
//             if (command == "get_state") {
//               id(clear_contract_fault).execute();
//               id(PUBLISH_STATE_SCRIPT).execute();
//               queue_ack("ok", 0, "node_ack");
//               return;
//             }
//
//             // === GEMEINSAM: set_config (master_mac) ===
//             if (command == "set_config") {
//               // ... master_mac-Validierung via smarthome::smarthome_validate_master_mac()
//             }
//
//             // === GERAETESPEZIFISCH: set_relay, calibrate, open, close, ... ===
//             // Hier folgen die pro Geraet unterschiedlichen Kommandos.
//
//             // === GEMEINSAM: unsupported-Fallback ===
//             id(contract_fault) = true;
//             id(contract_last_status_code) = -2;
//             id(contract_last_status_text) = "unsupported";
//             queue_ack("unsupported", -2, "master_validation");
//
// PUBLISH_STATE_SCRIPT ist pro Geraet:
//   net_erl_hall_module        → publish_net_erl_state
//   net_erl_hall_module_led_ring → publish_net_erl_led_state
//   net_sen_weather_station    → publish_net_sen_state
//   net_zrl_shutter_module     → publish_cover_contract_state
//   bat_sen_window_contact     → publish_bat_window_state
//   bat_sen_rain_sensor        → publish_bat_rain_state
// =============================================================================

// =============================================================================
// smarthome_extract_request_id – Extrahiert und validiert die request_id aus
// einem MQTT-Kommando-JSON.
//
// Rueckgabe: true wenn eine nicht-leere request_id gefunden wurde.
//            Bei false sollte der Aufrufer den Fehler -20 "missing_request_id" melden.
// =============================================================================
static inline bool smarthome_extract_request_id(JsonObjectConst x, std::string& request_id_out) {
    request_id_out.clear();
    if (x["request_id"].is<const char*>()) {
        request_id_out = x["request_id"].as<std::string>();
    }
    return !request_id_out.empty();
}

// =============================================================================
// smarthome_is_hex – Prueft ob ein Zeichen eine Hex-Ziffer ist (0-9, A-F, a-f).
// =============================================================================
static inline bool smarthome_is_hex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

// =============================================================================
// smarthome_validate_master_mac – Validiert eine MAC-Adresse im Format
// XX:XX:XX:XX:XX:XX (17 Zeichen, 6 Hex-Bytes durch Doppelpunkte getrennt).
//
// Rueckgabe: true wenn die MAC gueltig ist.
// =============================================================================
static inline bool smarthome_validate_master_mac(const std::string& mac) {
    if (mac.length() != 17) return false;
    for (size_t i = 0; i < mac.length(); i++) {
        if ((i + 1) % 3 == 0) {
            if (mac[i] != ':') return false;
        } else {
            if (!smarthome_is_hex(mac[i])) return false;
        }
    }
    return true;
}

// =============================================================================
// smarthome_handle_get_state – Standard-Behandlung fuer das get_state-Kommando.
// Setzt contract_fault zurueck und loest das publish-Skript aus.
//
// Parameter:
//   clear_fault_func   – Funktion/ID die contract_fault zuruecksetzt
//   publish_state_func – Funktion/ID die den Geraete-State publiziert
//
// Der Aufrufer muss danach selbst queue_ack("ok", 0, "node_ack") aufrufen.
// =============================================================================
// Hinweis: Diese Funktion wird als Lambda-Inline verwendet, nicht als C++-Funktion,
// da sie auf ESPHome-interne id()-Aufrufe angewiesen ist.
// Die Geräte-YAMLs nutzen stattdessen die Lambda-Inline-Variante.

} // namespace smarthome
