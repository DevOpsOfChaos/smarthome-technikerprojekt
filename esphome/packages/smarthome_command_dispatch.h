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
