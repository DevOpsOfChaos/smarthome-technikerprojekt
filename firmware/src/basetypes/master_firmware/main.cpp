/*
===============================================================================
 Datei: main.cpp
 Code-Name: Master Firmware
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / Basistyp / ESP-NOW-MQTT-Master
 Ersteller: DevOpsOfChaos
 Datum: 2026-05-14
 Letzte Bearbeitung: 2026-05-18

 Zweck: Master-Firmware als Bruecke zwischen ESP-NOW-Nodes und MQTT
 Beschreibung: Diese Firmware verwaltet die dynamische Node-Registry, nimmt
 HELLO, HEARTBEAT, STATE, EVENT und ACK per ESP-NOW entgegen und veroeffentlicht
 daraus Meta-, Availability-, State-, Event- und ACK-Meldungen per MQTT.
 Eingehende MQTT-Kommandos wie set_relay, get_state, set_config, open, close,
 stop und set_position werden validiert, als ESP-NOW-Befehl an die passende Node
 gesendet und ueber Pending-/ACK-Logik nachverfolgt.

 Protokoll:
 - ESP-NOW empfangen: HELLO, HEARTBEAT, STATE, EVENT, ACK.
 - ESP-NOW senden: HELLO_ACK, CMD, CFG und STATE_REQUEST.
 - MQTT senden: smarthome/master/{DEVICE_ID}/{status,event}.
 - MQTT senden: smarthome/device/{node_id}/{meta,availability,state,ack,event}.
 - MQTT empfangen: smarthome/device/{node_id}/command.

 Wichtige Begrenzungen:
 - MAX_DYNAMIC_NODES begrenzt die dynamische Node-Registry.
 - COMMAND_ACK_TIMEOUT_MS und COMMAND_MAX_RETRIES steuern die Retry-Logik.
 - NODE_OFFLINE_TIMEOUT_MS und BATTERY_NODE_OFFLINE_TIMEOUT_MS trennen Netz-
   und Batterie-Nodes bei der Online-Ueberwachung.
 - MQTT_BUFFER_BYTES begrenzt eingehende MQTT-Nachrichten.

 Genutzte Bibliotheken:
 - Arduino.h: Arduino-Grundfunktionen wie millis, delay und Serial.
 - WiFi.h: WLAN-Anbindung des Masters.
 - PubSubClient.h: MQTT-Client fuer Broker-Kommunikation.
 - esp_now.h und esp_wifi.h: ESP-NOW-Funk und WLAN-Kanalsteuerung.
 - esp_task_wdt.h: Watchdog fuer die Master-Hauptschleife.
 - AppConfig.h: eigene Master-Konfiguration mit Profilen, Timern und Limits.
 - PinConfig.h: eigene Pin-Konfiguration fuer optionale Master-Hardware.
 - Protocol.h und DeviceTypes.h: eigenes SmartHome-Funkprotokoll.
 - ShStorage.h: gemeinsame Grenzen fuer konfigurierbare Node-Werte.

 Aenderungsverlauf:
 - 2026-05-14: Master-Firmware mit dynamischer Node-Registry angelegt.
 - 2026-05-18: Kommentarstil an Device-Referenz angepasst und alte Metakommentare entfernt.
===============================================================================
*/

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_task_wdt.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <stddef.h>

#if __has_include(<esp_arduino_version.h>)
  #include <esp_arduino_version.h>
#endif

#ifndef ESP_ARDUINO_VERSION_MAJOR
  #define ESP_ARDUINO_VERSION_MAJOR 2
#endif

#include "AppConfig.h"
#include "PinConfig.h"
#include "../../../include/ProjectVersion.h"
#include "../../../include/DebugConfig.h"
#include "../../../lib/sh_protocol/src/Protocol.h"
#include "../../../lib/sh_protocol/src/DeviceTypes.h"
#include "../../../lib/sh_storage/src/ShStorage.h"

#if __has_include("../../../include/Secrets.h")
  #include "../../../include/Secrets.h"
#else
  #warning "Keine Secrets.h gefunden. Bitte aus Secrets.example.h erstellen."
  #define WIFI_SSID         "KEIN_SSID"
  #define WIFI_PASSWORD     "KEIN_PASSWORT"
  #define MQTT_HOST         "127.0.0.1"
  #define MQTT_PORT         1883
  #define MQTT_USER         "mqtt_user"
  #define MQTT_PASSWORD     "KEIN_MQTT_PASSWORT"
#endif

#ifndef MQTT_USER
  #define MQTT_USER ""
#endif

#ifndef MQTT_PASSWORD
  #define MQTT_PASSWORD ""
#endif

// =============================================================================
// LAYOUT-KOMPATIBILITAET - Statische Pruefung der Payload-Cast-Annahmen
// =============================================================================
// An mehreren Stellen wird ein Config-State-Payload auf den kleineren State-Typ
// gecastet, weil der Master nur die State-Felder liest (Config-Felder stehen am Ende).
// Das setzt voraus, dass relay_1 und andere Felder am selben Offset liegen.
// Diese static_asserts fangen Protokollaenderungen beim Kompilieren ab.

static_assert(
    offsetof(SmartHome::RelayComfortConfigStateReportPayload, relay_1) ==
    offsetof(SmartHome::RelayComfortStateReportPayload, relay_1),
    "RelayComfortConfigState und RelayComfortState haben unterschiedliche relay_1-Offsets");

static_assert(
    offsetof(SmartHome::RelayComfortConfigStateReportPayload, fault) ==
    offsetof(SmartHome::RelayComfortStateReportPayload, fault),
    "RelayComfortConfigState und RelayComfortState haben unterschiedliche fault-Offsets");

static_assert(
    offsetof(SmartHome::ZrlConfigStateReportPayload, relay_1) ==
    offsetof(SmartHome::ZrlStateReportPayload, relay_1),
    "ZrlConfigState und ZrlState haben unterschiedliche relay_1-Offsets");

static_assert(
    offsetof(SmartHome::ZrlConfigStateReportPayload, fault) ==
    offsetof(SmartHome::ZrlStateReportPayload, fault),
    "ZrlConfigState und ZrlState haben unterschiedliche fault-Offsets");

static_assert(
    offsetof(SmartHome::ExtendedSensorConfigStateReportPayload, temp_01c) ==
    offsetof(SmartHome::ExtendedSensorStateReportPayload, temp_01c),
    "ExtendedSensorConfigState und ExtendedSensorState haben unterschiedliche temp_01c-Offsets");

static_assert(
    offsetof(SmartHome::ExtendedRelayComfortGasConfigStateReportPayload, relay_1) ==
    offsetof(SmartHome::ExtendedRelayComfortGasStateReportPayload, relay_1),
    "ExtendedRelayComfortGasConfigState und ExtendedRelayComfortGasState haben unterschiedliche relay_1-Offsets");

static_assert(
    sizeof(SmartHome::ExtendedRelayComfortConfigStateReportPayload) ==
    sizeof(SmartHome::ExtendedRelayComfortGasStateReportPayload),
    "ExtendedRelayComfortConfigState und ExtendedRelayComfortGasState haben unterschiedliche Groessen "
    "(Annahme der 41-Byte-Ambiguitaet verletzt)");

namespace {

// =============================================================================
// KONSTANTEN - Debug, Version, MQTT-Topics, Request-ID, Status-Codes, Ungueltig
// =============================================================================

constexpr bool DEBUG_LOKAL_AKTIV = DEVICE_DEBUG_AKTIV && DEBUG_AKTIV;
constexpr char DATEI_GERAET[] = "MASTER";
constexpr char DATEI_VERSION[] = "0.4.0";
constexpr int MASTER_WDT_TIMEOUT_S = 10;
// MQTT-Topic fuer eingehende Device-Kommandos (+ = Wildcard fuer device_id)
constexpr char MQTT_TOPIC_COMMAND_SUB[] = "smarthome/device/+/command";
// Maximale Laenge der request_id (aus MQTT-Kommando)
constexpr size_t REQUEST_ID_LEN = 96U;
// Grenzen fuer report_interval_s aus dem Storage-Framework
constexpr long CFG_REPORT_INTERVAL_MIN = (long)SmartHome::ShStorage::SH_STORED_REPORT_INTERVAL_MIN_S;
constexpr long CFG_REPORT_INTERVAL_MAX = (long)SmartHome::ShStorage::SH_STORED_REPORT_INTERVAL_MAX_S;
constexpr unsigned long HELLO_REQUEST_RETRY_INTERVAL_MS = 30000UL;
// Bewusst gleich BATTERY_NODE_OFFLINE_TIMEOUT_MS: eine vollstaendige Schlafphase als Gnadenfrist.
// Aenderungen an BATTERY_NODE_OFFLINE_TIMEOUT_MS ziehen diese TTL automatisch mit.
constexpr unsigned long PROVISIONAL_NODE_TTL_MS = BATTERY_NODE_OFFLINE_TIMEOUT_MS;

// Ungueltigkeits-Marker fuer STATE-Payload-Felder (wenn Sensor nicht vorhanden)
constexpr uint32_t NET_SEN_PRESSURE_UNGUELTIG = 0xFFFFFFFFUL;
constexpr uint32_t NET_SEN_GAS_OHM_UNGUELTIG = 0xFFFFFFFFUL;
constexpr uint16_t NET_SEN_AIR_METRIC_UNGUELTIG = 0xFFFFU;
constexpr uint8_t BATTERY_PCT_UNGUELTIG = 0xFFU;
// 0xFFFF (65535 mV) ist physikalisch unmoeglich und sicherer Sentinel.
// 0U waere ein gueltiger Messwert (entladene Batterie) und darf nicht als Ungueltig erscheinen.
constexpr uint16_t BATTERY_MV_UNGUELTIG = 0xFFFFU;
constexpr uint8_t WINDOW_STATE_UNGUELTIG = 0xFFU;
constexpr uint16_t RAIN_RAW_UNGUELTIG = 0xFFFFU;
constexpr uint8_t COVER_POSITION_UNBEKANNT = 0xFFU;

// Fehler-Codes fuer MQTT-ACK-Antworten
constexpr int STATUS_CODE_NOT_CALIBRATED = -5;     // set_position ohne Kalibrierung
constexpr int STATUS_CODE_UNKNOWN_DEVICE = -6;      // device_id nicht in Registry
constexpr int STATUS_CODE_REGISTRY_FULL  = -7;      // Registry voll (max. 16 Nodes)
constexpr int STATUS_CODE_NO_ROUTE       = -8;      // MAC unbekannt, Retry nicht moeglich
constexpr int STATUS_CODE_META_REQUIRED  = -9;      // HELLO-Meta fehlt fuer sichere Validierung
constexpr int STATUS_CODE_INVALID_PAYLOAD = -21;    // MQTT-Payload ungueltig

// Registry-Persistenz im ESP32-NVS. Gespeichert werden nur stabile Meta-Daten
// aus HELLO, niemals Laufzeitwerte wie online, Pending-CMDs oder Sensorzustaende.
constexpr char REGISTRY_PREF_NAMESPACE[] = "master_reg";
constexpr char REGISTRY_PREF_KEY[] = "nodes_v1";
constexpr uint32_t REGISTRY_PERSIST_MAGIC = 0x53485231UL; // "SHR1"
constexpr uint16_t REGISTRY_PERSIST_VERSION = 1U;

// =============================================================================
// STRUKTUREN - Pending-Nachrichten, Node-Registry, Master-Status
// =============================================================================

// PendingHeader: Gemeinsame Felder fuer ausstehende ACK-pflichtige Nachrichten.
struct PendingHeader {
    bool aktiv;                          // true = Pending laeuft
    uint8_t seq;                         // ESP-NOW-Sequenznummer
    uint8_t retries;                     // Retry-Zaehler
    unsigned long letztes_senden_ms;     // Zeitstempel letzter Sendevorgang
    char request_id[REQUEST_ID_LEN];     // request_id aus MQTT-Kommando (fuer ACK)
    char command_channel[24];            // MQTT-Channel (fuer ACK-Routing)
};

// PendingCmdRequest: Ausstehende CMD-Nachricht, deren ACK noch nicht eingetroffen ist.
struct PendingCmdRequest {
    PendingHeader hdr;                   // Gemeinsame Pending-Felder (aktiv, seq, retries, ...)
    uint8_t cmd_type;                    // Kommando-Typ (SH_CMD_SET_RELAY, SH_CMD_COVER, ...)
    uint8_t param1;                      // Parameter 1 (z.B. Relais-Index)
    uint8_t param2;                      // Parameter 2 (z.B. Ziel-Zustand)
};

// PendingConfigRequest: Ausstehende CFG-Nachricht, deren ACK noch nicht eingetroffen ist.
struct PendingConfigRequest {
    PendingHeader hdr;                   // Gemeinsame Pending-Felder (aktiv, seq, retries, ...)
    uint8_t param_id;                    // Konfig-Parameter-ID (z.B. SH_CFG_REPORT_INTERVAL_S)
    uint16_t value;                      // Konfig-Wert
};

// NodeRuntime: Laufzeitzustand einer einzelnen Node in der Registry.
//   Jede Node hat einen Slot mit Zustand, Sensorwerten, MAC und Pending-Requests.
struct NodeRuntime {
    bool belegt;                         // Slot belegt (true = Node registriert)
    bool meta_bekannt;                   // HELLO wurde empfangen (Meta vorhanden)
    bool meta_wiederhergestellt;         // Meta stammt aus NVS und wartet auf frisches HELLO
    bool provisorisch;                   // Slot wurde ohne HELLO aus STATE/HEARTBEAT angelegt
    bool online;                         // Node antwortet (Heartbeat/Kontakt in Timeout)
    bool state_bekannt;                  // Mindestens ein STATE empfangen
    bool mac_bekannt;                    // MAC-Adresse bekannt
    bool fault;                          // Fehlerstatus der Node
    uint8_t auto_flags;                  // Auto-Light-Status-Bitmaske
    bool relay_1;                        // Relais 1 Zustand
    bool relay_2;                        // Relais 2 Zustand
    bool cover_mode;                     // Cover-Modus aktiv
    bool cover_calibrated;               // Cover kalibriert
    uint8_t cover_state;                 // Cover-State-Code (SH_COVER_STATE_*)
    uint8_t cover_position;              // Cover-Position (0-100, 255=unbekannt)
    int16_t temp_01c;                    // Temperatur in Zehntel-Grad
    uint16_t hum_01pct;                  // Feuchte in Zehntel-Prozent
    uint16_t lux;                        // Lux-Wert
    uint32_t pressure_pa;                // Luftdruck in Pascal
    uint32_t gas_ohm;                    // Gaswiderstand in Ohm
    uint16_t aqi;                        // Luftqualitaetsindex
    uint16_t tvoc_ppb;                   // TVOC in ppb
    uint16_t eco2_ppm;                   // eCO2 in ppm
    bool motion;                         // Bewegung erkannt
    uint8_t battery_pct;                 // Batteriestand in Prozent
    uint16_t battery_mv;                 // Batteriespannung in mV
    uint8_t window_open;                 // Fensterkontakt (0/1, 255=unbekannt)
    uint16_t rain_raw;                   // Regen-ADC-Rohwert
    uint8_t button_flags;                // Tastenflags
    uint32_t uptime_s;                   // Uptime der Node in Sekunden
    uint16_t caps;                       // Faehigkeiten (Bitmaske)
    uint16_t fw_version;                 // Firmware-Version
    uint8_t device_class;                // Geraeteklasse (SH_CLASS_NET_ERL, ...)
    uint8_t power_type;                  // Stromversorgung (mains/battery)
    uint8_t meta_schema_version;         // Meta-Daten-Schema-Version
    uint8_t control_mode;                // Steuerungsmodus
    uint8_t config_profile;              // Konfigurationsprofil
    uint8_t reporting_mode;              // Report-Modus
    char device_id[SH_DEVICE_ID_LEN];    // Geraete-ID
    char sensor_mask[SH_SENSOR_MASK_LEN];// Sensor-Maske
    char input_mask[SH_INPUT_MASK_LEN];  // Input-Maske
    char device_name[SH_DEVICE_NAME_LEN];// Geraete-Name
    uint8_t mac[6];                      // MAC-Adresse der Node
    unsigned long letzter_kontakt_ms;    // Zeitstempel letzte Kommunikation
    unsigned long letzter_hello_request_ms; // Zeitstempel letzter HELLO_REQUEST
    PendingCmdRequest pending_cmd;       // Ausstehendes CMD (falls aktiv)
    PendingConfigRequest pending_cfg;    // Ausstehendes CFG (falls aktiv)
};

// PersistedNodeSlot: Flash-Abbild der HELLO-Meta einer Node.
// Dieses Format ist absichtlich klein und versioniert, damit ein Master-Neustart
// die bekannten Capabilities wiederherstellen kann, ohne alte Runtime-Zustaende
// faelschlich als aktuell zu behandeln.
struct PersistedNodeSlot {
    uint8_t belegt;                      // 1 = Slot enthaelt gespeicherte Node-Meta
    uint8_t mac_bekannt;                 // 1 = MAC-Adresse wurde aus HELLO/Kontakt gespeichert
    uint8_t device_class;                // Geraeteklasse aus HELLO
    uint8_t power_type;                  // Versorgungstyp aus HELLO
    uint16_t caps;                       // Capability-Bitmaske aus HELLO
    uint16_t fw_version;                 // Firmware-Version aus HELLO
    uint8_t meta_schema_version;         // Meta-Schema-Version aus HELLO
    uint8_t control_mode;                // Steuerungsmodus aus HELLO
    uint8_t config_profile;              // Konfigurationsprofil aus HELLO
    uint8_t reporting_mode;              // Report-Modus aus HELLO
    char device_id[SH_DEVICE_ID_LEN];    // Geraete-ID
    char sensor_mask[SH_SENSOR_MASK_LEN];// Sensor-Maske
    char input_mask[SH_INPUT_MASK_LEN];  // Input-Maske
    char device_name[SH_DEVICE_NAME_LEN];// Anzeigename
    uint8_t mac[6];                      // letzte bekannte MAC-Adresse
};

// PersistedRegistry: Gesamtes Flash-Abbild der dynamischen Registry.
// magic/version schuetzen vor fremden oder alten Datenlayouts.
struct PersistedRegistry {
    uint32_t magic;                      // Kennung fuer gueltiges SmartHome-Registry-Blob
    uint16_t version;                    // Layout-Version dieser Struktur
    uint16_t slot_count;                 // Anzahl gespeicherter Slots
    PersistedNodeSlot slots[MAX_DYNAMIC_NODES]; // Meta-Slots in fester Reihenfolge
};

// MasterState: Zentraler Master-Status fuer Verbindungen und Sequenznummern.
struct MasterState {
    bool wlan_verbunden;                 // true = WLAN verbunden
    bool mqtt_verbunden;                 // true = MQTT verbunden
    bool espnow_bereit;                  // true = ESP-NOW initialisiert
    unsigned long letzter_wlan_versuch_ms; // Zeitstempel letzter WLAN-Connect-Versuch
    unsigned long letzter_mqtt_versuch_ms; // Zeitstempel letzter MQTT-Connect-Versuch
    uint8_t naechste_seq;                // Naechste ESP-NOW-Sequenznummer
};

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
IPAddress mqttBrokerIp;
bool mqttBrokerNutzeDirekteIp = false;
Preferences registryPrefs;
MasterState masterStatus = {};
NodeRuntime nodeStates[MAX_DYNAMIC_NODES] = {};

// Log-Level-Konvention:
// INFO  – Normalbetrieb, Verbindungsaufbau, State-Wechsel
// WARN  – Transiente Fehler (Retry moeglich), unerwartete Payloads
// ERROR – Kritische Fehler (ESP-NOW/MQTT-Init fehlgeschlagen, persistenter Funkverlust)

// Aufgabe: Schreibt formatierte Debugmeldungen auf die serielle Schnittstelle.
// Eingabewerte:
// - level ist die Log-Stufe als Text.
// - format und Folgewerten bilden die printf-artige Meldung.
// Ausgabewert: keiner; bei deaktiviertem Debug wird nichts ausgegeben.
void logf(const char* level, const char* format, ...) {
    if (!DEBUG_LOKAL_AKTIV) return;
    char message[256];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    Serial.print("[");
    Serial.print(level);
    Serial.print("] ");
    Serial.println(message);
}

// Aufgabe: Kopiert Text sicher in einen Zielpuffer und terminiert immer mit Nullbyte.
// Eingabewerte:
// - target zeigt auf den Zielpuffer.
// - targetSize ist die Zielpuffergroesse.
// - source ist der Quelltext oder nullptr.
// Ausgabewert: keiner; leere oder ungueltige Quellen ergeben einen leeren Zieltext.
void copyText(char* target, size_t targetSize, const char* source) {
    if (!target || targetSize == 0U) return;
    if (!source) {
        target[0] = '\0';
        return;
    }
    strncpy(target, source, targetSize - 1U);
    target[targetSize - 1U] = '\0';
}

// Aufgabe: Escaped Text fuer sichere Einbettung in JSON-Stringwerte.
// Eingabewerte:
// - target zeigt auf den Zielpuffer.
// - targetSize ist die Zielpuffergroesse.
// - source ist der rohe Text oder nullptr.
// Ausgabewert: keiner; target enthaelt danach einen JSON-sicheren Stringinhalt.
void jsonEscapeText(char* target, size_t targetSize, const char* source) {
    if (!target || targetSize == 0U) return;
    target[0] = '\0';
    if (!source) return;

    size_t out = 0U;
    for (const char* in = source; *in != '\0' && out + 1U < targetSize; ++in) {
        const char c = *in;
        const char* replacement = nullptr;
        switch (c) {
            case '"': replacement = "\\\""; break;
            case '\\': replacement = "\\\\"; break;
            case '\b': replacement = "\\b"; break;
            case '\f': replacement = "\\f"; break;
            case '\n': replacement = "\\n"; break;
            case '\r': replacement = "\\r"; break;
            case '\t': replacement = "\\t"; break;
            default: break;
        }

        if (replacement != nullptr) {
            for (const char* r = replacement; *r != '\0' && out + 1U < targetSize; ++r) {
                target[out++] = *r;
            }
            continue;
        }

        if ((unsigned char)c < 0x20U) {
            if (out + 1U < targetSize) target[out++] = ' ';
            continue;
        }
        target[out++] = c;
    }
    target[out] = '\0';
}

// Aufgabe: Wandelt eine MAC-Adresse in lesbaren Text um.
// Eingabewerte:
// - mac zeigt auf 6 MAC-Bytes oder ist nullptr.
// - buffer zeigt auf den Ausgabepuffer.
// - bufferSize ist die Ausgabepuffergroesse.
// Ausgabewert: keiner; bei fehlender MAC wird "unbekannt" geschrieben.
void macText(const uint8_t* mac, char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0U) return;
    if (!mac) {
        copyText(buffer, bufferSize, "unbekannt");
        return;
    }
    char local[18] = {0};
    SmartHome::macToString(mac, local);
    copyText(buffer, bufferSize, local);
}

// Aufgabe: Wandelt eine MAC in einen lesbaren String (AA:BB:CC:DD:EE:FF).
// Nutzt einen thread-lokalen Puffer – Aufrufer muss das Ergebnis sofort
// kopieren, falls es ueber mehrere Aufrufe hinweg benoetigt wird.
static const char* macToText(const uint8_t* mac) {
    static char buf[18];
    macText(mac, buf, sizeof(buf));
    return buf;
}

// Aufgabe: Beschreibt, ob der MQTT-Broker als IP-Adresse oder Hostname genutzt wird.
// Eingabewerte: keine.
// Ausgabewert: "ip" oder "host" fuer Logs und Statusausgaben.
const char* mqttBrokerTypText() {
    return mqttBrokerNutzeDirekteIp ? "ip" : "host";
}

// =============================================================================
// HILFSFUNKTIONEN - Device-Klasse, Power-Typ, Cover-Status, Control-Modus
// =============================================================================

// Aufgabe: Wandelt eine Device-Klasse in den MQTT-/Log-Text um.
// Eingabewert: deviceClass ist ein SH_CLASS_*-Wert.
// Ausgabewert: Text wie "net_erl", "net_zrl", "net_sen", "bat_sen" oder "unknown".
const char* deviceClassText(uint8_t deviceClass) {
    switch (deviceClass) {
        case SH_CLASS_NET_ERL: return "net_erl";
        case SH_CLASS_NET_ZRL: return "net_zrl";
        case SH_CLASS_NET_SEN: return "net_sen";
        case SH_CLASS_BAT_SEN: return "bat_sen";
        default: return "unknown";
    }
}

// Aufgabe: Wandelt den Versorgungstyp in lesbaren Text um.
// Eingabewert: powerType ist SH_POWER_MAINS oder SH_POWER_BATTERY.
// Ausgabewert: "battery" fuer Batterie-Nodes, sonst "mains".
const char* powerTypeText(uint8_t powerType) {
    return powerType == SH_POWER_BATTERY ? "battery" : "mains";
}

const char* nodePowerTypeText(size_t nodeIndex) {
    if (!nodeStates[nodeIndex].meta_bekannt) return "unknown";
    return powerTypeText(nodeStates[nodeIndex].power_type);
}

// Aufgabe: Wandelt den Control-Mode in den MQTT-/Meta-Text um.
// Eingabewert: controlMode ist ein SH_CONTROL_MODE_*-Wert.
// Ausgabewert: Text wie "relay", "relay_light", "cover" oder "none".
const char* controlModeText(uint8_t controlMode) {
    switch (controlMode) {
        case SH_CONTROL_MODE_RELAY: return "relay";
        case SH_CONTROL_MODE_RELAY_LIGHT: return "relay_light";
        case SH_CONTROL_MODE_DUAL_RELAY: return "dual_relay";
        case SH_CONTROL_MODE_DUAL_RELAY_LIGHT: return "dual_relay_light";
        case SH_CONTROL_MODE_COVER: return "cover";
        case SH_CONTROL_MODE_NONE:
        default: return "none";
    }
}

// Aufgabe: Wandelt das Konfigurationsprofil in den MQTT-/Meta-Text um.
// Eingabewert: configProfile ist ein SH_PROFILE_*-Wert.
// Ausgabewert: Profiltext oder "none".
const char* configProfileText(uint8_t configProfile) {
    switch (configProfile) {
        case SH_PROFILE_HALL_LIGHT: return "hall_light";
        case SH_PROFILE_COVER_BASIC: return "cover_basic";
        case SH_PROFILE_HALL_MODULE_LED_RING: return "hall_module_led_ring";
        case SH_PROFILE_NONE:
        default: return "none";
    }
}

// Aufgabe: Wandelt den Reporting-Modus in den MQTT-/Meta-Text um.
// Eingabewert: reportingMode ist ein SH_REPORTING_*-Wert.
// Ausgabewert: Reporting-Text oder "unknown".
const char* reportingModeText(uint8_t reportingMode) {
    switch (reportingMode) {
        case SH_REPORTING_PERIODIC: return "periodic";
        case SH_REPORTING_EVENT_DRIVEN: return "event_driven";
        case SH_REPORTING_HYBRID: return "hybrid";
        case SH_REPORTING_SLEEP_PERIODIC: return "sleep_periodic";
        case SH_REPORTING_SLEEP_EVENT: return "sleep_event";
        default: return "unknown";
    }
}

// Aufgabe: Leitet den lesbaren Cover-Status aus State-Code, Kalibrierung und Position ab.
// Eingabewerte:
// - coverStateCode ist der gemeldete SH_COVER_STATE_*-Wert.
// - calibrated zeigt, ob die Node Positionswerte belastbar melden kann.
// - position ist 0 bis 100 oder unbekannt.
// Ausgabewert: "opening", "closing", "open", "closed" oder "stopped".
const char* coverStateText(uint8_t coverStateCode, bool calibrated, uint8_t position) {
    switch (coverStateCode) {
        case SH_COVER_STATE_MOVING_UP: return "opening";
        case SH_COVER_STATE_MOVING_DOWN: return "closing";
        case SH_COVER_STATE_STOPPED:
        default:
            if (calibrated && position == 100U) return "open";
            if (calibrated && position == 0U) return "closed";
            return "stopped";
    }
}

// Aufgabe: Prueft, ob eine Cover-Position im gueltigen Prozentbereich liegt.
// Eingabewert: position ist der Positionswert aus Node-State oder Kommando.
// Ausgabewert: true bedeutet 0 bis 100 Prozent.
bool istCoverPositionBekannt(uint8_t position) {
    return position <= 100U;
}

// Aufgabe: Prueft, ob eine empfangene Device-Klasse vom Master verarbeitet wird.
// Eingabewert: deviceClass ist der HELLO-Wert der Node.
// Ausgabewert: true bedeutet, die Klasse ist fuer die Registry zugelassen.
bool istDeviceClassGueltig(uint8_t deviceClass) {
    switch (deviceClass) {
        case SH_CLASS_NET_ERL:
        case SH_CLASS_NET_ZRL:
        case SH_CLASS_NET_SEN:
        case SH_CLASS_BAT_SEN:
            return true;
        default:
            return false;
    }
}

// Aufgabe: Prueft, ob der Versorgungstyp gueltig ist.
// Eingabewert: powerType ist der HELLO-Wert der Node.
// Ausgabewert: true bedeutet mains oder battery.
bool istPowerTypeGueltig(uint8_t powerType) {
    return powerType == SH_POWER_MAINS || powerType == SH_POWER_BATTERY;
}

// Aufgabe: Liefert den passenden Offline-Timeout je nach Versorgungstyp.
// Eingabewert: powerType ist der Versorgungstyp der Node.
// Ausgabewert: Timeout in Millisekunden fuer Batterie- oder Netz-Nodes.
unsigned long offlineTimeoutMsForPowerType(uint8_t powerType) {
    return powerType == SH_POWER_BATTERY ? BATTERY_NODE_OFFLINE_TIMEOUT_MS : NODE_OFFLINE_TIMEOUT_MS;
}

// =============================================================================
// NODE-REGISTRY - Initialisierung, Suche (per ID/MAC/frei), Cap-Pruefung
// =============================================================================

// Aufgabe: Setzt einen Registry-Slot auf definierte Start- und Ungueltigkeitswerte.
// Eingabewert: node ist der zu initialisierende Registry-Slot.
// Ausgabewert: keiner; alle Felder sind danach fuer eine neue Node vorbereitet.
void initialisiereNodeSlot(NodeRuntime& node) {
    node = {};
    node.cover_state = SH_COVER_STATE_STOPPED;
    node.cover_position = COVER_POSITION_UNBEKANNT;
    node.temp_01c = INT16_MIN;
    node.hum_01pct = 0xFFFFU;
    node.lux = 0xFFFFU;
    node.pressure_pa = NET_SEN_PRESSURE_UNGUELTIG;
    node.gas_ohm = NET_SEN_GAS_OHM_UNGUELTIG;
    node.aqi = NET_SEN_AIR_METRIC_UNGUELTIG;
    node.tvoc_ppb = NET_SEN_AIR_METRIC_UNGUELTIG;
    node.eco2_ppm = NET_SEN_AIR_METRIC_UNGUELTIG;
    node.battery_pct = BATTERY_PCT_UNGUELTIG;
    node.battery_mv = BATTERY_MV_UNGUELTIG;
    node.window_open = WINDOW_STATE_UNGUELTIG;
    node.rain_raw = RAIN_RAW_UNGUELTIG;
    node.meta_schema_version = SH_META_SCHEMA_VERSION_CURRENT;
    node.control_mode = SH_CONTROL_MODE_NONE;
    node.config_profile = SH_PROFILE_NONE;
    node.reporting_mode = SH_REPORTING_HYBRID;
    copyText(node.sensor_mask, sizeof(node.sensor_mask), "XXXXXXXXXX");
    copyText(node.input_mask, sizeof(node.input_mask), "XXXXX");
    copyText(node.device_name, sizeof(node.device_name), "unknown");
}

// Aufgabe: Initialisiert die gesamte dynamische Node-Registry.
// Eingabewerte: keine.
// Ausgabewert: keiner; alle Registry-Slots sind danach leer und definiert vorbelegt.
void initialisiereNodeStates() {
    for (size_t i = 0; i < MAX_DYNAMIC_NODES; ++i) {
        initialisiereNodeSlot(nodeStates[i]);
    }
}

// Vorwaertsdeklaration, weil die Persistenz beim Laden bereits State-Felder
// anhand der gespeicherten Capabilities auf sichere Defaults begrenzt.
void sanitisiereNodeStateNachCapabilities(size_t nodeIndex);

// Aufgabe: Kopiert einen Runtime-Slot in das persistente Flash-Format.
// Eingabewerte:
// - node ist der aktuelle Runtime-Slot.
// - slot ist der Zielslot fuer das NVS-Abbild.
// Ausgabewert: keiner; nicht belegte oder unvollstaendige Nodes bleiben leer.
void fuellePersistSlotAusNode(const NodeRuntime& node, PersistedNodeSlot& slot) {
    slot = {};
    if (!node.belegt || !node.meta_bekannt) return;

    // Nur HELLO-Meta wird persistiert. Online, State, Pending und Sensorwerte
    // bleiben fluechtig, weil sie nach einem Neustart nicht mehr wahr sein muessen.
    slot.belegt = 1U;
    slot.mac_bekannt = node.mac_bekannt ? 1U : 0U;
    slot.device_class = node.device_class;
    slot.power_type = node.power_type;
    slot.caps = node.caps;
    slot.fw_version = node.fw_version;
    slot.meta_schema_version = node.meta_schema_version;
    slot.control_mode = node.control_mode;
    slot.config_profile = node.config_profile;
    slot.reporting_mode = node.reporting_mode;
    copyText(slot.device_id, sizeof(slot.device_id), node.device_id);
    copyText(slot.sensor_mask, sizeof(slot.sensor_mask), node.sensor_mask);
    copyText(slot.input_mask, sizeof(slot.input_mask), node.input_mask);
    copyText(slot.device_name, sizeof(slot.device_name), node.device_name);
    if (node.mac_bekannt) memcpy(slot.mac, node.mac, sizeof(slot.mac));
}

// Aufgabe: Schreibt die bekannte Node-Meta-Registry in den Flash.
// Eingabewert: grund beschreibt den Ausloeser fuer Logmeldungen.
// Ausgabewert: true bedeutet, das NVS-Blob wurde erfolgreich geschrieben.
bool persistiereRegistrySnapshot(const char* grund) {
    PersistedRegistry snapshot = {};
    snapshot.magic = REGISTRY_PERSIST_MAGIC;
    snapshot.version = REGISTRY_PERSIST_VERSION;
    snapshot.slot_count = (uint16_t)MAX_DYNAMIC_NODES;

    unsigned int gespeicherteSlots = 0U;
    for (size_t i = 0; i < MAX_DYNAMIC_NODES; ++i) {
        fuellePersistSlotAusNode(nodeStates[i], snapshot.slots[i]);
        if (snapshot.slots[i].belegt != 0U) gespeicherteSlots++;
    }

    if (!registryPrefs.begin(REGISTRY_PREF_NAMESPACE, false)) {
        logf("WARN", "Registry-Persistenz: NVS konnte nicht geoeffnet werden");
        return false;
    }
    // Kein CRC: magic+version reichen als Integritaetspruefung.
    // NVS-Schreibzugriffe sind atomar pro Key; ein teilweise geschriebener
    // Blob wird bei inkonsistenter Laenge von ladeRegistrySnapshot() verworfen.
    const size_t written = registryPrefs.putBytes(REGISTRY_PREF_KEY, &snapshot, sizeof(snapshot));
    registryPrefs.end();

    if (written != sizeof(snapshot)) {
        logf("WARN", "Registry-Persistenz: Schreiben fehlgeschlagen (%u/%u Bytes)",
             (unsigned)written,
             (unsigned)sizeof(snapshot));
        return false;
    }
    logf("INFO", "Registry-Persistenz: %u Node-Meta-Slots gespeichert (%s)",
         gespeicherteSlots,
         grund ? grund : "snapshot");
    return true;
}

// Aufgabe: Prueft, ob ein gespeicherter Slot plausibel genug zum Wiederherstellen ist.
// Eingabewert: slot ist der NVS-Slot.
// Ausgabewert: true bedeutet, device_id, Klasse und Versorgungstyp sind gueltig.
bool istPersistSlotGueltig(const PersistedNodeSlot& slot) {
    if (slot.belegt == 0U) return false;
    if (!SmartHome::isValidDeviceId(slot.device_id)) return false;
    if (!istDeviceClassGueltig(slot.device_class)) return false;
    if (!istPowerTypeGueltig(slot.power_type)) return false;
    if (slot.mac_bekannt != 0U && !SmartHome::isValidMac(slot.mac)) return false;
    // device_name muss mindestens ein Zeichen haben (wird vom HELLO immer gesetzt).
    if (slot.device_name[0] == '\0') return false;
    return true;
}

// Aufgabe: Stellt einen Runtime-Slot aus gespeicherter HELLO-Meta wieder her.
// Eingabewerte:
// - slot ist der persistierte NVS-Slot.
// - node ist der Zielslot in nodeStates[].
// Ausgabewert: keiner; Runtime-Felder bleiben fuer "frisch gebootet" konservativ.
void stelleNodeAusPersistSlotWiederHer(const PersistedNodeSlot& slot, NodeRuntime& node) {
    initialisiereNodeSlot(node);
    node.belegt = true;
    node.meta_bekannt = true;
    node.meta_wiederhergestellt = true;
    node.provisorisch = false;
    node.online = false;
    node.state_bekannt = false;
    node.mac_bekannt = slot.mac_bekannt != 0U;
    node.device_class = slot.device_class;
    node.power_type = slot.power_type;
    node.caps = slot.caps;
    node.fw_version = slot.fw_version;
    node.meta_schema_version = slot.meta_schema_version;
    node.control_mode = slot.control_mode;
    node.config_profile = slot.config_profile;
    node.reporting_mode = slot.reporting_mode;
    copyText(node.device_id, sizeof(node.device_id), slot.device_id);
    copyText(node.sensor_mask, sizeof(node.sensor_mask), slot.sensor_mask);
    copyText(node.input_mask, sizeof(node.input_mask), slot.input_mask);
    copyText(node.device_name, sizeof(node.device_name), slot.device_name);
    if (node.mac_bekannt) {
        memcpy(node.mac, slot.mac, sizeof(node.mac));
        // Der ESP-NOW-Peer wird erst beim tatsaechlichen Senden angelegt, weil
        // die Registry vor der ESP-NOW-Initialisierung aus dem Flash geladen wird.
    }
}

// Aufgabe: Laedt gespeicherte Node-Meta aus dem Flash in die Runtime-Registry.
// Eingabewerte: keine.
// Ausgabewert: true bedeutet, ein gueltiges Registry-Blob wurde gelesen.
bool ladeRegistrySnapshot() {
    if (!registryPrefs.begin(REGISTRY_PREF_NAMESPACE, true)) {
        logf("WARN", "Registry-Persistenz: NVS konnte nicht gelesen werden");
        return false;
    }
    const size_t blobLen = registryPrefs.getBytesLength(REGISTRY_PREF_KEY);
    if (blobLen != sizeof(PersistedRegistry)) {
        registryPrefs.end();
        if (blobLen != 0U) {
            logf("WARN", "Registry-Persistenz: unerwartete Blob-Laenge %u", (unsigned)blobLen);
        }
        return false;
    }

    PersistedRegistry snapshot = {};
    const size_t read = registryPrefs.getBytes(REGISTRY_PREF_KEY, &snapshot, sizeof(snapshot));
    registryPrefs.end();
    if (read != sizeof(snapshot) ||
        snapshot.magic != REGISTRY_PERSIST_MAGIC ||
        snapshot.version != REGISTRY_PERSIST_VERSION ||
        snapshot.slot_count != (uint16_t)MAX_DYNAMIC_NODES) {
        logf("WARN", "Registry-Persistenz: Blob ungueltig oder falsche Version");
        return false;
    }

    unsigned int geladeneSlots = 0U;
    for (size_t i = 0; i < MAX_DYNAMIC_NODES; ++i) {
        if (!istPersistSlotGueltig(snapshot.slots[i])) continue;
        stelleNodeAusPersistSlotWiederHer(snapshot.slots[i], nodeStates[i]);
        sanitisiereNodeStateNachCapabilities(i);
        geladeneSlots++;
    }
    logf("INFO", "Registry-Persistenz: %u Node-Meta-Slots geladen", geladeneSlots);
    return geladeneSlots > 0U;
}

// Aufgabe: Prueft, ob eine Node eine bestimmte Capability meldet.
// Eingabewerte:
// - nodeIndex ist der Index in nodeStates[].
// - cap ist das zu pruefende SH_CAP_*-Bit.
// Ausgabewert: true bedeutet, die Capability ist in caps gesetzt.
bool nodeHasCap(size_t nodeIndex, uint16_t cap) {
    return (nodeStates[nodeIndex].caps & cap) != 0U;
}

// Aufgabe: Sucht eine registrierte Node ueber ihre device_id.
// Eingabewert: nodeId ist die gesuchte Geraete-ID.
// Ausgabewert: Node-Index oder -1, wenn keine passende belegte Node existiert.
int findeNodeIndex(const char* nodeId) {
    if (!nodeId) return -1;
    for (size_t i = 0; i < MAX_DYNAMIC_NODES; ++i) {
        if (!nodeStates[i].belegt) continue;
        if (strncmp(nodeStates[i].device_id, nodeId, SH_DEVICE_ID_LEN) == 0) {
            return (int)i;
        }
    }
    return -1;
}

// Aufgabe: Sucht eine registrierte Node ueber ihre MAC-Adresse.
// Eingabewert: mac zeigt auf die gesuchte 6-Byte-MAC-Adresse.
// Ausgabewert: Node-Index oder -1, wenn keine passende bekannte MAC existiert.
int findeNodeIndexPerMac(const uint8_t* mac) {
    if (!mac) return -1;
    for (size_t i = 0; i < MAX_DYNAMIC_NODES; ++i) {
        if (!nodeStates[i].belegt || !nodeStates[i].mac_bekannt) continue;
        if (memcmp(nodeStates[i].mac, mac, 6) == 0) {
            return (int)i;
        }
    }
    return -1;
}

// Aufgabe: Findet den ersten freien Registry-Slot.
// Eingabewerte: keine.
// Ausgabewert: freier Node-Index oder -1, wenn die Registry voll ist.
int findeFreienNodeIndex() {
    for (size_t i = 0; i < MAX_DYNAMIC_NODES; ++i) {
        if (!nodeStates[i].belegt) return (int)i;
    }
    return -1;
}

// Aufgabe: Baut die 16-Bit-Capability-Maske aus dem HELLO-Payload.
// Eingabewert: payload ist der empfangene HELLO-Payload.
// Ausgabewert: Capability-Bitmaske aus caps_hi und caps_lo.
uint16_t holeHelloCaps(const SmartHome::HelloPayload& payload) {
    return (uint16_t)(((uint16_t)payload.caps_hi << 8) | payload.caps_lo);
}

// Aufgabe: Setzt NET-SEN-Zusatzwerte einer Node auf unbekannt.
// Eingabewert: nodeIndex ist der Index in nodeStates[].
// Ausgabewert: keiner; Druck, Gas und Luftqualitaetswerte sind danach ungueltig markiert.
void setzeNetSenZusatzwerteUnbekannt(size_t nodeIndex) {
    // Basis-Sensor-States enthalten diese Felder nicht. Sie werden deshalb
    // bewusst auf Unknown gesetzt, damit MQTT keine alten Extended-/Gas-Werte
    // als aktuelle Messwerte weitertraegt.
    nodeStates[nodeIndex].pressure_pa = NET_SEN_PRESSURE_UNGUELTIG;
    nodeStates[nodeIndex].gas_ohm = NET_SEN_GAS_OHM_UNGUELTIG;
    nodeStates[nodeIndex].aqi = NET_SEN_AIR_METRIC_UNGUELTIG;
    nodeStates[nodeIndex].tvoc_ppb = NET_SEN_AIR_METRIC_UNGUELTIG;
    nodeStates[nodeIndex].eco2_ppm = NET_SEN_AIR_METRIC_UNGUELTIG;
}

// Aufgabe: Entfernt State-Werte, die nicht zu den gemeldeten Capabilities passen.
// Eingabewert: nodeIndex ist der Index in nodeStates[].
// Ausgabewert: keiner; nicht unterstuetzte Werte werden auf neutrale oder unbekannte Werte gesetzt.
void sanitisiereNodeStateNachCapabilities(size_t nodeIndex) {
    // Ohne HELLO-Caps waere jede Bereinigung geraten. Provisorische Slots
    // behalten ihren Minimalzustand, bis ein echtes HELLO die Capabilities liefert.
    if (!nodeStates[nodeIndex].meta_bekannt) return;

    // Alles, was die Node nicht als Capability gemeldet hat, wird neutralisiert.
    // So kann ein alter State-Wert nach Firmware-/Profilwechsel nicht weiterleben.
    if (!nodeHasCap(nodeIndex, SH_CAP_RELAY)) nodeStates[nodeIndex].relay_1 = false;
    if (!nodeHasCap(nodeIndex, SH_CAP_RELAY2)) nodeStates[nodeIndex].relay_2 = false;
    // INT16_MIN = Sentinel "kein gueltiger Messwert" (Temperatur in Zehntelgrad).
    if (!nodeHasCap(nodeIndex, SH_CAP_TEMP)) nodeStates[nodeIndex].temp_01c = INT16_MIN;
    if (!nodeHasCap(nodeIndex, SH_CAP_HUM)) nodeStates[nodeIndex].hum_01pct = 0xFFFFU;
    if (!nodeHasCap(nodeIndex, SH_CAP_LUX)) nodeStates[nodeIndex].lux = 0xFFFFU;
    if (!nodeHasCap(nodeIndex, SH_CAP_PRESSURE)) nodeStates[nodeIndex].pressure_pa = NET_SEN_PRESSURE_UNGUELTIG;
    if (!nodeHasCap(nodeIndex, SH_CAP_AQI)) {
        // AQI, TVOC und eCO2 werden gemeinsam als Luftqualitaetsblock behandelt.
        nodeStates[nodeIndex].aqi = NET_SEN_AIR_METRIC_UNGUELTIG;
        nodeStates[nodeIndex].tvoc_ppb = NET_SEN_AIR_METRIC_UNGUELTIG;
        nodeStates[nodeIndex].eco2_ppm = NET_SEN_AIR_METRIC_UNGUELTIG;
    }
    if (!nodeHasCap(nodeIndex, SH_CAP_MOTION)) nodeStates[nodeIndex].motion = false;
    if (!nodeHasCap(nodeIndex, SH_CAP_WINDOW)) nodeStates[nodeIndex].window_open = WINDOW_STATE_UNGUELTIG;
    if (!nodeHasCap(nodeIndex, SH_CAP_RAIN)) nodeStates[nodeIndex].rain_raw = RAIN_RAW_UNGUELTIG;
    if (!nodeHasCap(nodeIndex, SH_CAP_BATTERY)) {
        nodeStates[nodeIndex].battery_pct = BATTERY_PCT_UNGUELTIG;
        nodeStates[nodeIndex].battery_mv = BATTERY_MV_UNGUELTIG;
    }
    if (!nodeHasCap(nodeIndex, SH_CAP_COVER)) {
        // Nicht-Cover-Geraete duerfen keine alten Cover-Werte behalten, sonst
        // koennte die UI falsche Rolladensteuerung anzeigen.
        nodeStates[nodeIndex].cover_mode = false;
        nodeStates[nodeIndex].cover_state = SH_COVER_STATE_STOPPED;
        nodeStates[nodeIndex].cover_position = COVER_POSITION_UNBEKANNT;
        nodeStates[nodeIndex].cover_calibrated = false;
    }

    // auto_flags zuruecksetzen fuer Geraeteklassen ohne Auto-Light (nur NET_ERL).
    if (nodeStates[nodeIndex].device_class != SH_CLASS_NET_ERL) {
        nodeStates[nodeIndex].auto_flags = 0U;
    }
}

// Aufgabe: Liefert den Availability-Text fuer MQTT.
// Eingabewert: nodeIndex ist der Index in nodeStates[].
// Ausgabewert: "online", "asleep", "late" oder "offline".
const char* availabilityStateText(size_t nodeIndex) {
    if (nodeStates[nodeIndex].online) {
        return "online";
    }
    const unsigned long letzterKontakt = nodeStates[nodeIndex].letzter_kontakt_ms;
    if (letzterKontakt == 0UL) return "offline";
    const unsigned long delta = millis() - letzterKontakt;
    const unsigned long timeout = offlineTimeoutMsForPowerType(nodeStates[nodeIndex].power_type);
    if (nodeStates[nodeIndex].power_type == SH_POWER_BATTERY) {
        // Batterie-Nodes schlafen planmaessig. "asleep" ist deshalb kein Fehler,
        // sondern der erwartete Zustand innerhalb des normalen Meldefensters.
        if (delta <= timeout) return "asleep";
        // "late" trennt verzoegerte Batteriemeldungen von echtem Offline.
        if (delta <= (timeout * 2UL)) return "late";
        return "offline";
    }
    return delta <= timeout ? "late" : "offline";
}

// Aufgabe: Stellt sicher, dass ein ESP-NOW-Peer fuer die Ziel-MAC existiert.
// Eingabewert: mac zeigt auf die Ziel-MAC-Adresse.
// Ausgabewert: true bedeutet, der Peer ist vorhanden oder wurde erfolgreich angelegt.
bool stellePeerSicher(const uint8_t* mac) {
    if (!mac || !SmartHome::isValidMac(mac)) return false;
    if (esp_now_is_peer_exist(mac)) return true;

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = (uint8_t)WLAN_KANAL;
    peerInfo.encrypt = false;

    esp_err_t err = esp_now_add_peer(&peerInfo);
    if (err != ESP_OK) {
        logf("WARN", "Peer konnte nicht angelegt werden (err=%d)", (int)err);
        return false;
    }

    const char* text = macToText(mac);
    logf("INFO", "Peer aktiv: %s", text);
    return true;
}

// =============================================================================
// ESP-NOW SENDEN - Paket mit/ohne Optionen an Peer senden
// =============================================================================

// Aufgabe: Sendet ein ESP-NOW-Paket mit Header, Payload, CRC und optionaler Sequenzsteuerung.
// Eingabewerte:
// - zielMac ist die Ziel-MAC-Adresse.
// - msgType ist der SmartHome-Nachrichtentyp.
// - payload und payloadLen beschreiben den Nutzdatenbereich.
// - label wird fuer Logs verwendet.
// - flags, sequenzVorgegeben, seqOverride und outSeq steuern ACK/Retry-Verhalten.
// Ausgabewert: true bedeutet, esp_now_send wurde erfolgreich gestartet.
bool sendePaketMitOptionen(
    const uint8_t* zielMac,
    uint8_t msgType,
    const void* payload,
    size_t payloadLen,
    const char* label,
    uint8_t flags,
    bool festeSeq,
    uint8_t seq,
    uint8_t* verwendeteSeq)
{
    // Vor dem Paketbau werden alle harten Sendebedingungen geprueft. Ein
    // fehlender Peer oder ein zu grosser Payload darf nicht erst in esp_now_send()
    // scheitern, weil sonst Pending-Requests faelschlich als gesendet gelten.
    // label ist immer ein String-Literal von der Aufrufstelle.
    if (zielMac == nullptr) {
        logf("WARN", "%s: verworfen, Ziel-MAC ist null", label);
        return false;
    }
    if (payloadLen > SH_MAX_PAYLOAD_BYTES) {
        logf("WARN", "%s: verworfen, payload zu gross (%u)", label, (unsigned)payloadLen);
        return false;
    }
    if (!stellePeerSicher(zielMac)) {
        const char* txt = macToText(zielMac);
        logf("WARN", "%s: Peer nicht sicher/angelegt fuer %s", label, txt);
        return false;
    }

    uint8_t buffer[SH_ESPNOW_MAX_BYTES] = {0};
    SmartHome::MsgHeader header = {};
    const uint8_t effektiveSeq = festeSeq ? seq : masterStatus.naechste_seq++;
    SmartHome::fillHeader(header, msgType, effektiveSeq, flags, (uint16_t)payloadLen);

    // Der CRC wird ueber Header und Payload gebildet. Deshalb wird zuerst der
    // Payload in den Zielpuffer kopiert, dann der Header finalisiert und erst
    // danach in den Paketpuffer geschrieben.
    uint8_t* payloadBuffer = buffer + SH_HEADER_SIZE;
    if (payloadLen > 0U && payload != nullptr) {
        memcpy(payloadBuffer, payload, payloadLen);
    }

    SmartHome::finalizePacketCrc(header, payloadBuffer);
    memcpy(buffer, &header, sizeof(header));

    const esp_err_t err = esp_now_send(zielMac, buffer, SH_HEADER_SIZE + payloadLen);
    if (err != ESP_OK) {
        logf("WARN", "%s konnte nicht gesendet werden (err=%d)", label, (int)err);
        return false;
    }

    const char* text = macToText(zielMac);
    logf("INFO", "%s gesendet an %s", label, text);
    if (verwendeteSeq != nullptr) {
        // Aufrufer mit ACK-Erwartung brauchen exakt diese Sequenz, um das
        // spaetere ACK dem offenen Pending-Eintrag zuordnen zu koennen.
        *verwendeteSeq = effektiveSeq;
    }
    return true;
}

// Aufgabe: Sendet ein einfaches ESP-NOW-Paket ohne spezielle Flags.
// Eingabewerte: Ziel-MAC, Nachrichtentyp, Payload, Payload-Laenge und Log-Label.
// Ausgabewert: true bedeutet, das Paket wurde an ESP-NOW uebergeben.
bool sendePaket(const uint8_t* zielMac, uint8_t msgType, const void* payload, size_t payloadLen, const char* label) {
    return sendePaketMitOptionen(zielMac, msgType, payload, payloadLen, label, 0U, false, 0U, nullptr);
}

// =============================================================================
// MQTT / JSON-HELFER - Topic und Payload bauen, publishen (retained/transient)
// =============================================================================

// Aufgabe: Baut ein Master-MQTT-Topic.
// Eingabewerte: channel ist der Topic-Suffix, buffer der Ausgabepuffer.
// Ausgabewert: keiner; buffer enthaelt smarthome/master/{DEVICE_ID}/{channel}.
void baueMasterTopic(const char* channel, char* buffer, size_t bufferSize) {
    snprintf(buffer, bufferSize, "smarthome/master/%s/%s", DEVICE_ID, channel);
}

// Aufgabe: Baut ein Device-MQTT-Topic fuer eine explizite device_id.
// Eingabewerte: deviceId, suffix und Ausgabepuffer.
// Ausgabewert: keiner; buffer enthaelt smarthome/device/{deviceId}/{suffix}.
void baueNodeTopicAusId(const char* deviceId, const char* suffix, char* buffer, size_t bufferSize) {
    snprintf(buffer, bufferSize, "smarthome/device/%s/%s", deviceId ? deviceId : "unknown", suffix);
}

// Aufgabe: Baut ein Device-MQTT-Topic fuer eine Registry-Node.
// Eingabewerte: nodeIndex, suffix und Ausgabepuffer.
// Ausgabewert: keiner; buffer enthaelt das Topic fuer nodeStates[nodeIndex].
void baueNodeTopic(size_t nodeIndex, const char* suffix, char* buffer, size_t bufferSize) {
    baueNodeTopicAusId(nodeStates[nodeIndex].device_id, suffix, buffer, bufferSize);
}

// Aufgabe: Verkuendet einen retained MQTT-Payload.
// Eingabewerte: topic und payload sind nullterminierte Texte.
// Ausgabewert: keiner; Fehler werden nur geloggt.
void publishRetained(const char* topic, const char* payload) {
    if (!masterStatus.mqtt_verbunden) return;
    if (!mqttClient.publish(topic, payload, true)) {
        logf("WARN", "MQTT retained publish fehlgeschlagen: %s", topic);
        return;
    }
    logf("INFO", "MQTT retained %s -> %s", topic, payload);
}

// Aufgabe: Verkuendet einen nicht-retained MQTT-Payload.
// Eingabewerte: topic und payload sind nullterminierte Texte.
// Ausgabewert: keiner; Fehler werden nur geloggt.
void publishTransient(const char* topic, const char* payload) {
    if (!masterStatus.mqtt_verbunden) return;
    if (!mqttClient.publish(topic, payload, false)) {
        logf("WARN", "MQTT publish fehlgeschlagen: %s", topic);
        return;
    }
    logf("INFO", "MQTT publish %s -> %s", topic, payload);
}

// Aufgabe: Baut das MQTT-Status-JSON des Masters.
// Eingabewerte: buffer, bufferSize und online-Zustand.
// Ausgabewert: keiner; buffer enthaelt den Master-Status als JSON.
void baueMasterStatusJson(char* buffer, size_t bufferSize, bool online) {
    // online ist der gewuenschte Master-Lebenszustand fuer die Ausgabe. Beim
    // Last-Will wird dieselbe Funktion mit false verwendet.
    snprintf(
        buffer,
        bufferSize,
        "{\"master_id\":\"%s\",\"online\":%s,\"wifi\":%s,\"mqtt\":%s,\"espnow\":%s,\"fw\":\"%s\"}",
        DEVICE_ID,
        online ? "true" : "false",
        masterStatus.wlan_verbunden ? "true" : "false",
        masterStatus.mqtt_verbunden ? "true" : "false",
        masterStatus.espnow_bereit ? "true" : "false",
        PROJECT_VERSION);
}

// Aufgabe: Baut ein einfaches Master-Event-JSON.
// Eingabewerte: buffer, bufferSize und eventName.
// Ausgabewert: keiner; buffer enthaelt das Event-JSON.
void baueMasterEventJson(char* buffer, size_t bufferSize, const char* eventName) {
    snprintf(
        buffer,
        bufferSize,
        "{\"master_id\":\"%s\",\"event\":\"%s\",\"fw\":\"%s\"}",
        DEVICE_ID,
        eventName ? eventName : "unknown",
        PROJECT_VERSION);
}

// Aufgabe: Baut das Meta-JSON fuer eine Node.
// Eingabewerte: nodeIndex, buffer und bufferSize.
// Ausgabewert: keiner; buffer enthaelt Device-Klasse, Capabilities, Profile und Namen.
void baueNodeMetaJson(size_t nodeIndex, char* buffer, size_t bufferSize) {
    char macBuffer[18] = {0};
    // MAC ist Meta, aber nicht zwingend aus HELLO selbst bekannt. Sie stammt aus
    // der aktuellen ESP-NOW-Absenderadresse und hilft bei Feld-Diagnose.
    macText(nodeStates[nodeIndex].mac_bekannt ? nodeStates[nodeIndex].mac : nullptr, macBuffer, sizeof(macBuffer));

    snprintf(
        buffer,
        bufferSize,
        "{\"device_id\":\"%s\",\"device_name\":\"%s\",\"device_class\":\"%s\",\"power_type\":\"%s\",\"fw_version\":%u,\"caps\":%u,\"mac_address\":\"%s\",\"meta_schema_version\":%u,\"control_mode\":\"%s\",\"config_profile\":\"%s\",\"reporting_mode\":\"%s\",\"sensor_mask\":\"%s\",\"input_mask\":\"%s\"}",
        nodeStates[nodeIndex].device_id,
        nodeStates[nodeIndex].device_name,
        deviceClassText(nodeStates[nodeIndex].device_class),
        powerTypeText(nodeStates[nodeIndex].power_type),
        (unsigned)nodeStates[nodeIndex].fw_version,
        (unsigned)nodeStates[nodeIndex].caps,
        macBuffer,
        (unsigned)nodeStates[nodeIndex].meta_schema_version,
        controlModeText(nodeStates[nodeIndex].control_mode),
        configProfileText(nodeStates[nodeIndex].config_profile),
        reportingModeText(nodeStates[nodeIndex].reporting_mode),
        nodeStates[nodeIndex].sensor_mask,
        nodeStates[nodeIndex].input_mask);
}

// Aufgabe: Baut das Availability-JSON fuer eine Node.
// Eingabewerte: nodeIndex, buffer und bufferSize.
// Ausgabewert: keiner; buffer enthaelt device_id und Availability-Status.
void baueNodeAvailabilityJson(size_t nodeIndex, char* buffer, size_t bufferSize) {
    const char* availability = availabilityStateText(nodeIndex);
    // online bleibt ein hartes Boolean fuer Automationen; availability liefert
    // die feinere fachliche Abstufung online/asleep/late/offline.
    snprintf(
        buffer,
        bufferSize,
        "{\"device_id\":\"%s\",\"availability\":\"%s\",\"online\":%s,\"power_type\":\"%s\"}",
        nodeStates[nodeIndex].device_id,
        availability,
        nodeStates[nodeIndex].online ? "true" : "false",
        nodePowerTypeText(nodeIndex));
}

// Aufgabe: Schreibt eine signed Zahl oder JSON-null in einen Textpuffer.
// Eingabewerte: buffer, bufferSize, value und invalidValue.
// Ausgabewert: keiner; invalidValue wird als "null" ausgegeben.
void schreibeIntOrNull(char* buffer, size_t bufferSize, long value, long invalidValue) {
    if (value == invalidValue) {
        copyText(buffer, bufferSize, "null");
        return;
    }
    snprintf(buffer, bufferSize, "%ld", value);
}

// Aufgabe: Schreibt eine unsigned Zahl oder JSON-null in einen Textpuffer.
// Eingabewerte: buffer, bufferSize, value und invalidValue.
// Ausgabewert: keiner; invalidValue wird als "null" ausgegeben.
void schreibeUIntOrNull(char* buffer, size_t bufferSize, unsigned long value, unsigned long invalidValue) {
    if (value == invalidValue) {
        copyText(buffer, bufferSize, "null");
        return;
    }
    snprintf(buffer, bufferSize, "%lu", value);
}

// Aufgabe: Wandelt bool in JSON-String "true"/"false" um.
// Vermeidet 15-fache Wiederholung des ? : -Musters in baueNodeStateJson.
static const char* boolStr(bool v) {
    return v ? "true" : "false";
}

// Aufgabe: Baut das MQTT-State-JSON fuer eine registrierte Node.
// Eingabewerte:
// - nodeIndex ist der Index in nodeStates[].
// - buffer zeigt auf den Ausgabe-Puffer fuer den JSON-String.
// - bufferSize ist die verfuegbare Puffergroesse.
// Ausgabewert: keiner; buffer enthaelt danach das passende State-JSON.
//
// Unterstuetzte Device-Klassen:
// 1. SH_CLASS_NET_ERL: Relais, Komfortsensorik, erweiterte Luftwerte und Fehlerstatus.
// 2. SH_CLASS_NET_ZRL: Cover-Zustand, Relais, Position, Kalibrierung und Fehlerstatus.
// 3. SH_CLASS_NET_SEN: Sensorwerte, Bewegung und Fehlerstatus.
// 4. SH_CLASS_BAT_SEN: Batteriewerte, Fenster-/Regenkanaele, Buttonflags und Fehlerstatus.
//
// Ungueltige Marker wie INT16_MIN, 0xFFFF und 0xFFFFFFFF werden als JSON-null
// ausgegeben, damit der Server keine alten Messwerte als aktuelle Werte behaelt.
void baueNodeStateJson(size_t nodeIndex, char* buffer, size_t bufferSize) {
    char tempText[16] = {0};
    char humText[16] = {0};
    char luxText[16] = {0};
    char pressureText[16] = {0};
    char gasText[16] = {0};
    char aqiText[16] = {0};
    char tvocText[16] = {0};
    char eco2Text[16] = {0};
    char batteryPctText[16] = {0};
    char batteryMvText[16] = {0};
    char windowText[16] = {0};
    char rainText[16] = {0};
    char coverPositionText[16] = {0};

    switch (nodeStates[nodeIndex].device_class) {
        case SH_CLASS_NET_ERL:
            // Relais-Nodes koennen je nach Profil nur Relais oder zusaetzlich
            // Komfort-/Luftwerte melden. Unknown-Marker werden vorher zu null.
            schreibeIntOrNull(tempText, sizeof(tempText), nodeStates[nodeIndex].temp_01c, INT16_MIN);
            schreibeUIntOrNull(humText, sizeof(humText), nodeStates[nodeIndex].hum_01pct, 0xFFFFU);
            schreibeUIntOrNull(luxText, sizeof(luxText), nodeStates[nodeIndex].lux, 0xFFFFU);
            schreibeUIntOrNull(pressureText, sizeof(pressureText), nodeStates[nodeIndex].pressure_pa, NET_SEN_PRESSURE_UNGUELTIG);
            schreibeUIntOrNull(gasText, sizeof(gasText), nodeStates[nodeIndex].gas_ohm, NET_SEN_GAS_OHM_UNGUELTIG);
            schreibeUIntOrNull(aqiText, sizeof(aqiText), nodeStates[nodeIndex].aqi, NET_SEN_AIR_METRIC_UNGUELTIG);
            schreibeUIntOrNull(tvocText, sizeof(tvocText), nodeStates[nodeIndex].tvoc_ppb, NET_SEN_AIR_METRIC_UNGUELTIG);
            schreibeUIntOrNull(eco2Text, sizeof(eco2Text), nodeStates[nodeIndex].eco2_ppm, NET_SEN_AIR_METRIC_UNGUELTIG);
            snprintf(
                buffer,
                bufferSize,
                "{\"device_id\":\"%s\",\"relay_1\":%s,\"temp_01c\":%s,\"hum_01pct\":%s,\"lux\":%s,\"pressure_pa\":%s,\"gas_ohm\":%s,\"aqi\":%s,\"tvoc_ppb\":%s,\"eco2_ppm\":%s,\"motion\":%s,\"auto_flags\":%u,\"fault\":%s}",
                nodeStates[nodeIndex].device_id,
                boolStr(nodeStates[nodeIndex].relay_1),
                tempText,
                humText,
                luxText,
                pressureText,
                gasText,
                aqiText,
                tvocText,
                eco2Text,
                boolStr(nodeStates[nodeIndex].motion),
                (unsigned)nodeStates[nodeIndex].auto_flags,
                boolStr(nodeStates[nodeIndex].fault));
            return;

        case SH_CLASS_NET_ZRL:
            // Cover-Position wird auch bei unkalibrierten Nodes ausgegeben, aber
            // dann als null, wenn sie unbekannt ist. Das verhindert stale UI-Werte.
            // cover_calibrated wird als boolesches Literal eingebaut; coverStateText()
            // beruecksichtigt calibrated intern fuer den State-Text.
            schreibeUIntOrNull(
                coverPositionText,
                sizeof(coverPositionText),
                nodeStates[nodeIndex].cover_position,
                COVER_POSITION_UNBEKANNT);
            snprintf(
                buffer,
                bufferSize,
                "{\"device_id\":\"%s\",\"relay_1\":%s,\"relay_2\":%s,\"cover_mode\":%s,\"cover_state\":\"%s\",\"cover_position\":%s,\"cover_calibrated\":%s,\"fault\":%s}",
                nodeStates[nodeIndex].device_id,
                boolStr(nodeStates[nodeIndex].relay_1),
                boolStr(nodeStates[nodeIndex].relay_2),
                boolStr(nodeStates[nodeIndex].cover_mode),
                coverStateText(nodeStates[nodeIndex].cover_state, nodeStates[nodeIndex].cover_calibrated, nodeStates[nodeIndex].cover_position),
                coverPositionText,
                boolStr(nodeStates[nodeIndex].cover_calibrated),
                boolStr(nodeStates[nodeIndex].fault));
            return;

        case SH_CLASS_NET_SEN:
            // NET-SEN hat keinen Relaisanteil. Das JSON konzentriert sich auf
            // Sensorwerte und Fehlerstatus.
            schreibeIntOrNull(tempText, sizeof(tempText), nodeStates[nodeIndex].temp_01c, INT16_MIN);
            schreibeUIntOrNull(humText, sizeof(humText), nodeStates[nodeIndex].hum_01pct, 0xFFFFU);
            schreibeUIntOrNull(luxText, sizeof(luxText), nodeStates[nodeIndex].lux, 0xFFFFU);
            schreibeUIntOrNull(pressureText, sizeof(pressureText), nodeStates[nodeIndex].pressure_pa, NET_SEN_PRESSURE_UNGUELTIG);
            schreibeUIntOrNull(gasText, sizeof(gasText), nodeStates[nodeIndex].gas_ohm, NET_SEN_GAS_OHM_UNGUELTIG);
            schreibeUIntOrNull(aqiText, sizeof(aqiText), nodeStates[nodeIndex].aqi, NET_SEN_AIR_METRIC_UNGUELTIG);
            schreibeUIntOrNull(tvocText, sizeof(tvocText), nodeStates[nodeIndex].tvoc_ppb, NET_SEN_AIR_METRIC_UNGUELTIG);
            schreibeUIntOrNull(eco2Text, sizeof(eco2Text), nodeStates[nodeIndex].eco2_ppm, NET_SEN_AIR_METRIC_UNGUELTIG);
            snprintf(
                buffer,
                bufferSize,
                "{\"device_id\":\"%s\",\"temp_01c\":%s,\"hum_01pct\":%s,\"lux\":%s,\"pressure_pa\":%s,\"gas_ohm\":%s,\"aqi\":%s,\"tvoc_ppb\":%s,\"eco2_ppm\":%s,\"motion\":%s,\"fault\":%s}",
                nodeStates[nodeIndex].device_id,
                tempText,
                humText,
                luxText,
                pressureText,
                gasText,
                aqiText,
                tvocText,
                eco2Text,
                boolStr(nodeStates[nodeIndex].motion),
                boolStr(nodeStates[nodeIndex].fault));
            return;

        case SH_CLASS_BAT_SEN:
            // Batterie-Sensoren liefern kurze Zustandsbilder. Fehlende Kanaele
            // bleiben explizit null, statt mit alten Werten weiterzulaufen.
            schreibeUIntOrNull(batteryPctText, sizeof(batteryPctText), nodeStates[nodeIndex].battery_pct, BATTERY_PCT_UNGUELTIG);
            schreibeUIntOrNull(batteryMvText, sizeof(batteryMvText), nodeStates[nodeIndex].battery_mv, BATTERY_MV_UNGUELTIG);
            schreibeUIntOrNull(windowText, sizeof(windowText), nodeStates[nodeIndex].window_open, WINDOW_STATE_UNGUELTIG);
            schreibeUIntOrNull(rainText, sizeof(rainText), nodeStates[nodeIndex].rain_raw, RAIN_RAW_UNGUELTIG);
            snprintf(
                buffer,
                bufferSize,
                "{\"device_id\":\"%s\",\"battery_pct\":%s,\"battery_mv\":%s,\"window_open\":%s,\"rain_raw\":%s,\"button_flags\":%u,\"fault\":%s}",
                nodeStates[nodeIndex].device_id,
                batteryPctText,
                batteryMvText,
                windowText,
                rainText,
                (unsigned)nodeStates[nodeIndex].button_flags,
                boolStr(nodeStates[nodeIndex].fault));
            return;

        default:
            // Unreachable: state wird nur nach erfolgreichem Class-Parser veroeffentlicht.
            // Falls je erreicht, liegt ein Programmierfehler in verarbeiteStateReport() vor.
            copyText(buffer, bufferSize, "{}");
            return;
    }
}

// Aufgabe: Wandelt einen Event-Typ in den MQTT-/Log-Text um.
// Eingabewert: eventType ist ein SH_EVENT_*-Wert.
// Ausgabewert: Event-Text oder "unknown".
const char* eventTypeText(uint8_t eventType) {
    switch (eventType) {
        case SH_EVENT_BUTTON_PRESS: return "button_press";
        case SH_EVENT_BUTTON_RELEASE: return "button_release";
        case SH_EVENT_BUTTON_LONG_PRESS: return "button_long_press";
        case SH_EVENT_MOTION_DETECTED: return "motion_detected";
        case SH_EVENT_WINDOW_OPENED: return "window_opened";
        case SH_EVENT_WINDOW_CLOSED: return "window_closed";
        case SH_EVENT_RAIN_DETECTED: return "rain_detected";
        case SH_EVENT_RELAY_CHANGED: return "relay_changed";
        case SH_EVENT_LIGHT_AUTO_ON: return "light_auto_on";
        case SH_EVENT_LIGHT_AUTO_OFF: return "light_auto_off";
        case SH_EVENT_COVER_UP: return "cover_up";
        case SH_EVENT_COVER_DOWN: return "cover_down";
        case SH_EVENT_COVER_STOP: return "cover_stop";
        case SH_EVENT_COVER_CALIB_START: return "cover_calib_start";
        case SH_EVENT_COVER_CALIB_DONE: return "cover_calib_done";
        case SH_EVENT_NODE_BOOT: return "node_boot";
        case SH_EVENT_SENSOR_FAULT: return "sensor_fault";
        case SH_EVENT_COMM_FAULT: return "comm_fault";
        default: return "unknown";
    }
}

// Aufgabe: Baut das MQTT-Event-JSON fuer eine Node.
// Eingabewerte:
// - nodeIndex ist der Index in nodeStates[].
// - payload enthaelt Event-Typ, Werte und Trigger.
// - buffer und bufferSize beschreiben den Ausgabepuffer.
// Ausgabewert: keiner; buffer enthaelt das Event-JSON.
void baueNodeEventJson(size_t nodeIndex, const SmartHome::EventReportPayload& payload, char* buffer, size_t bufferSize) {
    snprintf(
        buffer,
        bufferSize,
        "{\"device_id\":\"%s\",\"event\":\"%s\",\"event_type\":%u,\"trigger\":%u,\"param1\":%u,\"param2\":%u}",
        nodeStates[nodeIndex].device_id,
        eventTypeText(payload.event_type),
        (unsigned)payload.event_type,
        (unsigned)payload.trigger,
        (unsigned)payload.param1,
        (unsigned)payload.param2);
}

// Aufgabe: Verkuendet den aktuellen Master-Status retained per MQTT.
// Eingabewerte: keine.
// Ausgabewert: keiner.
void publishMasterStatus() {
    char topic[96] = {0};
    char payload[192] = {0};
    baueMasterTopic("status", topic, sizeof(topic));
    baueMasterStatusJson(payload, sizeof(payload), true);
    publishRetained(topic, payload);
}

// Aufgabe: Verkuendet ein Master-Event transient per MQTT.
// Eingabewert: eventName ist der Event-Name.
// Ausgabewert: keiner.
void publishMasterEvent(const char* eventName) {
    char topic[96] = {0};
    char payload[160] = {0};
    baueMasterTopic("event", topic, sizeof(topic));
    baueMasterEventJson(payload, sizeof(payload), eventName);
    publishTransient(topic, payload);
}

// Aufgabe: Verkuendet Meta-Daten einer Node retained per MQTT.
// Eingabewert: nodeIndex ist der Index in nodeStates[].
// Ausgabewert: keiner.
void publishNodeMeta(size_t nodeIndex) {
    // Meta wird erst nach echtem HELLO publiziert. Provisorische Slots duerfen
    // nicht als vollstaendige Geraete in MQTT erscheinen.
    if (!nodeStates[nodeIndex].belegt || !nodeStates[nodeIndex].meta_bekannt) return;
    char topic[96] = {0};
    char payload[640] = {0};
    baueNodeTopic(nodeIndex, "meta", topic, sizeof(topic));
    baueNodeMetaJson(nodeIndex, payload, sizeof(payload));
    publishRetained(topic, payload);
}

// Aufgabe: Verkuendet die Availability einer Node retained per MQTT.
// Eingabewert: nodeIndex ist der Index in nodeStates[].
// Ausgabewert: keiner.
void publishNodeAvailability(size_t nodeIndex) {
    // Availability darf auch fuer unvollstaendige Slots erscheinen. Dadurch ist
    // sichtbar, dass ein Geraet funkt, auch wenn HELLO/META noch aussteht.
    if (!nodeStates[nodeIndex].belegt) return;
    char topic[96] = {0};
    char payload[192] = {0};
    baueNodeTopic(nodeIndex, "availability", topic, sizeof(topic));
    baueNodeAvailabilityJson(nodeIndex, payload, sizeof(payload));
    publishRetained(topic, payload);
}

// Aufgabe: Verkuendet den aktuellen State einer Node retained per MQTT.
// Eingabewert: nodeIndex ist der Index in nodeStates[].
// Ausgabewert: keiner.
void publishNodeState(size_t nodeIndex) {
    // State wird nur veroeffentlicht, wenn mindestens ein STATE erfolgreich und
    // passend zur erkannten Klasse verarbeitet wurde.
    if (!nodeStates[nodeIndex].belegt || !nodeStates[nodeIndex].state_bekannt) return;
    char topic[96] = {0};
    char payload[512] = {0};
    baueNodeTopic(nodeIndex, "state", topic, sizeof(topic));
    baueNodeStateJson(nodeIndex, payload, sizeof(payload));
    publishRetained(topic, payload);
}

// Aufgabe: Verkuendet ein Node-Event transient per MQTT.
// Eingabewerte: nodeIndex und empfangener Event-Payload.
// Ausgabewert: keiner.
void publishNodeEvent(size_t nodeIndex, const SmartHome::EventReportPayload& payload) {
    if (!nodeStates[nodeIndex].belegt) return;
    char topic[96] = {0};
    char json[224] = {0};
    baueNodeTopic(nodeIndex, "event", topic, sizeof(topic));
    baueNodeEventJson(nodeIndex, payload, json, sizeof(json));
    publishTransient(topic, json);
}

// Aufgabe: Verkuendet ein ACK fuer eine device_id, auch wenn die Node nicht registriert ist.
// Eingabewerte: deviceId, requestId, Channel, Status, Code, ACK-Typ, Sequenz und Quelle.
// Ausgabewert: keiner; das ACK wird transient per MQTT veroeffentlicht.
void publishNodeAckById(const char* deviceId, const char* requestId, const char* channel, const char* statusText, int statusCode, uint8_t ackMsgType, uint8_t ackSeq, const char* source) {
    char topic[96] = {0};
    char payload[640] = {0};
    char escapedDeviceId[(SH_DEVICE_ID_LEN * 2U) + 1U] = {0};
    char escapedRequestId[(REQUEST_ID_LEN * 2U) + 1U] = {0};
    char escapedChannel[48] = {0};
    char escapedStatus[48] = {0};
    char escapedSource[64] = {0};

    baueNodeTopicAusId(deviceId, "ack", topic, sizeof(topic));
    // ACKs spiegeln teilweise Werte aus MQTT-Kommandos zurueck. Alle Stringfelder
    // werden deshalb escaped, auch wenn device_id und request_id normalerweise
    // kontrollierte Formate haben.
    jsonEscapeText(escapedDeviceId, sizeof(escapedDeviceId), deviceId ? deviceId : "unknown");
    jsonEscapeText(escapedRequestId, sizeof(escapedRequestId), requestId ? requestId : "");
    jsonEscapeText(escapedChannel, sizeof(escapedChannel), channel ? channel : "command");
    jsonEscapeText(escapedStatus, sizeof(escapedStatus), statusText ? statusText : "unknown");
    jsonEscapeText(escapedSource, sizeof(escapedSource), source ? source : "master");

    snprintf(
        payload,
        sizeof(payload),
        "{\"device_id\":\"%s\",\"request_id\":\"%s\",\"channel\":\"%s\",\"status\":\"%s\",\"status_code\":%d,\"ack_msg_type\":%u,\"ack_seq\":%u,\"source\":\"%s\"}",
        escapedDeviceId,
        escapedRequestId,
        escapedChannel,
        escapedStatus,
        statusCode,
        (unsigned)ackMsgType,
        (unsigned)ackSeq,
        escapedSource);
    publishTransient(topic, payload);
}

// Aufgabe: Verkuendet ein ACK fuer eine registrierte Node.
// Eingabewerte: nodeIndex und ACK-Felder.
// Ausgabewert: keiner; intern wird publishNodeAckById genutzt.
void publishNodeAck(size_t nodeIndex, const char* requestId, const char* channel, const char* statusText, int statusCode, uint8_t ackMsgType, uint8_t ackSeq, const char* source) {
    publishNodeAckById(nodeStates[nodeIndex].device_id, requestId, channel, statusText, statusCode, ackMsgType, ackSeq, source);
}

// Aufgabe: Verkuendet nach MQTT-Reconnect alle bekannten Node-Daten erneut.
// Eingabewerte: keine.
// Ausgabewert: keiner; Meta, Availability und bekannte States werden erneut publiziert.
void publishBekannteNodesNachReconnect() {
    // Retained Topics koennen beim Broker verloren gegangen sein oder der Master
    // kann zwischenzeitlich offline gewesen sein. Nach MQTT-Reconnect wird der
    // komplette bekannte Zustand erneut angeboten.
    publishMasterStatus();
    publishMasterEvent("mqtt_connected");
    for (size_t i = 0; i < MAX_DYNAMIC_NODES; ++i) {
        if (!nodeStates[i].belegt) continue;
        publishNodeMeta(i);
        publishNodeAvailability(i);
        publishNodeState(i);
    }
}

// Vorwaertsdeklaration, damit Kontakt-Updates nach einem NVS-Restore aktiv ein
// frisches HELLO anfordern koennen.
bool sendeHelloRequestAnMac(const uint8_t* mac);

// Aufgabe: Aktualisiert Kontaktzeit, MAC-Adresse, Peer und Online-Status einer Node.
// Eingabewerte:
// - nodeIndex ist der Index in nodeStates[].
// - mac ist die aktuelle Absender-MAC.
// Ausgabewert: keiner; bei Online-Wechsel wird Availability publiziert.
void aktualisiereNodeKontakt(size_t nodeIndex, const uint8_t* mac) {
    // Jede gueltige Funknachricht zaehlt als Kontakt. Die Offline-Logik arbeitet
    // nur mit diesem Zeitstempel und muss nicht wissen, welcher Nachrichtentyp kam.
    nodeStates[nodeIndex].letzter_kontakt_ms = millis();

    if (mac != nullptr) {
        // Die MAC kann sich nach Reflash oder Boardtausch aendern. Wenn dieselbe
        // device_id mit neuer MAC sendet, folgt der Master der aktuellen Quelle.
        const bool neueMac = !nodeStates[nodeIndex].mac_bekannt || memcmp(nodeStates[nodeIndex].mac, mac, 6) != 0;
        memcpy(nodeStates[nodeIndex].mac, mac, 6);
        nodeStates[nodeIndex].mac_bekannt = true;
        stellePeerSicher(nodeStates[nodeIndex].mac);
        if (neueMac) {
            const char* text = macToText(mac);
            logf("INFO", "%s MAC aktualisiert: %s", nodeStates[nodeIndex].device_id, text);
        }
    }

    if (nodeStates[nodeIndex].meta_wiederhergestellt && nodeStates[nodeIndex].mac_bekannt) {
        // Persistierte Meta macht den Master sofort handlungsfaehig, bleibt aber
        // ein Cache. Beim naechsten Kontakt wird deshalb ein echtes HELLO
        // nachgezogen, damit geaenderte Capabilities/Profile wieder zur Quelle
        // der Wahrheit werden.
        const unsigned long now = millis();
        if (nodeStates[nodeIndex].letzter_hello_request_ms == 0UL ||
            (now - nodeStates[nodeIndex].letzter_hello_request_ms) >= HELLO_REQUEST_RETRY_INTERVAL_MS) {
            if (sendeHelloRequestAnMac(nodeStates[nodeIndex].mac)) {
                nodeStates[nodeIndex].letzter_hello_request_ms = now;
                logf("INFO", "%s: HELLO_REFRESH nach Registry-Restore angefordert", nodeStates[nodeIndex].device_id);
            }
        }
    }

    const bool warOffline = !nodeStates[nodeIndex].online;
    nodeStates[nodeIndex].online = true;
    if (warOffline) {
        // Nur Statuswechsel werden direkt geloggt/publiziert. Normale Heartbeats
        // erneuern den Zeitstempel, ohne MQTT unnoetig zu fluten.
        publishNodeAvailability(nodeIndex);
        logf("INFO", "Node %s ist online", nodeStates[nodeIndex].device_id);
    }
}

// =============================================================================
// PROTOKOLL-VERARBEITUNG (ESP-NOW) - HELLO, HEARTBEAT, STATE, EVENT, ACK
// =============================================================================

// Aufgabe: Sendet eine HELLO_ACK-Antwort an eine Node.
// Eingabewerte:
// - zielMac ist die MAC-Adresse der Node.
// - ackStatus ist SH_ACK_OK oder ein Ablehnungsstatus.
// Ausgabewert: keiner; die Antwort wird per ESP-NOW gesendet.
void sendeHelloAck(const uint8_t* zielMac, uint8_t ackStatus) {
    SmartHome::HelloAckPayload payload = {};
    payload.channel = (uint8_t)(masterStatus.wlan_verbunden ? WiFi.channel() : WLAN_KANAL);
    payload.ack_status = ackStatus;
    sendePaket(zielMac, SH_MSG_HELLO_ACK, &payload, sizeof(payload), "HELLO_ACK");
}

// Aufgabe: Registriert eine neue Node oder findet eine bestehende anhand der HELLO-Daten.
// Eingabewerte:
// - senderMac ist die MAC-Adresse des Senders.
// - payload enthaelt device_id, device_class, power_type und Meta-Daten der Node.
// Ausgabewert: Node-Index bei Erfolg, -1 bei ungueltigen Daten oder
// STATUS_CODE_REGISTRY_FULL wenn kein Registry-Slot mehr frei ist.
//
// Ablauf:
// 1. device_id, device_class und power_type werden validiert.
// 2. Eine bekannte device_id gewinnt, solange kein MAC- oder Typ-Konflikt vorliegt.
// 3. Eine bekannte MAC mit anderer device_id wird abgelehnt.
// 4. Neue Nodes bekommen den ersten freien Registry-Slot.
int registriereOderFindeNode(const uint8_t* senderMac, const SmartHome::HelloPayload& payload) {
    if (!SmartHome::isValidDeviceId(payload.device_id)) {
        logf("WARN", "HELLO abgelehnt: ungueltige device_id=%s", payload.device_id);
        return -1;
    }
    if (!istDeviceClassGueltig(payload.device_class) || !istPowerTypeGueltig(payload.power_type)) {
        logf("WARN", "HELLO abgelehnt: class=%u power=%u", (unsigned)payload.device_class, (unsigned)payload.power_type);
        return -1;
    }
    const int idIndex = findeNodeIndex(payload.device_id);
    const int macIndex = findeNodeIndexPerMac(senderMac);

    if (idIndex >= 0) {
        if (macIndex >= 0 && macIndex != idIndex) {
            logf("WARN", "HELLO abgelehnt: MAC-Konflikt fuer %s", payload.device_id);
            return -1;
        }
        if (nodeStates[idIndex].meta_bekannt &&
            (nodeStates[idIndex].device_class != payload.device_class || nodeStates[idIndex].power_type != payload.power_type)) {
            logf("WARN", "HELLO abgelehnt: bestehende node %s hat andere class/power", payload.device_id);
            return -1;
        }
        nodeStates[idIndex].device_class = payload.device_class;
        nodeStates[idIndex].power_type = payload.power_type;
        return idIndex;
    }

    if (macIndex >= 0 && strncmp(nodeStates[macIndex].device_id, payload.device_id, SH_DEVICE_ID_LEN) != 0) {
        logf("WARN", "HELLO abgelehnt: bekannte MAC gehoert bereits zu %s", nodeStates[macIndex].device_id);
        return -1;
    }

    const int freeIndex = findeFreienNodeIndex();
    if (freeIndex < 0) {
        logf("WARN", "HELLO abgelehnt: Registry voll (device_id=%s)", payload.device_id);
        return STATUS_CODE_REGISTRY_FULL;
    }

    initialisiereNodeSlot(nodeStates[freeIndex]);
    nodeStates[freeIndex].belegt = true;
    nodeStates[freeIndex].device_class = payload.device_class;
    nodeStates[freeIndex].power_type = payload.power_type;
    copyText(nodeStates[freeIndex].device_id, sizeof(nodeStates[freeIndex].device_id), payload.device_id);
    logf("INFO", "Node dynamisch registriert: %s (%s)", payload.device_id, deviceClassText(payload.device_class));
    return freeIndex;
}

// Aufgabe: Versucht, eine Node provisorisch anhand ihrer device_id zu registrieren.
// Diese Registrierung ist "leichtgewichtig": Sie merkt nur Identitaet, MAC und
// Kontaktzeit, damit ein gezielter HELLO_REQUEST moeglich ist. Klasse, Caps und
// Versorgungstyp bleiben bis zum echten HELLO unbekannt.
// Liefert Node-Index oder -1 bei Fehler, STATUS_CODE_REGISTRY_FULL wenn voll.
int registriereProvisorischMitId(const uint8_t* senderMac, const char* deviceId) {
    if (!SmartHome::isValidDeviceId(deviceId)) {
        logf("WARN", "Provisorische Registration abgelehnt: ungueltige device_id=%s", deviceId ? deviceId : "?");
        return -1;
    }

    const char* macTxt = macToText(senderMac);
    logf("INFO", "Provisorische Registration: device_id=%s mac=%s, warte auf HELLO", deviceId ? deviceId : "?", macTxt);

    const int idIndex = findeNodeIndex(deviceId);
    const int macIndex = findeNodeIndexPerMac(senderMac);
    if (idIndex >= 0) {
        return idIndex; // schon vorhanden
    }
    if (macIndex >= 0 && strncmp(nodeStates[macIndex].device_id, deviceId, SH_DEVICE_ID_LEN) != 0) {
        logf("WARN", "Provisorische Registration abgelehnt: bekannte MAC gehoert bereits zu %s", nodeStates[macIndex].device_id);
        return -1;
    }

    const int freeIndex = findeFreienNodeIndex();
    if (freeIndex < 0) {
        logf("WARN", "Provisorische Registration fehlgeschlagen: Registry voll (device_id=%s)", deviceId);
        return STATUS_CODE_REGISTRY_FULL;
    }

    initialisiereNodeSlot(nodeStates[freeIndex]);
    nodeStates[freeIndex].belegt = true;
    nodeStates[freeIndex].provisorisch = true;
    nodeStates[freeIndex].device_class = SH_CLASS_UNKNOWN;
    nodeStates[freeIndex].power_type = SH_POWER_MAINS;
    copyText(nodeStates[freeIndex].device_id, sizeof(nodeStates[freeIndex].device_id), deviceId);
    nodeStates[freeIndex].meta_bekannt = false;
    logf("INFO", "Node provisorisch registriert: %s (via state/heartbeat, HELLO ausstehend)", deviceId);
    if (senderMac != nullptr) aktualisiereNodeKontakt((size_t)freeIndex, senderMac);
    publishNodeAvailability((size_t)freeIndex);
    return freeIndex;
}

// Aufgabe: Verarbeitet ein HELLO einer Node und veroeffentlicht deren Meta-Daten.
// Eingabewerte:
// - senderMac ist die Absender-MAC.
// - payload enthaelt Identitaet, Capabilities, Profile und Masken der Node.
// Ausgabewert: keiner; bei Erfolg wird HELLO_ACK OK gesendet.
void verarbeiteHello(const uint8_t* senderMac, const SmartHome::HelloPayload& payload) {
    const int nodeIndex = registriereOderFindeNode(senderMac, payload);
    if (nodeIndex < 0) {
        const uint8_t ackStatus = (nodeIndex == STATUS_CODE_REGISTRY_FULL)
            ? SH_ACK_REJECTED_FULL : SH_ACK_REJECTED;
        sendeHelloAck(senderMac, ackStatus);
        return;
    }

    nodeStates[nodeIndex].meta_bekannt = true;
    nodeStates[nodeIndex].meta_wiederhergestellt = false;
    nodeStates[nodeIndex].provisorisch = false;
    nodeStates[nodeIndex].caps = holeHelloCaps(payload);
    nodeStates[nodeIndex].fw_version = payload.fw_version;
    nodeStates[nodeIndex].meta_schema_version = payload.meta_schema_version;
    nodeStates[nodeIndex].control_mode = payload.control_mode;
    nodeStates[nodeIndex].config_profile = payload.config_profile;
    nodeStates[nodeIndex].reporting_mode = payload.reporting_mode;
    nodeStates[nodeIndex].letzter_hello_request_ms = 0UL;
    copyText(nodeStates[nodeIndex].device_name, sizeof(nodeStates[nodeIndex].device_name), payload.device_name);
    copyText(nodeStates[nodeIndex].sensor_mask, sizeof(nodeStates[nodeIndex].sensor_mask), payload.sensor_mask);
    copyText(nodeStates[nodeIndex].input_mask, sizeof(nodeStates[nodeIndex].input_mask), payload.input_mask);
    sanitisiereNodeStateNachCapabilities((size_t)nodeIndex);
    aktualisiereNodeKontakt((size_t)nodeIndex, senderMac);
    persistiereRegistrySnapshot("HELLO");

    publishNodeMeta((size_t)nodeIndex);
    publishNodeAvailability((size_t)nodeIndex);
    if (nodeStates[nodeIndex].state_bekannt) {
        publishNodeState((size_t)nodeIndex);
    }

    sendeHelloAck(senderMac, SH_ACK_OK);
    logf("INFO", "HELLO von %s (%s)", payload.device_id, payload.device_name);
}

// Vorwaertsdeklaration, damit der Aufruf in Heartbeat/State erkannt wird.
bool sendeHelloRequestAnMac(const uint8_t* mac);

bool fordereHelloBeiBedarf(size_t nodeIndex, const uint8_t* senderMac, const char* grund) {
    // Sobald echtes HELLO angekommen ist, sind Klasse, Caps und Profile bekannt.
    // Ab dann waere ein wiederholter HELLO_REQUEST nur Funkrauschen.
    if (nodeStates[nodeIndex].meta_bekannt) return false;
    const unsigned long now = millis();
    if (nodeStates[nodeIndex].letzter_hello_request_ms != 0UL &&
        (now - nodeStates[nodeIndex].letzter_hello_request_ms) < HELLO_REQUEST_RETRY_INTERVAL_MS) {
        // Drosselung: eine dauerhaft sendende, aber nicht korrekt antwortende
        // Node soll den Master nicht alle paar Millisekunden HELLO_REQUEST senden lassen.
        return false;
    }

    const char* macTxt = macToText(senderMac);
    logf("INFO", "%s: Meta fuer %s fehlt, sende HELLO_REQUEST an %s",
         grund ? grund : "HELLO_SYNC",
         nodeStates[nodeIndex].device_id,
         macTxt);
    if (sendeHelloRequestAnMac(senderMac)) {
        nodeStates[nodeIndex].letzter_hello_request_ms = now;
        return true;
    }
    return false;
}

// Aufgabe: Verarbeitet ein HEARTBEAT einer bekannten Node.
// Eingabewerte:
// - senderMac ist die Absender-MAC.
// - payload enthaelt node_id und uptime_s.
// Ausgabewert: keiner; Kontaktzeit, Uptime und Availability werden aktualisiert.
void verarbeiteHeartbeat(const uint8_t* senderMac, const SmartHome::HeartbeatPayload& payload) {
    int nodeIndex = findeNodeIndex(payload.node_id);
    if (nodeIndex < 0) {
        const int r = registriereProvisorischMitId(senderMac, payload.node_id);
        if (r < 0) {
            logf("WARN", "HEARTBEAT ignoriert: unbekannte node_id=%s (registrieren failed code=%d)", payload.node_id, r);
            return;
        }
        nodeIndex = r;
    }
    fordereHelloBeiBedarf((size_t)nodeIndex, senderMac, "HEARTBEAT");
    aktualisiereNodeKontakt((size_t)nodeIndex, senderMac);
    nodeStates[nodeIndex].uptime_s = payload.uptime_s;
    publishNodeAvailability((size_t)nodeIndex);
    logf("INFO", "HEARTBEAT von %s (uptime=%lus)", payload.node_id, (unsigned long)payload.uptime_s);
}

// =============================================================================
// DEVICE-CLASS STATE PARSER – je einer pro Geraeteklasse
// =============================================================================
// Jeder Parser interpretiert die Payload-Bytes gemaess der Payload-Laenge.
// Rueckgabe: true = erfolgreich geparst, false = ungueltige Laenge.

static bool parseNetErlState(size_t nodeIndex, const uint8_t* payload, uint16_t payloadLen) {
    // NET-ERL hat die meisten Varianten: vom einfachen Relais bis zum
    // LED-Ring-Modul mit erweiterten Luftqualitaetswerten. Die Reihenfolge
    // ist wichtig, weil manche Config-/State-Layouts gleich gross sind.
    if (payloadLen == sizeof(SmartHome::StateReportPayload)) {
        const SmartHome::StateReportPayload& state = *reinterpret_cast<const SmartHome::StateReportPayload*>(payload);
        nodeStates[nodeIndex].relay_1 = (state.relay_1 != 0U);
        nodeStates[nodeIndex].fault = (state.fault != 0U);
        // Einfaches Relais: keine Sensor-Erweiterungsfelder.
        setzeNetSenZusatzwerteUnbekannt(nodeIndex);
        return true;
    }
    if (payloadLen == sizeof(SmartHome::StateConfigReportPayload)) {
        const SmartHome::StateConfigReportPayload& state = *reinterpret_cast<const SmartHome::StateConfigReportPayload*>(payload);
        nodeStates[nodeIndex].relay_1 = (state.relay_1 != 0U);
        nodeStates[nodeIndex].fault = (state.fault != 0U);
        // Einfaches Relais mit Config: keine Sensor-Erweiterungsfelder.
        setzeNetSenZusatzwerteUnbekannt(nodeIndex);
        return true;
    }
    if (payloadLen == sizeof(SmartHome::RelayComfortStateReportPayload) ||
        payloadLen == sizeof(SmartHome::RelayComfortConfigStateReportPayload)) {
        const SmartHome::RelayComfortStateReportPayload& state = *reinterpret_cast<const SmartHome::RelayComfortStateReportPayload*>(payload);
        nodeStates[nodeIndex].relay_1 = (state.relay_1 != 0U);
        nodeStates[nodeIndex].temp_01c = state.temp_01c;
        nodeStates[nodeIndex].hum_01pct = state.hum_01pct;
        nodeStates[nodeIndex].lux = state.lux;
        // Sensor-Erweiterungsfelder sind in diesem Payload-Typ nicht belegt.
        setzeNetSenZusatzwerteUnbekannt(nodeIndex);
        nodeStates[nodeIndex].motion = (state.motion != 0U);
        nodeStates[nodeIndex].fault = (state.fault != 0U);
        nodeStates[nodeIndex].auto_flags = state.auto_flags;
        return true;
    }
    // ExtendedRelayComfortConfigStateReportPayload und
    // ExtendedRelayComfortGasStateReportPayload sind beide 41 Bytes.
    // verarbeiteStateReport() ruft Parser erst nach bekannter HELLO-Meta auf.
    // Nach HELLO gilt:
    //   SH_CAP_LED_RING gesetzt: Config-State-Layout (kein Gas-Sensor)
    //   SH_CAP_LED_RING nicht:   Gas-State-Layout
    // Ohne HELLO darf dieser Block nie erreicht werden.
    if (payloadLen == sizeof(SmartHome::ExtendedRelayComfortStateReportPayload)) {
        const SmartHome::ExtendedRelayComfortStateReportPayload& state =
            *reinterpret_cast<const SmartHome::ExtendedRelayComfortStateReportPayload*>(payload);
        nodeStates[nodeIndex].relay_1 = (state.relay_1 != 0U);
        nodeStates[nodeIndex].temp_01c = state.temp_01c;
        nodeStates[nodeIndex].hum_01pct = state.hum_01pct;
        nodeStates[nodeIndex].lux = state.lux;
        nodeStates[nodeIndex].pressure_pa = state.pressure_pa;
        nodeStates[nodeIndex].gas_ohm = NET_SEN_GAS_OHM_UNGUELTIG;
        nodeStates[nodeIndex].aqi = state.aqi;
        nodeStates[nodeIndex].tvoc_ppb = state.tvoc_ppb;
        nodeStates[nodeIndex].eco2_ppm = state.eco2_ppm;
        nodeStates[nodeIndex].motion = (state.motion != 0U);
        nodeStates[nodeIndex].fault = (state.fault != 0U);
        nodeStates[nodeIndex].auto_flags = state.auto_flags;
        return true;
    }
    if (payloadLen == sizeof(SmartHome::ExtendedRelayComfortConfigStateReportPayload)) {
        // 41 Bytes: Config-State (kein Gas-Sensor, SH_CAP_LED_RING gesetzt) ODER
        // Gas-State (Gas-Sensor vorhanden, SH_CAP_LED_RING nicht gesetzt).
        // Die Entscheidung haengt an SH_CAP_LED_RING aus dem HELLO-Payload.
        if (nodeHasCap(nodeIndex, SH_CAP_LED_RING)) {
            // LED-Ring-Node: Config-State-Layout parsen.
            const SmartHome::ExtendedRelayComfortConfigStateReportPayload& state =
                *reinterpret_cast<const SmartHome::ExtendedRelayComfortConfigStateReportPayload*>(payload);
            nodeStates[nodeIndex].relay_1 = (state.relay_1 != 0U);
            nodeStates[nodeIndex].temp_01c = state.temp_01c;
            nodeStates[nodeIndex].hum_01pct = state.hum_01pct;
            nodeStates[nodeIndex].lux = state.lux;
            nodeStates[nodeIndex].pressure_pa = state.pressure_pa;
            nodeStates[nodeIndex].gas_ohm = NET_SEN_GAS_OHM_UNGUELTIG;
            nodeStates[nodeIndex].aqi = state.aqi;
            nodeStates[nodeIndex].tvoc_ppb = state.tvoc_ppb;
            nodeStates[nodeIndex].eco2_ppm = state.eco2_ppm;
            nodeStates[nodeIndex].motion = (state.motion != 0U);
            nodeStates[nodeIndex].fault = (state.fault != 0U);
            nodeStates[nodeIndex].auto_flags = state.auto_flags;
        } else {
            // Standard-ERL ohne LED-Ring: Gas-State-Layout parsen.
            const SmartHome::ExtendedRelayComfortGasStateReportPayload& state =
                *reinterpret_cast<const SmartHome::ExtendedRelayComfortGasStateReportPayload*>(payload);
            nodeStates[nodeIndex].relay_1 = (state.relay_1 != 0U);
            nodeStates[nodeIndex].temp_01c = state.temp_01c;
            nodeStates[nodeIndex].hum_01pct = state.hum_01pct;
            nodeStates[nodeIndex].lux = state.lux;
            nodeStates[nodeIndex].pressure_pa = state.pressure_pa;
            nodeStates[nodeIndex].gas_ohm = state.gas_ohm;
            nodeStates[nodeIndex].aqi = state.aqi;
            nodeStates[nodeIndex].tvoc_ppb = state.tvoc_ppb;
            nodeStates[nodeIndex].eco2_ppm = state.eco2_ppm;
            nodeStates[nodeIndex].motion = (state.motion != 0U);
            nodeStates[nodeIndex].fault = (state.fault != 0U);
            nodeStates[nodeIndex].auto_flags = state.auto_flags;
        }
        return true;
    }
    if (payloadLen == sizeof(SmartHome::ExtendedRelayComfortGasConfigStateReportPayload)) {
        const SmartHome::ExtendedRelayComfortGasConfigStateReportPayload& state =
            *reinterpret_cast<const SmartHome::ExtendedRelayComfortGasConfigStateReportPayload*>(payload);
        nodeStates[nodeIndex].relay_1 = (state.relay_1 != 0U);
        nodeStates[nodeIndex].temp_01c = state.temp_01c;
        nodeStates[nodeIndex].hum_01pct = state.hum_01pct;
        nodeStates[nodeIndex].lux = state.lux;
        nodeStates[nodeIndex].pressure_pa = state.pressure_pa;
        nodeStates[nodeIndex].gas_ohm = state.gas_ohm;
        nodeStates[nodeIndex].aqi = state.aqi;
        nodeStates[nodeIndex].tvoc_ppb = state.tvoc_ppb;
        nodeStates[nodeIndex].eco2_ppm = state.eco2_ppm;
        nodeStates[nodeIndex].motion = (state.motion != 0U);
        nodeStates[nodeIndex].fault = (state.fault != 0U);
        nodeStates[nodeIndex].auto_flags = state.auto_flags;
        return true;
    }
    logf("WARN", "STATE_REPORT Laenge ungueltig fuer %s", nodeStates[nodeIndex].device_id);
    return false;
}

static bool parseNetZrlState(size_t nodeIndex, const uint8_t* payload, uint16_t payloadLen) {
    // NET-ZRL liefert Rolladen-/Cover-Zustand. Config-State enthaelt
    // zusaetzliche Konfigwerte, aber dieselben Live-Felder.
    if (payloadLen == sizeof(SmartHome::ZrlStateReportPayload)) {
        const SmartHome::ZrlStateReportPayload& state = *reinterpret_cast<const SmartHome::ZrlStateReportPayload*>(payload);
        nodeStates[nodeIndex].relay_1 = (state.relay_1 != 0U);
        nodeStates[nodeIndex].relay_2 = (state.relay_2 != 0U);
        nodeStates[nodeIndex].cover_mode = (state.cover_mode != 0U);
        nodeStates[nodeIndex].cover_state = state.cover_state;
        nodeStates[nodeIndex].cover_position = state.cover_position;
        nodeStates[nodeIndex].cover_calibrated = (state.cover_calibrated != 0U);
        nodeStates[nodeIndex].fault = (state.fault != 0U);
        return true;
    }
    if (payloadLen == sizeof(SmartHome::ZrlConfigStateReportPayload)) {
        // Config-State enthaelt dieselben Live-Zustandsfelder plus report_interval_s.
        // Der Master nutzt hier nur den Live-Zustand; daher bleibt der Kopierblock gleich.
        const SmartHome::ZrlConfigStateReportPayload& state = *reinterpret_cast<const SmartHome::ZrlConfigStateReportPayload*>(payload);
        nodeStates[nodeIndex].relay_1 = (state.relay_1 != 0U);
        nodeStates[nodeIndex].relay_2 = (state.relay_2 != 0U);
        nodeStates[nodeIndex].cover_mode = (state.cover_mode != 0U);
        nodeStates[nodeIndex].cover_state = state.cover_state;
        nodeStates[nodeIndex].cover_position = state.cover_position;
        nodeStates[nodeIndex].cover_calibrated = (state.cover_calibrated != 0U);
        nodeStates[nodeIndex].fault = (state.fault != 0U);
        return true;
    }
    logf("WARN", "STATE_REPORT Laenge ungueltig fuer %s", nodeStates[nodeIndex].device_id);
    return false;
}

static bool parseNetSenState(size_t nodeIndex, const uint8_t* payload, uint16_t payloadLen) {
    // NET-SEN kennt Basis-, Extended- und Gas-Varianten. Felder, die in
    // einer Variante fehlen, werden explizit auf Unknown gesetzt.
    if (payloadLen == sizeof(SmartHome::SensorStateReportPayload)) {
        const SmartHome::SensorStateReportPayload& state = *reinterpret_cast<const SmartHome::SensorStateReportPayload*>(payload);
        nodeStates[nodeIndex].temp_01c = state.temp_01c;
        nodeStates[nodeIndex].hum_01pct = state.hum_01pct;
        nodeStates[nodeIndex].lux = state.lux;
        nodeStates[nodeIndex].motion = (state.motion != 0U);
        nodeStates[nodeIndex].fault = (state.fault != 0U);
        setzeNetSenZusatzwerteUnbekannt(nodeIndex);
        return true;
    }
    if (payloadLen == sizeof(SmartHome::SensorConfigStateReportPayload)) {
        const SmartHome::SensorConfigStateReportPayload& state = *reinterpret_cast<const SmartHome::SensorConfigStateReportPayload*>(payload);
        nodeStates[nodeIndex].temp_01c = state.temp_01c;
        nodeStates[nodeIndex].hum_01pct = state.hum_01pct;
        nodeStates[nodeIndex].lux = state.lux;
        nodeStates[nodeIndex].motion = (state.motion != 0U);
        nodeStates[nodeIndex].fault = (state.fault != 0U);
        setzeNetSenZusatzwerteUnbekannt(nodeIndex);
        return true;
    }
    if (payloadLen == sizeof(SmartHome::ExtendedSensorStateReportPayload) ||
        payloadLen == sizeof(SmartHome::ExtendedSensorConfigStateReportPayload)) {
        const SmartHome::ExtendedSensorStateReportPayload& state = *reinterpret_cast<const SmartHome::ExtendedSensorStateReportPayload*>(payload);
        nodeStates[nodeIndex].temp_01c = state.temp_01c;
        nodeStates[nodeIndex].hum_01pct = state.hum_01pct;
        nodeStates[nodeIndex].lux = state.lux;
        nodeStates[nodeIndex].pressure_pa = state.pressure_pa;
        nodeStates[nodeIndex].gas_ohm = NET_SEN_GAS_OHM_UNGUELTIG;
        nodeStates[nodeIndex].aqi = state.aqi;
        nodeStates[nodeIndex].tvoc_ppb = state.tvoc_ppb;
        nodeStates[nodeIndex].eco2_ppm = state.eco2_ppm;
        nodeStates[nodeIndex].motion = (state.motion != 0U);
        nodeStates[nodeIndex].fault = (state.fault != 0U);
        return true;
    }
    if (payloadLen == sizeof(SmartHome::ExtendedSensorGasStateReportPayload) ||
        payloadLen == sizeof(SmartHome::ExtendedSensorGasConfigStateReportPayload)) {
        const SmartHome::ExtendedSensorGasStateReportPayload& state = *reinterpret_cast<const SmartHome::ExtendedSensorGasStateReportPayload*>(payload);
        nodeStates[nodeIndex].temp_01c = state.temp_01c;
        nodeStates[nodeIndex].hum_01pct = state.hum_01pct;
        nodeStates[nodeIndex].lux = state.lux;
        nodeStates[nodeIndex].pressure_pa = state.pressure_pa;
        nodeStates[nodeIndex].gas_ohm = state.gas_ohm;
        nodeStates[nodeIndex].aqi = state.aqi;
        nodeStates[nodeIndex].tvoc_ppb = state.tvoc_ppb;
        nodeStates[nodeIndex].eco2_ppm = state.eco2_ppm;
        nodeStates[nodeIndex].motion = (state.motion != 0U);
        nodeStates[nodeIndex].fault = (state.fault != 0U);
        return true;
    }
    logf("WARN", "STATE_REPORT Laenge ungueltig fuer %s", nodeStates[nodeIndex].device_id);
    return false;
}

static bool parseBatSenState(size_t nodeIndex, const uint8_t* payload, uint16_t payloadLen) {
    // BAT-SEN sendet kurze Batteriestates. Diese Laengen ueberschneiden
    // sich mit NET-SEN, deshalb darf die Klasse nicht nur aus sizeof()
    // geraten werden.
    if (payloadLen == sizeof(SmartHome::BatteryStateReportPayload)) {
        const SmartHome::BatteryStateReportPayload& state = *reinterpret_cast<const SmartHome::BatteryStateReportPayload*>(payload);
        nodeStates[nodeIndex].battery_pct = state.battery_pct;
        nodeStates[nodeIndex].battery_mv = state.battery_mv;
        nodeStates[nodeIndex].window_open = state.window_open;
        nodeStates[nodeIndex].rain_raw = state.rain_raw;
        nodeStates[nodeIndex].button_flags = state.button_flags;
        nodeStates[nodeIndex].fault = (state.fault != 0U);
        return true;
    }
    if (payloadLen == sizeof(SmartHome::BatteryConfigStateReportPayload)) {
        // Config-State enthaelt dieselben Live-Zustandsfelder plus report_interval_s.
        // Der Master nutzt hier nur den Live-Zustand; daher bleibt der Kopierblock gleich.
        const SmartHome::BatteryConfigStateReportPayload& state = *reinterpret_cast<const SmartHome::BatteryConfigStateReportPayload*>(payload);
        nodeStates[nodeIndex].battery_pct = state.battery_pct;
        nodeStates[nodeIndex].battery_mv = state.battery_mv;
        nodeStates[nodeIndex].window_open = state.window_open;
        nodeStates[nodeIndex].rain_raw = state.rain_raw;
        nodeStates[nodeIndex].button_flags = state.button_flags;
        nodeStates[nodeIndex].fault = (state.fault != 0U);
        return true;
    }
    logf("WARN", "STATE_REPORT Laenge ungueltig fuer %s", nodeStates[nodeIndex].device_id);
    return false;
}

// Aufgabe: Verarbeitet einen eingehenden STATE-Report und uebernimmt ihn in die Node-Registry.
// Eingabewerte:
// - senderMac ist die MAC-Adresse des Senders.
// - payload zeigt auf den Roh-Payload; die device_id steht am Payload-Anfang.
// - payloadLen ist die Payload-Laenge des STATE-Frames.
// Ausgabewert: keiner; nodeStates[], Availability und State-MQTT werden aktualisiert.
//
// Grundregel:
// STATE wird erst nach einem echten oder aus NVS wiederhergestellten HELLO
// geparst. Der Master leitet keine Geraeteklasse aus device_id-Praefixen oder
// Payload-Laengen ab.
void verarbeiteStateReport(const uint8_t* senderMac, const uint8_t* payload, uint16_t payloadLen) {
    if (!payload || payloadLen < SH_DEVICE_ID_LEN) {
        logf("WARN", "STATE_REPORT verworfen: payload ungueltig");
        return;
    }

    // Die device_id liegt am Anfang jeder STATE-Variante. Sie wird bewusst in
    // einen eigenen Puffer kopiert, weil der Roh-Payload nicht zwingend als
    // C-String terminiert sein muss.
    char nodeId[SH_DEVICE_ID_LEN] = {0};
    memcpy(nodeId, payload, SH_DEVICE_ID_LEN - 1U);
    nodeId[SH_DEVICE_ID_LEN - 1U] = '\0';
    if (!SmartHome::isValidDeviceId(nodeId)) {
        logf("WARN", "STATE_REPORT verworfen: ungueltige node_id=%s", nodeId);
        return;
    }

    int nodeIndex = findeNodeIndex(nodeId);
    if (nodeIndex < 0) {
        // Unbekannte Nodes werden leichtgewichtig aufgenommen, damit der Master
        // Kontakt/Availability verfolgen und gleichzeitig ein echtes HELLO
        // nachfordern kann. Die Metadaten bleiben bis dahin unvollstaendig.
        const int r = registriereProvisorischMitId(senderMac, nodeId);
        if (r < 0) {
            logf("WARN", "STATE_REPORT ignoriert: unbekannte node_id=%s (registrieren failed code=%d)", nodeId, r);
            return;
        }
        nodeIndex = r;
    }

    fordereHelloBeiBedarf((size_t)nodeIndex, senderMac, "STATE_REPORT");
    aktualisiereNodeKontakt((size_t)nodeIndex, senderMac);

    // Ohne HELLO-Meta kennt der Master weder Klasse noch Capabilities. Jede
    // Ableitung aus ID oder Byte-Laenge waere ein Sonderfall und kann falsch
    // parsen. Der Node muss auf HELLO_REQUEST mit einem echten HELLO reagieren.
    if (!nodeStates[nodeIndex].meta_bekannt) {
        logf("WARN", "STATE_REPORT von %s wartet auf HELLO: Meta fehlt", nodeId);
        publishNodeAvailability((size_t)nodeIndex);
        return;
    }

    bool parsed = false;
    switch (nodeStates[nodeIndex].device_class) {
        case SH_CLASS_NET_ERL: parsed = parseNetErlState((size_t)nodeIndex, payload, payloadLen); break;

        case SH_CLASS_NET_ZRL: parsed = parseNetZrlState((size_t)nodeIndex, payload, payloadLen); break;

        case SH_CLASS_NET_SEN: parsed = parseNetSenState((size_t)nodeIndex, payload, payloadLen); break;

        case SH_CLASS_BAT_SEN: parsed = parseBatSenState((size_t)nodeIndex, payload, payloadLen); break;

        default:
            logf("WARN", "STATE_REPORT ohne Handler fuer %s", nodeId);
            return;
    }
    if (!parsed) return;

    nodeStates[nodeIndex].state_bekannt = true;
    sanitisiereNodeStateNachCapabilities((size_t)nodeIndex);
    publishNodeState((size_t)nodeIndex);
    publishNodeAvailability((size_t)nodeIndex);
    logf("INFO", "STATE_REPORT von %s verarbeitet", nodeId);
}

// Aufgabe: Verarbeitet einen EVENT-Report einer Node.
// Eingabewerte:
// - senderMac ist die Absender-MAC.
// - payload enthaelt node_id, Event-Typ, Werte und Trigger.
// Ausgabewert: keiner; Kontaktzeit und MQTT-Event werden aktualisiert.
void verarbeiteEventReport(const uint8_t* senderMac, const SmartHome::EventReportPayload& payload) {
    const int nodeIndex = findeNodeIndex(payload.node_id);
    if (nodeIndex < 0) {
        // Events werden nicht provisorisch registriert. Ein Event ohne vorherige
        // HELLO/STATE-Basis haette keine verlaessliche Meta- oder State-Einordnung.
        // Es wird jedoch ein HELLO_REQUEST ausgeloest, damit sich die Node vollstaendig
        // vorstellt. So wird ein Boot-Event (haeufigstes erstes Frame nach Reset) nicht
        // dauerhaft ignoriert.
        logf("WARN", "EVENT ignoriert: unbekannte node_id=%s, HELLO_REQUEST wird angefordert", payload.node_id);
        if (senderMac && SmartHome::isValidMac(senderMac)) {
            sendeHelloRequestAnMac(senderMac);
        }
        return;
    }

    aktualisiereNodeKontakt((size_t)nodeIndex, senderMac);
    publishNodeEvent((size_t)nodeIndex, payload);
    publishNodeAvailability((size_t)nodeIndex);
    logf("INFO", "EVENT von %s: type=%u", payload.node_id, (unsigned)payload.event_type);
}

// Aufgabe: Verarbeitet eine ACK-Bestaetigung von einer Node.
// Eingabewerte:
// - senderMac ist die MAC-Adresse der Node.
// - payload enthaelt ACK-Sequenz, bestaetigten Nachrichtentyp und Status.
// Ausgabewert: keiner; passende Pending-Eintraege werden abgeschlossen.
//
// Ablauf:
// 1. ACK wird der sendenden Node zugeordnet.
// 2. ack_seq und ack_msg_type werden gegen pending_cmd oder pending_cfg gematcht.
// 3. Bei Treffer wird ein MQTT-ACK veroeffentlicht und der Pending-Eintrag geloescht.
// 4. ACKs ohne passenden Pending-Eintrag gelten als veraltet oder doppelt und werden ignoriert.
void verarbeiteAck(const uint8_t* senderMac, const SmartHome::AckPayload& payload) {
    const int nodeIndex = findeNodeIndexPerMac(senderMac);
    if (nodeIndex < 0) {
        logf("WARN", "ACK ignoriert: unbekannte Sender-MAC");
        return;
    }

    if (nodeStates[nodeIndex].pending_cfg.hdr.aktiv &&
        payload.ack_msg_type == SH_MSG_CFG &&
        payload.ack_seq == nodeStates[nodeIndex].pending_cfg.hdr.seq) {
        // CFG-ACK bestaetigt genau eine offene Konfigurationsnachricht. Danach
        // wird der Pending-Slot sofort geloescht, damit neue CFGs erlaubt sind.
        const char* statusText = payload.status == SH_ACK_OK ? "ok" : (payload.status == SH_ACK_REJECTED ? "rejected" : "error");
        publishNodeAck((size_t)nodeIndex, nodeStates[nodeIndex].pending_cfg.hdr.request_id, nodeStates[nodeIndex].pending_cfg.hdr.command_channel, statusText, (int)payload.status, payload.ack_msg_type, payload.ack_seq, "node_ack");
        nodeStates[nodeIndex].pending_cfg = {};
        return;
    }

    if (nodeStates[nodeIndex].pending_cmd.hdr.aktiv &&
        payload.ack_msg_type == SH_MSG_CMD &&
        payload.ack_seq == nodeStates[nodeIndex].pending_cmd.hdr.seq) {
        // CMD-ACK laeuft getrennt von CFG-ACK. Entscheidend sind Nachrichtentyp
        // und Sequenz, nicht nur die Absender-MAC.
        const char* statusText = payload.status == SH_ACK_OK ? "ok" : (payload.status == SH_ACK_REJECTED ? "rejected" : "error");
        publishNodeAck((size_t)nodeIndex, nodeStates[nodeIndex].pending_cmd.hdr.request_id, nodeStates[nodeIndex].pending_cmd.hdr.command_channel, statusText, (int)payload.status, payload.ack_msg_type, payload.ack_seq, "node_ack");
        nodeStates[nodeIndex].pending_cmd = {};
        return;
    }

    // Kein passendes Pending gefunden. Kann nach einem Master-Timeout auftreten,
    // wenn die Node verzoegert ein ACK schickt. Erwartet, kein Fehler.
    logf("INFO", "ACK ignoriert: passt zu keinem offenen Request (verzoegert oder doppelt)");
}

// =============================================================================
// ESP-NOW EMPFANG - Paket validieren und an Handler weiterleiten
// =============================================================================

// Aufgabe: Validiert ein empfangenes ESP-NOW-Paket und leitet es an den passenden Handler weiter.
// Eingabewerte:
// - senderMac ist die MAC-Adresse des Senders.
// - daten zeigt auf die Rohdaten aus Header und Payload.
// - laenge ist die gesamte Rohdatenlaenge.
// Ausgabewert: keiner; gueltige Pakete werden verarbeitet, ungueltige verworfen.
//
// Ablauf:
// 1. Eingabe, Mindestlaenge und CRC werden geprueft.
// 2. Der Header bestimmt msg_type, seq, flags und Payload-Laenge.
// 3. HELLO, HEARTBEAT, STATE, EVENT und ACK werden an eigene Handler verteilt.
// 4. Unbekannte Nachrichtentypen werden geloggt und ignoriert.
void verarbeiteEspNowPaket(const uint8_t* senderMac, const uint8_t* daten, int laenge) {
    // senderMac, daten und laenge werden von onEspNowReceive teilweise
    // geprueft, bleiben aber als defensive Massnahme fuer direkte Aufrufe
    // (z. B. aus Tests) und onEspNowReceive (v2) erhalten.
    if (!senderMac || !daten || laenge < (int)sizeof(SmartHome::MsgHeader)) {
        logf("WARN", "ESP-NOW Paket verworfen: ungueltige Eingabe");
        return;
    }

    // CRC-Pruefung ist nur hier vorhanden und wird nicht doppelt validiert.
    if (!SmartHome::hasValidPacketCrc(daten, (size_t)laenge)) {
        logf("WARN", "ESP-NOW Paket verworfen: CRC/Header ungueltig");
        return;
    }

    const SmartHome::MsgHeader* header = reinterpret_cast<const SmartHome::MsgHeader*>(daten);
    const uint8_t* payload = daten + SH_HEADER_SIZE;

    // Anhand msg_type an den richtigen Handler weiterleiten
    switch (header->msg_type) {
        case SH_MSG_HELLO:
            // HELLO: Neue Node anmelden oder bestehende aktualisieren
            if (header->payload_len == sizeof(SmartHome::HelloPayload)) {
                verarbeiteHello(senderMac, *reinterpret_cast<const SmartHome::HelloPayload*>(payload));
            } else {
                logf("WARN", "HELLO ignoriert: falsche Payload-Laenge %u", (unsigned)header->payload_len);
            }
            break;
        case SH_MSG_HEARTBEAT:
            // HEARTBEAT: Kontaktzeit der Node aktualisieren
            if (header->payload_len == sizeof(SmartHome::HeartbeatPayload)) {
                verarbeiteHeartbeat(senderMac, *reinterpret_cast<const SmartHome::HeartbeatPayload*>(payload));
            } else {
                logf("WARN", "HEARTBEAT ignoriert: falsche Payload-Laenge %u", (unsigned)header->payload_len);
            }
            break;
        case SH_MSG_STATE:
            // STATE: Node-Zustand aktualisieren; konkrete Payload-Variante erkennt verarbeiteStateReport().
            verarbeiteStateReport(senderMac, payload, header->payload_len);
            break;
        case SH_MSG_EVENT:
            if (header->payload_len == sizeof(SmartHome::EventReportPayload)) {
                verarbeiteEventReport(senderMac, *reinterpret_cast<const SmartHome::EventReportPayload*>(payload));
            } else {
                logf("WARN", "EVENT ignoriert: falsche Payload-Laenge %u", (unsigned)header->payload_len);
            }
            break;
        case SH_MSG_ACK:
            if (header->payload_len == sizeof(SmartHome::AckPayload)) {
                verarbeiteAck(senderMac, *reinterpret_cast<const SmartHome::AckPayload*>(payload));
            } else {
                logf("WARN", "ACK ignoriert: falsche Payload-Laenge %u", (unsigned)header->payload_len);
            }
            break;
        default:
            logf("WARN", "ESP-NOW Nachricht ignoriert (msg_type=%u)", header->msg_type);
            break;
    }
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
// Aufgabe: ESP-NOW-Receive-Callback fuer Arduino-ESP32 ab Version 3.
// Eingabewerte: info enthaelt die Absenderadresse, daten/laenge das Rohpaket.
// Ausgabewert: keiner; gueltige Pakete werden an verarbeiteEspNowPaket uebergeben.
void onEspNowReceive(const esp_now_recv_info_t* info, const uint8_t* daten, int laenge) {
    if (info == nullptr) return;
    verarbeiteEspNowPaket(info->src_addr, daten, laenge);
}
#else
// Aufgabe: ESP-NOW-Receive-Callback fuer Arduino-ESP32 Version 2.
// Eingabewerte: senderMac, daten und laenge kommen direkt vom ESP-NOW-Stack.
// Ausgabewert: keiner; gueltige Pakete werden an verarbeiteEspNowPaket uebergeben.
void onEspNowReceive(const uint8_t* senderMac, const uint8_t* daten, int laenge) {
    verarbeiteEspNowPaket(senderMac, daten, laenge);
}
#endif

// Aufgabe: Loggt den Sendezustand eines ESP-NOW-Pakets.
// Eingabewerte: mac ist das Ziel, status ist der ESP-NOW-Sendestatus.
// Ausgabewert: keiner.
void logEspNowSendStatus(const uint8_t* mac, esp_now_send_status_t status) {
    const char* text = macToText(mac);
    logf(
        status == ESP_NOW_SEND_SUCCESS ? "INFO" : "WARN",
        "ESP-NOW Sendestatus an %s: %s",
        text,
        status == ESP_NOW_SEND_SUCCESS ? "OK" : "FEHLER");
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
// Aufgabe: ESP-NOW-Send-Callback fuer Arduino-ESP32 ab Version 3.
// Eingabewerte: info enthaelt die Zieladresse, status den Sendestatus.
// Ausgabewert: keiner.
void onEspNowSent(const wifi_tx_info_t* info, esp_now_send_status_t status) {
    if (info == nullptr) return;
    logEspNowSendStatus(info->des_addr, status);
}
#else
// Aufgabe: ESP-NOW-Send-Callback fuer Arduino-ESP32 Version 2.
// Eingabewerte: mac ist die Zieladresse, status der Sendestatus.
// Ausgabewert: keiner.
void onEspNowSent(const uint8_t* mac, esp_now_send_status_t status) {
    logEspNowSendStatus(mac, status);
}
#endif

// Aufgabe: Sendet eine STATE_REQUEST-Abfrage an eine Node.
// Eingabewert: nodeIndex ist der Index in nodeStates[].
// Ausgabewert: true bedeutet, die Anfrage wurde an ESP-NOW uebergeben.
bool sendeStateRequest(size_t nodeIndex) {
    if (!nodeStates[nodeIndex].mac_bekannt) {
        logf("WARN", "STATE_REQUEST verworfen: MAC fuer %s unbekannt", nodeStates[nodeIndex].device_id);
        return false;
    }

    // STATE_REQUEST ist eine reine Abfrage ohne Pending-Tracking. Die Antwort
    // kommt als eigenstaendiger STATE_REPORT und wird dort verarbeitet.
    SmartHome::CmdPayload payload = {};
    payload.cmd_type = SH_CMD_STATE_REQUEST;
    return sendePaket(nodeStates[nodeIndex].mac, SH_MSG_CMD, &payload, sizeof(payload), "STATE_REQUEST");
}

// Aufgabe: Befuellt den gemeinsamen Pending-Header nach erfolgreichem ESP-NOW-Versand.
static void fuellePendingHeader(PendingHeader* slot, uint8_t seq, const char* requestId, const char* channel) {
    slot->aktiv = true;
    slot->seq  = seq;
    slot->retries = 0U;
    slot->letztes_senden_ms = millis();
    copyText(slot->request_id, sizeof(slot->request_id), requestId);
    copyText(slot->command_channel, sizeof(slot->command_channel), channel ? channel : "command");
}

// Aufgabe: Sendet ein bestaetigungspflichtiges CMD an eine Node und merkt es als pending_cmd.
// Eingabewerte: nodeIndex, cmdType, param1, param2, requestId, Channel und Log-Label.
// Ausgabewert: true bedeutet, das CMD wurde gesendet und als Pending eingetragen.
bool sendeCmdRequest(size_t nodeIndex, uint8_t cmdType, uint8_t param1, uint8_t param2, const char* requestId, const char* channel, const char* label) {
    if (!nodeStates[nodeIndex].mac_bekannt) {
        logf("WARN", "%s verworfen: MAC fuer %s unbekannt", label, nodeStates[nodeIndex].device_id);
        return false;
    }

    // CMDs mit Seiteneffekt werden mit ACK_REQUEST gesendet und lokal gemerkt.
    // Erst ACK oder Timeout erzeugt die finale MQTT-Rueckmeldung.
    SmartHome::CmdPayload payload = {};
    payload.cmd_type = cmdType;
    payload.param1 = param1;
    payload.param2 = param2;

    uint8_t seq = 0U;
    const bool erfolgreich = sendePaketMitOptionen(
        nodeStates[nodeIndex].mac,
        SH_MSG_CMD,
        &payload,
        sizeof(payload),
        label,
        SH_FLAG_ACK_REQUEST,
        false,
        0U,
        &seq);

    if (!erfolgreich) return false;

    // Pending wird erst nach erfolgreichem esp_now_send gesetzt. Sonst koennte
    // ein nicht gesendeter Auftrag spaeter faelschlich timeouten.
    nodeStates[nodeIndex].pending_cmd = {};
    nodeStates[nodeIndex].pending_cmd.cmd_type = cmdType;
    nodeStates[nodeIndex].pending_cmd.param1 = param1;
    nodeStates[nodeIndex].pending_cmd.param2 = param2;
    fuellePendingHeader(&nodeStates[nodeIndex].pending_cmd.hdr, seq, requestId, channel);
    return true;
}

// Aufgabe: Sendet an eine explizite MAC ein HELLO-Request-CMD, damit die Node
// darauf mit einem echten HELLO antwortet. Wird bei provisorischen Slots
// und nach STATE/HEARTBEAT von noch unvollstaendig registrierten Nodes aufgerufen.
bool sendeHelloRequestAnMac(const uint8_t* mac) {
    if (!mac || !SmartHome::isValidMac(mac)) {
        logf("WARN", "HELLO_REQUEST: ungueltige MAC, Abbruch");
        return false;
    }
    const char* mactxt = macToText(mac);
    SmartHome::CmdPayload payload = {};
    payload.cmd_type = SH_CMD_HELLO_REQUEST;
    const bool ok = sendePaket(mac, SH_MSG_CMD, &payload, sizeof(payload), "HELLO_REQUEST");
    if (!ok) logf("WARN", "HELLO_REQUEST: senden an %s fehlgeschlagen", mactxt);
    else     logf("INFO", "HELLO_REQUEST: gesendet an %s", mactxt);
    return ok;
}

// Aufgabe: Sendet ein Relaiskommando an eine Node.
// Eingabewerte: nodeIndex, relayIndex, Zielzustand, requestId und Channel.
// Ausgabewert: true bedeutet, das Kommando wurde als CMD gesendet.
bool sendeRelayCommand(size_t nodeIndex, uint8_t relayIndex, bool relayState, const char* requestId, const char* channel) {
    return sendeCmdRequest(nodeIndex, SH_CMD_SET_RELAY, relayIndex, relayState ? 1U : 0U, requestId, channel, "COMMAND_SET_RELAY");
}

// Aufgabe: Sendet ein Cover-Kommando an eine Node.
// Eingabewerte: nodeIndex, Aktion, Zielposition, requestId und Channel.
// Ausgabewert: true bedeutet, das Kommando wurde als CMD gesendet.
bool sendeCoverCommand(size_t nodeIndex, uint8_t coverAction, uint8_t position, const char* requestId, const char* channel) {
    return sendeCmdRequest(nodeIndex, SH_CMD_COVER, coverAction, position, requestId, channel, "COMMAND_COVER");
}

// Aufgabe: Sendet eine bestaetigungspflichtige CFG-Nachricht an eine Node.
// Eingabewerte: nodeIndex, paramId, value, requestId und Channel.
// Ausgabewert: true bedeutet, die CFG wurde gesendet und als pending_cfg eingetragen.
bool sendeConfigCommand(size_t nodeIndex, uint8_t paramId, uint16_t value, const char* requestId, const char* channel) {
    if (!nodeStates[nodeIndex].mac_bekannt) {
        logf("WARN", "CONFIG_SET verworfen: MAC fuer %s unbekannt", nodeStates[nodeIndex].device_id);
        return false;
    }

    // CFG nutzt denselben Funkpfad wie CMD, aber einen eigenen Payload- und
    // Pending-Typ. Dadurch kann der Master ACKs sauber nach SH_MSG_CFG trennen.
    SmartHome::CfgPayload payload = {};
    payload.param_id = paramId;
    payload.value = value;

    uint8_t seq = 0U;
    const bool erfolgreich = sendePaketMitOptionen(
        nodeStates[nodeIndex].mac,
        SH_MSG_CFG,
        &payload,
        sizeof(payload),
        "CONFIG_SET",
        SH_FLAG_ACK_REQUEST,
        false,
        0U,
        &seq);

    if (!erfolgreich) return false;

    // request_id und command_channel bleiben bis ACK/Timeout erhalten, damit die
    // spaetere MQTT-Antwort exakt zum urspruenglichen Auftrag passt.
    nodeStates[nodeIndex].pending_cfg = {};
    nodeStates[nodeIndex].pending_cfg.param_id = paramId;
    nodeStates[nodeIndex].pending_cfg.value = value;
    fuellePendingHeader(&nodeStates[nodeIndex].pending_cfg.hdr, seq, requestId, channel);
    return true;
}

// Aufgabe: Loggt den letzten MQTT-Verbindungsfehler mit PubSubClient-State.
// Eingabewerte: keine.
// Ausgabewert: keiner.
void loggeMqttConnectFehler() {
    if (mqttBrokerNutzeDirekteIp) {
        char brokerIpText[16] = "0.0.0.0";
        mqttBrokerIp.toString().toCharArray(brokerIpText, sizeof(brokerIpText));
        logf("WARN", "MQTT connect fehlgeschlagen (state=%d, broker=%s:%d, typ=%s)", mqttClient.state(), brokerIpText, MQTT_PORT, mqttBrokerTypText());
        return;
    }
    logf("WARN", "MQTT connect fehlgeschlagen (state=%d, broker=%s:%d, typ=%s)", mqttClient.state(), MQTT_HOST, MQTT_PORT, mqttBrokerTypText());
}

// =============================================================================
// JSON-HELFER - Manuelles JSON-Parsing ohne ArduinoJson-Lib
// =============================================================================

// Aufgabe: Ueberspringt Leerzeichen im JSON-Cursor.
// Eingabewert: cursor zeigt auf die aktuelle Parser-Position und wird weitergeschoben.
// Ausgabewert: true bedeutet, nach dem Ueberspringen steht noch ein Zeichen bereit.
bool skipWhitespace(const char*& cursor) {
    if (cursor == nullptr) return false;
    while (*cursor != '\0' && isspace((unsigned char)*cursor)) cursor++;
    return *cursor != '\0';
}

// Aufgabe: Prueft, ob ein JSON-Wert nach einem Literal sauber beendet ist.
// Eingabewert: cursor zeigt auf das Zeichen nach true, false oder einer Zahl.
// Ausgabewert: true bedeutet Ende, Whitespace, Komma oder Objekt-/Array-Ende.
bool jsonValueTerminated(const char* cursor) {
    if (cursor == nullptr) return false;
    const char* c = cursor;
    while (*c != '\0' && isspace((unsigned char)*c)) c++;
    return *c == '\0' || *c == ',' || *c == '}' || *c == ']';
}

// Aufgabe: Findet den Wert eines Top-Level-Keys in einem JSON-Objekt.
// Eingabewerte:
// - json ist ein JSON-Objekt als Text.
// - key ist der gesuchte Key ohne Anfuehrungszeichen.
// Ausgabewert: Zeiger auf den Wertanfang oder nullptr, wenn der Key nicht existiert.
//
// Der Scanner ignoriert Strings und verschachtelte Objekte. Dadurch matcht ein
// command-Key in values oder in einem String nicht versehentlich den Hauptbefehl.
const char* jsonFindTopLevelValue(const char* json, const char* key) {
    if (!json || !key || key[0] == '\0') return nullptr;
    const size_t keyLen = strlen(key);
    // depth==1 bedeutet: wir befinden uns direkt im uebergebenen Root-Objekt.
    // Damit werden gleichnamige Keys innerhalb von "values" oder Arrays ignoriert.
    int depth = 0;
    bool inString = false;
    bool escaped = false;

    for (const char* cursor = json; *cursor != '\0'; ++cursor) {
        const char c = *cursor;
        if (inString) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') inString = false;
            continue;
        }

        if (c == '"') {
            // Potenziellen Key lesen. Escapes im Key werden uebersprungen, damit
            // ein maskiertes Anfuehrungszeichen nicht faelschlich den Key beendet.
            const char* keyStart = cursor + 1;
            const char* keyEnd = keyStart;
            bool keyEscaped = false;
            while (*keyEnd != '\0') {
                if (keyEscaped) {
                    keyEscaped = false;
                } else if (*keyEnd == '\\') {
                    keyEscaped = true;
                } else if (*keyEnd == '"') {
                    break;
                }
                keyEnd++;
            }
            if (*keyEnd == '\0') return nullptr;

            if (depth == 1 && (size_t)(keyEnd - keyStart) == keyLen && strncmp(keyStart, key, keyLen) == 0) {
                const char* afterKey = keyEnd + 1;
                if (!skipWhitespace(afterKey) || *afterKey != ':') {
                    cursor = keyEnd;
                    continue;
                }
                afterKey++;
                if (!skipWhitespace(afterKey)) return nullptr;
                return afterKey;
            }

            cursor = keyEnd;
            continue;
        }

        if (c == '{' || c == '[') depth++;
        else if ((c == '}' || c == ']') && depth > 0) depth--;
    }

    return nullptr;
}

// Aufgabe: Liest einen JSON-String ab dem Wertanfang und dekodiert einfache Escapes.
// Eingabewerte: cursor zeigt auf das oeffnende Anfuehrungszeichen, ziel ist der Ausgabepuffer.
// Ausgabewert: true bedeutet, der String wurde vollstaendig gelesen.
bool jsonReadStringValue(const char* cursor, char* ziel, size_t zielGroesse) {
    if (!cursor || !ziel || zielGroesse == 0U || *cursor != '"') return false;
    cursor++;
    size_t out = 0U;
    bool escaped = false;

    while (*cursor != '\0') {
        char c = *cursor++;
        if (escaped) {
            switch (c) {
                case '"': c = '"'; break;
                case '\\': c = '\\'; break;
                case '/': c = '/'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                default: break;
            }
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
            continue;
        } else if (c == '"') {
            ziel[out] = '\0';
            return jsonValueTerminated(cursor);
        }

        if (out + 1U < zielGroesse) {
            ziel[out++] = c;
        }
    }

    ziel[out] = '\0';
    return false;
}

// Aufgabe: Liest einen String-Wert aus einem einfachen JSON-Text.
// Eingabewerte: json, key, Zielpuffer und Zielpuffergroesse.
// Ausgabewert: true bedeutet, der String wurde gefunden und kopiert.
bool jsonHoleString(const char* json, const char* key, char* ziel, size_t zielGroesse) {
    const char* cursor = jsonFindTopLevelValue(json, key);
    return jsonReadStringValue(cursor, ziel, zielGroesse);
}

// Aufgabe: Liest einen Boolean-Wert aus einem einfachen JSON-Text.
// Eingabewerte: json, key und Ausgabepointer.
// Ausgabewert: true bedeutet, true oder false wurde gefunden und geschrieben.
bool jsonHoleBool(const char* json, const char* key, bool* wert) {
    if (!wert) return false;
    const char* cursor = jsonFindTopLevelValue(json, key);
    if (!cursor) return false;
    if (strncmp(cursor, "true", 4) == 0 && jsonValueTerminated(cursor + 4)) {
        *wert = true;
        return true;
    }
    if (strncmp(cursor, "false", 5) == 0 && jsonValueTerminated(cursor + 5)) {
        *wert = false;
        return true;
    }
    return false;
}

// Aufgabe: Liest eine Ganzzahl aus einem einfachen JSON-Text.
// Eingabewerte: json, key und Ausgabepointer.
// Ausgabewert: true bedeutet, die Zahl wurde gefunden und geschrieben.
bool jsonHoleZahl(const char* json, const char* key, long* wert) {
    if (!wert) return false;
    const char* cursor = jsonFindTopLevelValue(json, key);
    if (!cursor) return false;
    char* ende = nullptr;
    const long parsed = strtol(cursor, &ende, 10);
    if (ende == cursor) return false;
    // Nur Ganzzahlen sind erlaubt. Dezimal- und Exponentialschreibweise wuerden
    // fuer Protokollfelder wie Sekunden/Positionen unklare Rundung bedeuten.
    if (*ende == '.' || *ende == 'e' || *ende == 'E') return false;
    if (!jsonValueTerminated(ende)) return false;
    *wert = parsed;
    return true;
}

// Aufgabe: Extrahiert ein JSON-Objekt per Key aus einem einfachen JSON-String.
// Eingabewerte:
// - json ist der Eingabestring.
// - key ist der gesuchte Schluessel ohne Anfuehrungszeichen.
// - ziel zeigt auf den Ausgabe-Puffer fuer das gefundene Objekt.
// - zielGroesse ist die verfuegbare Puffergroesse.
// Ausgabewert: true bedeutet, das Objekt wurde gefunden und nach ziel kopiert.
//
// Ablauf:
// 1. Der Key wird per strstr gesucht.
// 2. Nach dem Doppelpunkt wird das erste oeffnende Objektzeichen gesucht.
// 3. Geschweifte Klammern werden ueber eine Tiefe gezaehlt.
// 4. Das Objekt endet, sobald die Tiefe wieder 0 erreicht.
//
// Grenze: Das ist weiterhin ein kleiner Command-Parser, kein vollstaendiger
// JSON-Parser. Verschachtelte Objekte und einfache String-Escapes werden sauber
// behandelt; Unicode-Escapes werden nicht dekodiert.
bool jsonHoleObjekt(const char* json, const char* key, char* ziel, size_t zielGroesse) {
    if (!json || !key || !ziel || zielGroesse == 0U) return false;
    const char* cursor = jsonFindTopLevelValue(json, key);
    if (!cursor || *cursor != '{') return false;
    const char* start = cursor;
    // Das Objekt wird roh kopiert. Die Tiefe verhindert, dass eine innere
    // schliessende Klammer das values-Objekt zu frueh beendet.
    int tiefe = 0;
    bool inString = false;
    bool escaped = false;
    while (*cursor != '\0') {
        const char zeichen = *cursor;
        if (inString) {
            if (escaped) escaped = false;
            else if (zeichen == '\\') escaped = true;
            else if (zeichen == '"') inString = false;
        } else {
            if (zeichen == '"') inString = true;
            else if (zeichen == '{') tiefe++;
            else if (zeichen == '}') {
                tiefe--;
                if (tiefe == 0) {
                    cursor++;
                    const size_t len = (size_t)(cursor - start);
                    if (len >= zielGroesse) return false;
                    memcpy(ziel, start, len);
                    ziel[len] = '\0';
                    return true;
                }
            }
        }
        cursor++;
    }
    return false;
}

// Aufgabe: Parst und validiert ein minimales set_config-Kommando.
// Eingabewerte:
// - nodeIndex ist die Ziel-Node.
// - json enthaelt das MQTT-Kommando.
// - paramId und value zeigen auf die Ausgabewerte fuer die CFG-Nachricht.
// - errorText und errorSize beschreiben den Fehlertextpuffer.
// Ausgabewert: true bedeutet, paramId und value sind gueltig fuer die Ziel-Node.
bool parseSetConfigMinimal(size_t nodeIndex, const char* json, uint8_t* paramId, uint16_t* value, char* errorText, size_t errorSize) {
    if (!paramId || !value || !errorText || errorSize == 0U) return false;
    errorText[0] = '\0';

    char valuesJson[192] = {0};
    if (!jsonHoleObjekt(json, "values", valuesJson, sizeof(valuesJson))) {
        copyText(errorText, errorSize, "set_config verlangt ein values-Objekt");
        return false;
    }

    long numberValue = 0L;
    if (jsonHoleZahl(valuesJson, "report_interval_s", &numberValue)) {
        // Der Wertebereich kommt aus dem Storage-Framework und gilt fuer alle
        // Nodes gleich. Der Master validiert vor dem Senden, damit die Node keine
        // offensichtlich ungueltige CFG verarbeiten muss.
        if (numberValue < CFG_REPORT_INTERVAL_MIN || numberValue > CFG_REPORT_INTERVAL_MAX) {
            snprintf(errorText, errorSize, "report_interval_s ausserhalb %ld..%ld", CFG_REPORT_INTERVAL_MIN, CFG_REPORT_INTERVAL_MAX);
            return false;
        }
        *paramId = SH_CFG_REPORT_INTERVAL_S;
        *value = (uint16_t)numberValue;
        return true;
    }

    snprintf(errorText, errorSize, "Kein unterstuetztes CFG-Feld fuer %s gefunden", deviceClassText(nodeStates[nodeIndex].device_class));
    return false;
}

// =============================================================================
// COMMAND-VALIDIERUNG - Cover-Geraet, Relay-Zulaessigkeit, ACK-Typ
// =============================================================================

// Aufgabe: Prueft, ob eine Node als Cover-Geraet behandelt werden soll.
// Eingabewert: nodeIndex ist der Index in nodeStates[].
// Ausgabewert: true bedeutet Control-Mode Cover oder SH_CAP_COVER.
bool istCoverGeraet(size_t nodeIndex) {
    // Control-Mode ist die bevorzugte Aussage aus HELLO. Die Capability bleibt
    // als Rueckfall erhalten, falls aeltere Nodes noch keinen Control-Mode setzen.
    return nodeStates[nodeIndex].control_mode == SH_CONTROL_MODE_COVER || nodeHasCap(nodeIndex, SH_CAP_COVER);
}

// Aufgabe: Prueft, ob ein Relaiskommando fuer eine Node zulaessig ist.
// Eingabewerte: nodeIndex und relayIndex 0 oder 1.
// Ausgabewert: true bedeutet, die Node ist kein Cover und besitzt das gewuenschte Relais.
bool istRelayBefehlZulaessig(size_t nodeIndex, uint8_t relayIndex) {
    // Cover-Nodes besitzen intern Relais, diese duerfen aber nicht direkt ueber
    // set_relay geschaltet werden. Dafuer gibt es eigene Cover-Kommandos.
    if (istCoverGeraet(nodeIndex)) return false;
    if (relayIndex == 0U) {
        return nodeHasCap(nodeIndex, SH_CAP_RELAY);
    }
    if (relayIndex == 1U) {
        return nodeHasCap(nodeIndex, SH_CAP_RELAY2);
    }
    return false;
}

// Aufgabe: Bestimmt den ACK-Nachrichtentyp passend zum MQTT-Kommando.
// Eingabewert: cmd ist der MQTT-command-Text.
// Ausgabewert: SH_MSG_CFG fuer set_config, sonst SH_MSG_CMD.
uint8_t ackMsgTypeFuerCommand(const char* cmd) {
    return (cmd != nullptr && strcmp(cmd, "set_config") == 0) ? SH_MSG_CFG : SH_MSG_CMD;
}

// Aufgabe: Stellt sicher, dass eine Node fuer sicherheitsrelevante Kommandos
// echte oder wiederhergestellte HELLO-Meta besitzt.
// Eingabewerte: nodeIndex, requestId, commandChannel, ackMsgType und Grundtext.
// Ausgabewert: true bedeutet, die Validierung darf mit Caps/Profile fortfahren.
bool stelleMetaFuerCommandSicher(size_t nodeIndex, const char* requestId, const char* commandChannel, uint8_t ackMsgType, const char* grund) {
    if (nodeStates[nodeIndex].meta_bekannt) return true;

    // Ohne HELLO-Meta kennt der Master keine Capabilities. Dann waere ein
    // "unsupported" fachlich falsch: Das Geraet ist nicht als ungeeignet
    // bewiesen, der Master ist nur noch nicht synchronisiert.
    const uint8_t* mac = nodeStates[nodeIndex].mac_bekannt ? nodeStates[nodeIndex].mac : nullptr;
    fordereHelloBeiBedarf(nodeIndex, mac, grund ? grund : "MQTT_COMMAND");
    publishNodeAck(nodeIndex, requestId, commandChannel, "meta_required", STATUS_CODE_META_REQUIRED, ackMsgType, 0U, "master_registry");
    return false;
}

// Aufgabe: Behandelt das get_state-MQTT-Kommando fuer eine registrierte Node.
static void handleMqttGetState(size_t nodeIndex, const char* requestId, const char* commandChannel) {
    if (!sendeStateRequest(nodeIndex)) {
        publishNodeAck(nodeIndex, requestId, commandChannel, "send_failed", -4, SH_MSG_CMD, 0U, "master_send");
        return;
    }
    publishNodeAck(nodeIndex, requestId, commandChannel, "sent", 0, SH_MSG_CMD, 0U, "master_send");
}

// Aufgabe: Behandelt das set_relay-MQTT-Kommando (relay_1 oder relay_2).
static void handleMqttSetRelay(size_t nodeIndex, const char* requestId, const char* commandChannel, const char* json) {
    if (!stelleMetaFuerCommandSicher(nodeIndex, requestId, commandChannel, SH_MSG_CMD, "MQTT_SET_RELAY")) return;
    if (nodeStates[nodeIndex].pending_cmd.hdr.aktiv) {
        publishNodeAck(nodeIndex, requestId, commandChannel, "busy", -2, SH_MSG_CMD, nodeStates[nodeIndex].pending_cmd.hdr.seq, "master_busy");
        return;
    }

    bool relayState = false;
    if (jsonHoleBool(json, "relay_1", &relayState)) {
        if (!istRelayBefehlZulaessig(nodeIndex, 0U)) {
            publishNodeAck(nodeIndex, requestId, commandChannel, "unsupported", -3, SH_MSG_CMD, 0U, "master_validation");
            return;
        }
        if (!sendeRelayCommand(nodeIndex, 0U, relayState, requestId, commandChannel)) {
            publishNodeAck(nodeIndex, requestId, commandChannel, "send_failed", -4, SH_MSG_CMD, 0U, "master_send");
        }
        return;
    }
    if (jsonHoleBool(json, "relay_2", &relayState)) {
        if (!istRelayBefehlZulaessig(nodeIndex, 1U)) {
            publishNodeAck(nodeIndex, requestId, commandChannel, "unsupported", -3, SH_MSG_CMD, 0U, "master_validation");
            return;
        }
        if (!sendeRelayCommand(nodeIndex, 1U, relayState, requestId, commandChannel)) {
            publishNodeAck(nodeIndex, requestId, commandChannel, "send_failed", -4, SH_MSG_CMD, 0U, "master_send");
        }
        return;
    }
    publishNodeAck(nodeIndex, requestId, commandChannel, "invalid_payload", STATUS_CODE_INVALID_PAYLOAD, SH_MSG_CMD, 0U, "master_validation");
}

// Aufgabe: Behandelt Cover-MQTT-Kommandos (open, close, stop, set_position).
static void handleMqttCoverCommand(size_t nodeIndex, const char* cmd, const char* requestId, const char* commandChannel, const char* json) {
    if (!stelleMetaFuerCommandSicher(nodeIndex, requestId, commandChannel, SH_MSG_CMD, "MQTT_COVER")) return;
    if (!istCoverGeraet(nodeIndex)) {
        publishNodeAck(nodeIndex, requestId, commandChannel, "unsupported", -3, SH_MSG_CMD, 0U, "master_validation");
        return;
    }
    if (nodeStates[nodeIndex].pending_cmd.hdr.aktiv) {
        publishNodeAck(nodeIndex, requestId, commandChannel, "busy", -2, SH_MSG_CMD, nodeStates[nodeIndex].pending_cmd.hdr.seq, "master_busy");
        return;
    }

    if (strcmp(cmd, "open") == 0) {
        if (!sendeCoverCommand(nodeIndex, SH_COVER_CMD_OPEN, 0U, requestId, commandChannel))
            publishNodeAck(nodeIndex, requestId, commandChannel, "send_failed", -4, SH_MSG_CMD, 0U, "master_send");
        return;
    }
    if (strcmp(cmd, "close") == 0) {
        if (!sendeCoverCommand(nodeIndex, SH_COVER_CMD_CLOSE, 0U, requestId, commandChannel))
            publishNodeAck(nodeIndex, requestId, commandChannel, "send_failed", -4, SH_MSG_CMD, 0U, "master_send");
        return;
    }
    if (strcmp(cmd, "stop") == 0) {
        if (!sendeCoverCommand(nodeIndex, SH_COVER_CMD_STOP, 0U, requestId, commandChannel))
            publishNodeAck(nodeIndex, requestId, commandChannel, "send_failed", -4, SH_MSG_CMD, 0U, "master_send");
        return;
    }

    // set_position
    long position = -1L;
    if (!jsonHoleZahl(json, "position", &position) || position < 0L || position > 100L) {
        publishNodeAck(nodeIndex, requestId, commandChannel, "invalid_payload", STATUS_CODE_INVALID_PAYLOAD, SH_MSG_CMD, 0U, "master_validation");
        return;
    }
    if (!nodeStates[nodeIndex].cover_calibrated && position != 0L && position != 100L) {
        publishNodeAck(nodeIndex, requestId, commandChannel, "not_calibrated", STATUS_CODE_NOT_CALIBRATED, SH_MSG_CMD, 0U, "master_validation");
        return;
    }
    if (!sendeCoverCommand(nodeIndex, SH_COVER_CMD_SET_POSITION, (uint8_t)position, requestId, commandChannel))
        publishNodeAck(nodeIndex, requestId, commandChannel, "send_failed", -4, SH_MSG_CMD, 0U, "master_send");
}

// Aufgabe: Behandelt das set_config-MQTT-Kommando.
static void handleMqttSetConfig(size_t nodeIndex, const char* requestId, const char* commandChannel, const char* json) {
    if (!stelleMetaFuerCommandSicher(nodeIndex, requestId, commandChannel, SH_MSG_CFG, "MQTT_SET_CONFIG")) return;
    if (nodeStates[nodeIndex].pending_cfg.hdr.aktiv) {
        publishNodeAck(nodeIndex, requestId, commandChannel, "busy", -2, SH_MSG_CFG, nodeStates[nodeIndex].pending_cfg.hdr.seq, "master_busy");
        return;
    }

    uint8_t paramId = 0U;
    uint16_t value = 0U;
    char errorText[96] = {0};
    if (!parseSetConfigMinimal(nodeIndex, json, &paramId, &value, errorText, sizeof(errorText))) {
        logf("WARN", "set_config fuer %s verworfen: %s", nodeStates[nodeIndex].device_id, errorText);
        publishNodeAck(nodeIndex, requestId, commandChannel, "invalid_payload", STATUS_CODE_INVALID_PAYLOAD, SH_MSG_CFG, 0U, "master_validation");
        return;
    }
    if (!sendeConfigCommand(nodeIndex, paramId, value, requestId, commandChannel)) {
        publishNodeAck(nodeIndex, requestId, commandChannel, "send_failed", -4, SH_MSG_CFG, 0U, "master_send");
    }
}

// =============================================================================
// MQTT-COMMAND-HANDLER - 4 Kommandos: get_state, set_relay, Cover, set_config
// =============================================================================

// Aufgabe: Verarbeitet eingehende MQTT-Kommandos vom Broker.
// Eingabewerte:
// - topic enthaelt smarthome/device/{node_id}/command.
// - payload enthaelt das JSON-Kommando.
// - length ist die Payload-Laenge in Bytes.
// Ausgabewert: keiner; gueltige Kommandos werden als ESP-NOW-Befehl oder ACK umgesetzt.
//
// Unterstuetzte Kommandos:
// 1. get_state sendet eine STATE_REQUEST-Abfrage an die Node.
// 2. set_relay sendet ein Relaiskommando und erwartet ein ACK.
// 3. open, close, stop und set_position senden Cover-Kommandos und erwarten ein ACK.
// 4. set_config sendet eine CFG-Nachricht und erwartet ein ACK.
//
// Alle bestaetigungspflichtigen Kommandos brauchen eine request_id, damit die
// MQTT-ACK-Antwort eindeutig dem urspruenglichen Auftrag zugeordnet werden kann.
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // PubSubClient liefert Payload nicht nullterminiert. Der Master arbeitet
    // bewusst mit einem begrenzten Stack-Puffer, damit fehlerhafte MQTT-Payloads
    // nicht den RAM sprengen.
    char json[256] = {0};
    size_t copyLen = length;
    if (copyLen >= sizeof(json)) copyLen = sizeof(json) - 1U;
    memcpy(json, payload, copyLen);
    json[copyLen] = '\0';
    if (copyLen > 0U && json[copyLen - 1U] == '\n') {
        json[copyLen - 1U] = '\0';
    }

    const char* prefix = "smarthome/device/";
    if (strncmp(topic, prefix, strlen(prefix)) != 0) {
        logf("WARN", "MQTT Topic ignoriert: %s", topic);
        return;
    }

    char nodeId[SH_DEVICE_ID_LEN] = {0};
    const char* start = topic + strlen(prefix);
    const char* slash = strchr(start, '/');
    // Nur der eindeutige Command-Kanal ist erlaubt. Andere Device-Topics werden
    // hier ignoriert, damit Status-/State-Retained-Messages nicht als Befehle gelten.
    if (!slash || strcmp(slash, "/command") != 0) {
        logf("WARN", "MQTT Topic ignoriert: %s", topic);
        return;
    }

    size_t nodeLen = (size_t)(slash - start);
    if (nodeLen == 0U || nodeLen >= sizeof(nodeId)) {
        logf("WARN", "MQTT Topic ohne gueltige node_id: %s", topic);
        return;
    }
    memcpy(nodeId, start, nodeLen);
    nodeId[nodeLen] = '\0';

    char cmd[32] = {0};
    if (!jsonHoleString(json, "command", cmd, sizeof(cmd)) || cmd[0] == '\0') {
        logf("WARN", "MQTT command ohne gueltiges command-Feld fuer %s", nodeId);
        return;
    }

    logf("INFO", "MQTT empfangen %s -> %s", topic, json);

    const int nodeIndex = findeNodeIndex(nodeId);

    char requestId[REQUEST_ID_LEN] = {0};
    // request_id ist Pflicht fuer MQTT-Kommandos, damit der Server eine Rueckmeldung zuordnen kann.
    if (!jsonHoleString(json, "request_id", requestId, sizeof(requestId)) || requestId[0] == '\0') {
        logf("WARN", "MQTT %s ohne request_id fuer %s verworfen", cmd, nodeId);
        return;
    }

    const char* commandChannel = "command";
    const uint8_t ackMsgType = ackMsgTypeFuerCommand(cmd);

    // Wenn nodeIndex < 0: Node nicht registriert, deshalb sofort ACK mit "unknown_device".
    if (nodeIndex < 0) {
        publishNodeAckById(nodeId, requestId, commandChannel, "unknown_device", STATUS_CODE_UNKNOWN_DEVICE, ackMsgType, 0U, "master_registry");
        return;
    }

    if (strcmp(cmd, "get_state") == 0) {
        handleMqttGetState((size_t)nodeIndex, requestId, commandChannel);
        return;
    }
    if (strcmp(cmd, "set_relay") == 0) {
        handleMqttSetRelay((size_t)nodeIndex, requestId, commandChannel, json);
        return;
    }
    if (strcmp(cmd, "open") == 0 || strcmp(cmd, "close") == 0 || strcmp(cmd, "stop") == 0 || strcmp(cmd, "set_position") == 0) {
        handleMqttCoverCommand((size_t)nodeIndex, cmd, requestId, commandChannel, json);
        return;
    }
    if (strcmp(cmd, "set_config") == 0) {
        handleMqttSetConfig((size_t)nodeIndex, requestId, commandChannel, json);
        return;
    }

    // Unreachable: alle Kommandos werden oben abgefangen. Nur noch als
    // Sicherheitsnetz fuer zukuenftige, hier nicht behandelte Kommandos.
    logf("WARN", "MQTT Kommando ignoriert fuer %s", nodeId);
}

// =============================================================================
// INITIALISIERUNG - Hardware, WLAN, ESP-NOW, MQTT
// =============================================================================

// Aufgabe: Initialisiert optionale lokale Master-Hardware.
// Eingabewerte: keine; Pins kommen aus PinConfig.h.
// Ausgabewert: keiner; aktuell wird nur eine optionale Status-LED vorbereitet.
void initialisiereHardware() {
    if (PIN_STATUS_LED >= 0) {
        pinMode(PIN_STATUS_LED, OUTPUT);
        digitalWrite(PIN_STATUS_LED, LOW);
    }
}

// Aufgabe: Startet WLAN im Station-Modus und initialisiert den Watchdog.
// Eingabewerte: keine; Zugangsdaten und Timeout kommen aus Konfiguration und Secrets.h.
// Ausgabewert: keiner; der Verbindungsaufbau laeuft asynchron weiter.
void initialisiereWlan() {
    // Der Master nutzt WLAN im STA-Modus, weil ESP-NOW und MQTT parallel laufen.
    // Sleep wird deaktiviert, damit ESP-NOW-Empfang und Broker-Verbindung stabil bleiben.
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    masterStatus.letzter_wlan_versuch_ms = millis();
    logf("INFO", "WLAN-Verbindung gestartet: SSID=%s", WIFI_SSID);

    // Watchdog initialisieren. Wenn MQTT/WLAN/ESP-NOW haengen, soll der Master
    // hart neu starten statt still keine Node-Kommunikation mehr zu vermitteln.
    esp_task_wdt_deinit();
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = (uint32_t)MASTER_WDT_TIMEOUT_S * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL);
}

// Aufgabe: Prueft den WLAN-Zustand und startet bei Bedarf einen Reconnect.
// Eingabewerte: keine.
// Ausgabewert: keiner; masterStatus.wlan_verbunden wird aktualisiert.
void pruefeWlanVerbindung() {
    const bool verbunden = (WiFi.status() == WL_CONNECTED);
    if (verbunden != masterStatus.wlan_verbunden) {
        masterStatus.wlan_verbunden = verbunden;
        if (verbunden) {
            logf("INFO", "WLAN verbunden: IP=%s Kanal=%d", WiFi.localIP().toString().c_str(), WiFi.channel());
        } else {
            logf("WARN", "WLAN getrennt");
        }
    }

    if (!verbunden && (millis() - masterStatus.letzter_wlan_versuch_ms) >= WIFI_RECONNECT_INTERVAL_MS) {
        WiFi.reconnect();
        masterStatus.letzter_wlan_versuch_ms = millis();
        logf("INFO", "WLAN-Reconnect gestartet");
    }
}

// Aufgabe: Initialisiert ESP-NOW und registriert Send-/Receive-Callbacks.
// Eingabewerte: keine; WLAN-Kanal kommt aus AppConfig.h.
// Ausgabewert: keiner; masterStatus.espnow_bereit zeigt den Erfolg.
void initialisiereEspNow() {
    if (!masterStatus.wlan_verbunden) {
        // Ohne WLAN-Verbindung wird der konfigurierte Projektkanal fest gesetzt.
        // Ist WLAN verbunden, bestimmt der Access Point den Kanal.
        esp_wifi_set_channel((uint8_t)WLAN_KANAL, WIFI_SECOND_CHAN_NONE);
    }

    if (esp_now_init() != ESP_OK) {
        masterStatus.espnow_bereit = false;
        logf("ERROR", "ESP-NOW Init fehlgeschlagen");
        return;
    }

    esp_now_register_send_cb(onEspNowSent);
    esp_now_register_recv_cb(onEspNowReceive);
    masterStatus.espnow_bereit = true;
    logf("INFO", "ESP-NOW bereit");
}

// Aufgabe: Konfiguriert MQTT-Broker, Callback und Puffer.
// Eingabewerte: keine; Brokerdaten kommen aus Secrets.h oder Fallback-Werten.
// Ausgabewert: keiner.
void initialisiereMqtt() {
    mqttBrokerNutzeDirekteIp = mqttBrokerIp.fromString(MQTT_HOST);
    if (mqttBrokerNutzeDirekteIp) mqttClient.setServer(mqttBrokerIp, MQTT_PORT);
    else mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(MQTT_BUFFER_BYTES);
    logf("INFO", "MQTT konfiguriert: %s:%d (typ=%s)", MQTT_HOST, MQTT_PORT, mqttBrokerTypText());
}

// Aufgabe: Haelt die MQTT-Verbindung am Leben und verbindet bei Bedarf neu.
// Eingabewerte: keine.
// Ausgabewert: keiner; bei Reconnect werden bekannte Node-Daten erneut publiziert.
void pruefeMqttVerbindung() {
    if (!masterStatus.wlan_verbunden) {
        // Ohne WLAN ist MQTT nicht sinnvoll erreichbar. Der Status wird sauber
        // zurueckgesetzt, damit ein spaeterer Reconnect alle retained Topics neu sendet.
        if (masterStatus.mqtt_verbunden) {
            mqttClient.disconnect();
            masterStatus.mqtt_verbunden = false;
        }
        return;
    }

    if (mqttClient.connected()) {
        // PubSubClient braucht regelmaessige loop()-Aufrufe, sonst laufen
        // Keepalive und eingehende Command-Callbacks nicht.
        mqttClient.loop();
        if (!masterStatus.mqtt_verbunden) masterStatus.mqtt_verbunden = true;
        return;
    }

    masterStatus.mqtt_verbunden = false;
    if ((millis() - masterStatus.letzter_mqtt_versuch_ms) < MQTT_RECONNECT_INTERVAL_MS) {
        return;
    }

    masterStatus.letzter_mqtt_versuch_ms = millis();

    char willTopic[96] = {0};
    char willPayload[192] = {0};
    // Last-Will: Broker publiziert offline, wenn der Master hart verschwindet.
    baueMasterTopic("status", willTopic, sizeof(willTopic));
    baueMasterStatusJson(willPayload, sizeof(willPayload), false);

    const bool verbunden = mqttClient.connect(
        DEVICE_ID,
        MQTT_USER,
        MQTT_PASSWORD,
        willTopic,
        0,
        true,
        willPayload,
        true);

    if (!verbunden) {
        loggeMqttConnectFehler();
        return;
    }

    masterStatus.mqtt_verbunden = true;
    logf("INFO", "MQTT verbunden");

    if (!mqttClient.subscribe(MQTT_TOPIC_COMMAND_SUB)) {
        logf("WARN", "MQTT Subscribe fehlgeschlagen: %s", MQTT_TOPIC_COMMAND_SUB);
    }

    publishBekannteNodesNachReconnect();
}

// =============================================================================
// PENDING-TIMEOUTS - CMD und CFG Timeouts mit Retry-Logik
// =============================================================================

// Aufgabe: Gemeinsame Timeout-/Retry-Logik fuer CMD- und CFG-Pending-Slots.
// Eingabewerte:
// - pendSlot: Member-Pointer auf das Pending-Feld in NodeRuntime
// - msgType: SH_MSG_CMD oder SH_MSG_CFG
// - logLabel: Kurzer Log-Prefix ("CMD" oder "CFG")
// - retryLogLabel: Label fuer den ESP-NOW-Retry-Sendevorgang
// - fillPayload: Callback zum Befuellen des Spezial-Payloads aus dem Pending-Slot
// Ausgabewert: keiner.
template<typename Pending, typename Payload>
static void pruefePendingTimeoutsImpl(
    Pending NodeRuntime::*pendSlot,
    uint8_t msgType,
    const char* logLabel,
    const char* retryLogLabel,
    void (*fillPayload)(Payload&, const Pending&))
{
    for (size_t i = 0; i < MAX_DYNAMIC_NODES; ++i) {
        if (!nodeStates[i].belegt) continue;
        Pending& pend = nodeStates[i].*pendSlot;
        if (!pend.hdr.aktiv) continue;
        if ((millis() - pend.hdr.letztes_senden_ms) < COMMAND_ACK_TIMEOUT_MS) continue;

        // MAC muss bekannt sein, bevor ein Retry versucht wird. Ist sie unbekannt,
        // gibt es keine Route zur Node und der Auftrag muss sofort abgebrochen werden.
        if (!nodeStates[i].mac_bekannt) {
            logf("WARN", "%s Retry abgebrochen: MAC fuer %s unbekannt", logLabel, nodeStates[i].device_id);
            publishNodeAck(i, pend.hdr.request_id, pend.hdr.command_channel, "no_route", STATUS_CODE_NO_ROUTE, msgType, pend.hdr.seq, "master_no_route");
            pend = {};
            continue;
        }

        if (pend.hdr.retries < COMMAND_MAX_RETRIES) {
            // Retry mit unveraenderter Sequenznummer: Ein spaetes ACK bestaetigt
            // weiterhin denselben MQTT-Auftrag.
            Payload payload = {};
            fillPayload(payload, pend);
            const bool gesendet = sendePaketMitOptionen(
                nodeStates[i].mac,
                msgType,
                &payload,
                sizeof(Payload),
                retryLogLabel,
                (uint8_t)(SH_FLAG_ACK_REQUEST | SH_FLAG_RETRANSMIT),
                true,
                pend.hdr.seq,
                nullptr);
            if (gesendet) {
                pend.hdr.retries++;
                pend.hdr.letztes_senden_ms = millis();
                continue;
            }
            // Sendefehler beim Retry: sofort abbrechen, nicht als Timeout melden.
            logf("WARN", "%s Retry send-fail fuer %s, Auftrag wird abgebrochen", logLabel, nodeStates[i].device_id);
            publishNodeAck(i, pend.hdr.request_id, pend.hdr.command_channel, "send_failed", -4, msgType, pend.hdr.seq, "master_send");
            pend = {};
            continue;
        }

        // Retries erschoepft: MQTT-Auftrag aktiv beenden. Ohne dieses ACK wuerde
        // der Server endlos auf Antwort warten.
        publishNodeAck(i, pend.hdr.request_id, pend.hdr.command_channel, "timeout", (int)SH_ERROR_ACK_TIMEOUT, msgType, pend.hdr.seq, "master_timeout");
        pend = {};
    }
}

// Aufgabe: Prueft ausstehende CMD-ACKs und sendet Retries oder Timeout-ACKs.
static void pruefePendingCmdTimeouts() {
    auto fillCmd = [](SmartHome::CmdPayload& p, const PendingCmdRequest& pend) {
        p.cmd_type = pend.cmd_type;
        p.param1   = pend.param1;
        p.param2   = pend.param2;
    };
    pruefePendingTimeoutsImpl<PendingCmdRequest, SmartHome::CmdPayload>(
        &NodeRuntime::pending_cmd, SH_MSG_CMD, "CMD", "COMMAND retry", fillCmd);
}

// Aufgabe: Prueft ausstehende CFG-ACKs und sendet Retries oder Timeout-ACKs.
static void pruefePendingCfgTimeouts() {
    // CFG-Retry ist getrennt von CMD-Retry, weil Nodes ACKs mit
    // ack_msg_type unterscheiden und Konfigurationsfehler anders melden.
    auto fillCfg = [](SmartHome::CfgPayload& p, const PendingConfigRequest& pend) {
        p.param_id = pend.param_id;
        p.value    = pend.value;
    };
    pruefePendingTimeoutsImpl<PendingConfigRequest, SmartHome::CfgPayload>(
        &NodeRuntime::pending_cfg, SH_MSG_CFG, "CFG", "CONFIG retry", fillCfg);
}

// Aufgabe: Markiert Nodes nach Ablauf ihres Offline-Timeouts als nicht online.
// Eingabewerte: keine.
// Ausgabewert: keiner; Availability wird bei Statuswechsel publiziert.
void pruefeOfflineTimeout() {
    for (size_t i = 0; i < MAX_DYNAMIC_NODES; ++i) {
        if (!nodeStates[i].belegt || !nodeStates[i].online) continue;
        // Batteriegeraete duerfen laenger still sein als Netzgeraete. Die
        // Entscheidung liegt zentral in offlineTimeoutMsForPowerType().
        if ((millis() - nodeStates[i].letzter_kontakt_ms) > offlineTimeoutMsForPowerType(nodeStates[i].power_type)) {
            nodeStates[i].online = false;
            publishNodeAvailability(i);
            logf("WARN", "Node %s nicht mehr online (availability=%s)", nodeStates[i].device_id, availabilityStateText(i));
        }
    }
}

// Aufgabe: Entfernt leichte Registry-Slots, die nie durch ein echtes HELLO bestaetigt wurden.
// Eingabewerte: keine.
// Ausgabewert: keiner; veraltete Slots werden freigegeben und als offline publiziert.
void pruefeProvisorischeNodeTtl() {
    for (size_t i = 0; i < MAX_DYNAMIC_NODES; ++i) {
        if (!nodeStates[i].belegt || !nodeStates[i].provisorisch || nodeStates[i].meta_bekannt) continue;
        if ((millis() - nodeStates[i].letzter_kontakt_ms) <= PROVISIONAL_NODE_TTL_MS) continue;

        // Erst offline publizieren, dann Slot leeren. So bleibt serverseitig
        // nachvollziehbar, dass der nur vermutete Kontakt wieder verschwunden ist.
        // letzter_kontakt_ms wird auf 0 gesetzt, damit availabilityStateText() sicher
        // "offline" zurueckgibt und nicht "late" (delta liegt kurz nach TTL-Ablauf
        // noch im late-Fenster der Batterie-Nodes).
        char deviceId[SH_DEVICE_ID_LEN] = {0};
        copyText(deviceId, sizeof(deviceId), nodeStates[i].device_id);
        nodeStates[i].online = false;
        nodeStates[i].letzter_kontakt_ms = 0UL;
        publishNodeAvailability(i);
        initialisiereNodeSlot(nodeStates[i]);
        logf("WARN", "Provisorische Node %s geloescht: kein HELLO innerhalb TTL", deviceId);
    }
}

// =============================================================================
// STARTMELDUNG - Konsolenausgabe beim Bootvorgang
// =============================================================================

// Aufgabe: Gibt eine kompakte Startmeldung auf Serial aus.
// Eingabewerte: keine.
// Ausgabewert: keiner; bei deaktiviertem Debug wird nichts ausgegeben.
void gibStartmeldungAus() {
    if (!DEBUG_LOKAL_AKTIV) return;

    Serial.println("================================");
    Serial.println(PROJECT_NAME);
    Serial.print(DATEI_GERAET);
    Serial.print(" v");
    Serial.println(DATEI_VERSION);
    Serial.print("FW: ");
    Serial.println(PROJECT_VERSION);
    Serial.print("Variante: ");
    Serial.println(FW_VARIANT);
    Serial.println("Master-Stand:");
    Serial.println(" - dynamische Node-Registry");
    Serial.println(" - HELLO / HELLO_ACK / HEARTBEAT / STATE / EVENT / ACK");
    Serial.println(" - set_relay / get_state / set_config(report_interval_s)");
    Serial.println(" - cover: open / close / stop / set_position");
    Serial.println(" - Pending/ACK pro Geraet");
    Serial.print("Max. dynamische Nodes: ");
    Serial.println(MAX_DYNAMIC_NODES);
    Serial.println("================================");
}

}  // namespace

// =============================================================================
// ARDUINO ENTRY POINTS - Initialisierung und Hauptschleife
// =============================================================================

// Aufgabe: Initialisiert die Master-Firmware beim Boot.
// Eingabewerte: keine.
// Ausgabewert: keiner; danach uebernimmt loop() die zyklische Kommunikation.
//
// Ablauf:
// 1. Serial wird bei aktivem Debug gestartet.
// 2. Master-Status und Node-Registry werden zurueckgesetzt.
// 3. Startmeldung und optionale Hardware werden initialisiert.
// 4. WLAN, ESP-NOW und MQTT werden vorbereitet.
void setup() {
    if (DEBUG_LOKAL_AKTIV) {
        Serial.begin(115200);
        delay(150);
    }

    // Runtime-Zustand bleibt fluechtig, aber stabile HELLO-Meta wird aus NVS
    // geladen. Dadurch kennt der Master nach Neustart wieder Klassen und Caps.
    masterStatus = {};
    initialisiereNodeStates();
    ladeRegistrySnapshot();
    gibStartmeldungAus();
    initialisiereHardware();
    initialisiereWlan();
    delay(500);
    pruefeWlanVerbindung();
    initialisiereEspNow();
    initialisiereMqtt();
}

// Aufgabe: Fuehrt die zyklische Master-Hauptschleife aus.
// Eingabewerte: keine.
// Ausgabewert: keiner; Verbindungen, Pending-Requests und Offline-Status werden gepflegt.
//
// Ablauf:
// 1. Watchdog zuruecksetzen.
// 2. WLAN- und MQTT-Verbindung pruefen oder wiederherstellen.
// 3. Pending-CMD- und Pending-CFG-Timeouts abarbeiten.
// 4. Node-Offline-Timeouts und provisorische Registry-Slots pruefen.
// 5. LOOP_INTERVAL_MS warten.
void loop() {
    // Die loop bleibt bewusst klein: Verbindungsmanagement, ausstehende
    // Funkauftraege und Registry-Lebenszeichen werden in getrennten Helfern
    // abgearbeitet. So bleibt jeder Fehlerpfad isoliert pruefbar.
    esp_task_wdt_reset();
    pruefeWlanVerbindung();
    pruefeMqttVerbindung();
    pruefePendingCmdTimeouts();
    pruefePendingCfgTimeouts();
    pruefeOfflineTimeout();
    pruefeProvisorischeNodeTtl();
    delay(LOOP_INTERVAL_MS);
}
