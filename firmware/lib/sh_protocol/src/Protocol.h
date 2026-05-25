/*
===============================================================================
 Datei: Protocol.h
 Code-Name: Protocol
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / gemeinsame Bibliothek
 Ersteller: DevOpsOfChaos
 Letzte Bearbeitung: 2026-05-18

 Zweck: ESP-NOW-Protokoll
 Beschreibung: Definiert Nachrichten, Nutzdaten und Prueffunktionen fuer die Funkkommunikation.

 Genutzte Bibliotheken:
 - Arduino.h: Arduino-/ESP32-Basistypen und Hilfsfunktionen.
 - string.h: memcpy, strncpy, strlen und snprintf fuer sichere Pufferarbeit.
 - stdint.h: feste Ganzzahltypen fuer das binaere Protokoll.
 - ctype.h: Zeichenpruefung fuer Device-IDs und Masken.
 - DeviceTypes.h: eigene Bibliothek mit Geraeteklassen und Faehigkeiten.

 Aenderungsverlauf:
 - 2026-05-18: Kommentarstil vereinheitlicht und Doxygen-Metakommentare entfernt.
===============================================================================
*/
#pragma once

#include <Arduino.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#include "DeviceTypes.h"

// ============================================================
// PROTOKOLL-KONSTANTEN - Magic, Version, Puffergroessen
// ============================================================

#define SH_PROTO_MAGIC         0xA5U
#define SH_PROTO_VERSION       1U

#define SH_ESPNOW_MAX_BYTES    250U
#define SH_HEADER_SIZE         10U
#define SH_MAX_PAYLOAD_BYTES   (SH_ESPNOW_MAX_BYTES - SH_HEADER_SIZE)

#define SH_DEVICE_ID_LEN       16U
#define SH_DEVICE_NAME_LEN     32U
#define SH_SENSOR_MASK_LEN     11U
#define SH_INPUT_MASK_LEN      6U
#define SH_MAX_DEVICES         32U

// ============================================================
// NACHRICHTENTYPEN (msg_type) - Identifiziert den Payload-Typ
// ============================================================

#define SH_MSG_HELLO           0x01U
#define SH_MSG_HELLO_ACK       0x02U
#define SH_MSG_STATE           0x03U
#define SH_MSG_EVENT           0x04U
#define SH_MSG_CMD             0x05U
#define SH_MSG_CFG             0x06U
#define SH_MSG_ACK             0x07U
#define SH_MSG_HEARTBEAT       0x09U

// ============================================================
// HEADER-FLAGS - Steuerbits im MsgHeader
// ============================================================

#define SH_FLAG_ACK_REQUEST    0x01U
#define SH_FLAG_RETRANSMIT     0x02U
#define SH_FLAG_ENCRYPTED      0x04U

// ============================================================
// ACK-STATUS - Rueckgabewerte fuer ACK-Nachrichten
// ============================================================

#define SH_ACK_OK              0x00U
#define SH_ACK_ERROR           0x01U
#define SH_ACK_REJECTED        0x02U
#define SH_ACK_REJECTED_FULL   0x03U  // Registry voll, keine Slots frei

// ============================================================
// KOMMANDO-TYPEN (cmd_type) - Aktionen die der Master anfordert
// ============================================================

#define SH_CMD_RELAY           0x01U
#define SH_CMD_COVER           0x02U
#define SH_CMD_STATE_REQUEST   0x03U
#define SH_CMD_REBOOT          0x04U
#define SH_CMD_SET_RELAY       SH_CMD_RELAY
// Master an Node: Fordert die Node auf, ein HELLO zu senden (wenn vorhanden)
#define SH_CMD_HELLO_REQUEST   0x06U

// ============================================================
// COVER-KOMMANDOS - Unterkommandos fuer SH_CMD_COVER
// ============================================================

#define SH_COVER_CMD_OPEN          0x01U
#define SH_COVER_CMD_CLOSE         0x02U
#define SH_COVER_CMD_STOP          0x03U
#define SH_COVER_CMD_SET_POSITION  0x04U

// ============================================================
// KONFIGURATIONS-PARAMETER (param_id) - Werte die per CFG gesetzt werden
//   0x01-0x06: Basis-Parameter
//   0x10-0x13: Sensor-Parameter (Delta, Hysterese)
//   0x20-0x25: Relay/Automation-Parameter
//   0x30-0x33: Cover-Parameter (Fahrzeit, Kalibrierung)
//   0x40-0x42: Battery-Parameter (Wake, RX, Low-Battery)
//   0x50-0x52: Ring/NeoPixel-Parameter
// ============================================================

#define SH_CFG_DEVICE_NAME            0x01U
#define SH_CFG_REPORT_INTERVAL_S      0x02U
#define SH_CFG_ACK_TIMEOUT_MS         0x03U
#define SH_CFG_MAX_RETRIES            0x04U
#define SH_CFG_EVENT_DEBOUNCE_MS      0x05U
#define SH_CFG_LED_ENABLED            0x06U

#define SH_CFG_TEMP_DELTA_01C         0x10U
#define SH_CFG_HUM_DELTA_01PCT        0x11U
#define SH_CFG_LUX_DELTA              0x12U
#define SH_CFG_PRESENCE_HOLD_S        0x13U

#define SH_CFG_RELAY_MODE             0x20U
#define SH_CFG_AUTO_OFF_DELAY_S       0x21U
#define SH_CFG_LIGHT_THRESHOLD_ON     0x22U
#define SH_CFG_LIGHT_THRESHOLD_OFF    0x23U
#define SH_CFG_RELAY_DEFAULT_ON_BOOT  0x24U
#define SH_CFG_AUTOMATION_ENABLED     0x25U

#define SH_CFG_COVER_RUN_UP_MS        0x30U
#define SH_CFG_COVER_RUN_DOWN_MS      0x31U
#define SH_CFG_COVER_REVERSE_LOCK_MS  0x32U
#define SH_CFG_COVER_CALIBRATED       0x33U

#define SH_CFG_WAKE_INTERVAL_S        0x40U
#define SH_CFG_RX_WINDOW_MS           0x41U
#define SH_CFG_LOW_BATTERY_PCT        0x42U

#define SH_CFG_RING_ENABLED           0x50U
#define SH_CFG_RING_BRIGHTNESS        0x51U
#define SH_CFG_RING_MODE              0x52U

// ============================================================
// EVENT-TYPEN - Semantische Ereignisse die ein Node melden kann
// ============================================================

#define SH_EVENT_BUTTON_PRESS         0x01U
#define SH_EVENT_MOTION_DETECTED      0x02U
#define SH_EVENT_WINDOW_OPENED        0x03U
#define SH_EVENT_WINDOW_CLOSED        0x04U
#define SH_EVENT_RAIN_DETECTED        0x05U
#define SH_EVENT_RELAY_CHANGED        0x06U
#define SH_EVENT_LIGHT_AUTO_ON        0x07U
#define SH_EVENT_LIGHT_AUTO_OFF       0x08U
#define SH_EVENT_COVER_UP             0x09U
#define SH_EVENT_COVER_DOWN           0x0AU
#define SH_EVENT_COVER_STOP           0x0BU
#define SH_EVENT_COVER_CALIB_START    0x0CU
#define SH_EVENT_COVER_CALIB_DONE     0x0DU
#define SH_EVENT_NODE_BOOT            0x0EU
#define SH_EVENT_SENSOR_FAULT         0x0FU
#define SH_EVENT_COMM_FAULT           0x10U
#define SH_EVENT_BUTTON_RELEASE       0x11U
#define SH_EVENT_BUTTON_LONG_PRESS    0x12U

// ============================================================
// TRIGGER - Ausloeser eines Ereignisses (Button, Master, Auto...)
// ============================================================

#define SH_TRIGGER_UNKNOWN            0x00U
#define SH_TRIGGER_MANUAL_BUTTON      0x01U
#define SH_TRIGGER_MASTER_CMD         0x02U
#define SH_TRIGGER_AUTO               0x03U
#define SH_TRIGGER_AUTO_OFF_TIMER     0x04U
#define SH_TRIGGER_CONFIG             0x05U

// ============================================================
// COVER-STATUS - Bewegungszustand eines Rolladens
// ============================================================

#define SH_COVER_STATE_STOPPED        0x00U
#define SH_COVER_STATE_MOVING_UP      0x01U
#define SH_COVER_STATE_MOVING_DOWN    0x02U

// ============================================================
// FEHLERCODES - Standardisierte Fehler fuer ACK/Event
// ============================================================

#define SH_ERROR_NONE                 0x00U
#define SH_ERROR_SENSOR_INIT          0x01U
#define SH_ERROR_SENSOR_READ          0x02U
#define SH_ERROR_ACK_TIMEOUT          0x03U
#define SH_ERROR_COVER_CALIB          0x04U

// ============================================================
// RELAY-COMFORT-FLAGS - Bitmaske fuer Auto-Light-Status
//   Wird in STATE-Reports als auto_flags-Feld gesendet
// ============================================================

#define SH_RELAY_COMFORT_FLAG_AUTO_REQUEST_ON           0x01U
#define SH_RELAY_COMFORT_FLAG_AUTO_RELAY_OWNED          0x02U
#define SH_RELAY_COMFORT_FLAG_BLOCKED_BY_SERVER         0x04U
#define SH_RELAY_COMFORT_FLAG_BLOCKED_BY_LUX            0x08U
#define SH_RELAY_COMFORT_FLAG_BLOCKED_BY_MISSING_LUX    0x10U
#define SH_RELAY_COMFORT_FLAG_PRESENCE_SOURCE_AVAILABLE 0x20U
#define SH_RELAY_COMFORT_FLAG_LIGHT_VALUE_AVAILABLE     0x40U
#define SH_RELAY_COMFORT_FLAG_LIGHT_GUARD_ENABLED       0x80U

namespace SmartHome {

/*
 * Kurzbeschreibung: ESP-NOW-Nachrichtenkopf (10 Bytes, packed).
 *
 * Wird JEDER ESP-NOW-Nachricht vorangestellt.
 *
 * Hinweis: magic, proto_ver und payload_len werden von isValidHeader() geprueft.
 *       crc16 wird durch finalizePacketCrc() gesetzt und von hasValidPacketCrc() validiert.
 */
typedef struct __attribute__((packed)) {
    uint8_t  magic;       // Magic-Byte (0xA5) zur Protokollerkennung
    uint8_t  proto_ver;   // Protokollversion (aktuell 1)
    uint8_t  msg_type;    // Nachrichtentyp (SH_MSG_HELLO, SH_MSG_STATE, ...)
    uint8_t  seq;         // Sequenznummer (wrappt bei 255)
    uint8_t  flags;       // Steuerbits (SH_FLAG_ACK_REQUEST, SH_FLAG_RETRANSMIT, ...)
    uint8_t  _reserved;   // Reserviert fuer zukuenftige Nutzung (immer 0)
    uint16_t payload_len; // Laenge des Payloads in Bytes (max 240)
    uint16_t crc16;       // CRC16-Checksumme ueber Header+Payload
} MsgHeader;

static_assert(sizeof(MsgHeader) == SH_HEADER_SIZE,
    "MsgHeader muss exakt SH_HEADER_SIZE Bytes groß sein");

/*
 * Kurzbeschreibung: HELLO-Payload: Node stellt sich beim Master vor (79 Bytes).
 *
 * Wird beim Boot und periodisch (bis HELLO_ACK empfangen) gesendet.
 * Traegt den kompletten oberen Geraetevertrag.
 *
 * Hinweis: device_class, caps_hi/lo, power_type, control_mode, config_profile,
 *       reporting_mode werden im Master fuer die Node-Registry verwendet.
 */
typedef struct __attribute__((packed)) {
    char     device_id[SH_DEVICE_ID_LEN];     // Eindeutige Geraete-ID (z.B. "NET-ERL-001")
    char     device_name[SH_DEVICE_NAME_LEN]; // Anzeigename (z.B. "Hall-Modul")
    uint8_t  device_class;                    // Geraeteklasse (SH_CLASS_NET_ERL, ...)
    uint8_t  caps_hi;                         // Faehigkeiten Bits 15..8
    uint8_t  caps_lo;                         // Faehigkeiten Bits 7..0
    uint8_t  power_type;                      // Stromversorgung (SH_POWER_MAINS/BATTERY)
    uint16_t fw_version;                      // Firmware-Version
    uint32_t boot_counter;                    // Boot-Zaehler (inkrementiert bei Neustart)
    uint8_t  meta_schema_version;             // Meta-Schema-Version
    uint8_t  control_mode;                    // Steuerungsmodus
    uint8_t  config_profile;                  // Konfigurationsprofil
    uint8_t  reporting_mode;                  // Report-Modus
    char     sensor_mask[SH_SENSOR_MASK_LEN]; // Sensor-Maske (11 Zeichen, z.B. "THLPGAMXXX")
    char     input_mask[SH_INPUT_MASK_LEN];   // Input-Maske (6 Zeichen, z.B. "BXXXX")
} HelloPayload;

static_assert(sizeof(HelloPayload) == 79,
    "HelloPayload muss 79 Bytes groß sein");

/*
 * Kurzbeschreibung: HELLO-ACK-Payload: Master bestaetigt Registrierung (4 Bytes).
 *
 * Wird vom Master nach erfolgreicher HELLO-Verarbeitung gesendet.
 *
 * Hinweis: channel enthaelt den ESP-NOW-Kanal, auf dem der Node antworten soll.
 */
typedef struct __attribute__((packed)) {
    uint8_t channel;    // ESP-NOW-Kanal fuer weitere Kommunikation
    uint8_t ack_status; // ACK-Status (SH_ACK_OK, SH_ACK_REJECTED, ...)
    uint8_t _pad[2];    // Padding auf 4-Byte-Grenze
} HelloAckPayload;

static_assert(sizeof(HelloAckPayload) == 4,
    "HelloAckPayload muss 4 Bytes groß sein");

/*
 * Kurzbeschreibung: HEARTBEAT-Payload: Lebenszeichen mit Betriebszeit (20 Bytes).
 *
 * Wird periodisch vom Node gesendet, solange keine STATE-Reports aktiv sind.
 */
typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN]; // Geraete-ID des Senders
    uint32_t uptime_s;                   // Betriebszeit in Sekunden seit letztem Boot
} HeartbeatPayload;

static_assert(sizeof(HeartbeatPayload) == 20,
    "HeartbeatPayload muss 20 Bytes groß sein");

/*
 * Kurzbeschreibung: CMD-Payload: Kommando vom Master an einen Node (4 Bytes).
 *
 * Der Master fordert eine Aktion an (Relais schalten, Cover fahren, Reboot, ...).
 * param1 und param2 werden je nach cmd_type unterschiedlich interpretiert.
 *
 * Hinweis: Bei SH_CMD_RELAY: param1=Relais-Index, param2=Zielzustand (0/1).
 *       Bei SH_CMD_COVER: param1=Cover-Unterkommando (SH_COVER_CMD_*), param2=Zielposition.
 */
typedef struct __attribute__((packed)) {
    uint8_t cmd_type; // Kommando-Typ (SH_CMD_RELAY, SH_CMD_COVER, SH_CMD_STATE_REQUEST, ...)
    uint8_t param1;   // Parameter 1 (z.B. Relay-Index, Cover-Unterkommando)
    uint8_t param2;   // Parameter 2 (z.B. Zielzustand, Zielposition)
    uint8_t _pad;     // Padding
} CmdPayload;

static_assert(sizeof(CmdPayload) == 4,
    "CmdPayload muss 4 Bytes groß sein");

/*
 * Kurzbeschreibung: CFG-Payload: Konfigurationsparameter setzen (4 Bytes).
 *
 * Der Master setzt einen einzelnen Parameter auf dem Node (via SH_MSG_CFG).
 * param_id identifiziert den Parameter, value enthaelt den neuen Wert.
 */
typedef struct __attribute__((packed)) {
    uint8_t  param_id; // Parameter-ID (SH_CFG_DEVICE_NAME, SH_CFG_RELAY_MODE, ...)
    uint8_t  _pad;     // Padding
    uint16_t value;    // Neuer Wert
} CfgPayload;

static_assert(sizeof(CfgPayload) == 4,
    "CfgPayload muss 4 Bytes groß sein");

/*
 * Kurzbeschreibung: ACK-Payload: Bestaetigung oder Ablehnung einer Nachricht (4 Bytes).
 *
 * Wird vom Empfaenger gesendet, wenn SH_FLAG_ACK_REQUEST im Header gesetzt ist.
 */
typedef struct __attribute__((packed)) {
    uint8_t ack_seq;      // Sequenznummer der bestaetigten Nachricht
    uint8_t ack_msg_type; // msg_type der bestaetigten Nachricht
    uint8_t status;       // ACK-Status (SH_ACK_OK, SH_ACK_ERROR, SH_ACK_REJECTED, ...)
    uint8_t _pad;         // Padding
} AckPayload;

static_assert(sizeof(AckPayload) == 4,
    "AckPayload muss 4 Bytes groß sein");

/*
 * Kurzbeschreibung: Basis-STATE-Payload: Minimaler Zustand - einfaches Relais (20 Bytes).
 *
 * Genutzt von: SH_CLASS_NET_ERL (einfaches Relais ohne Sensorik, z.B. net_erl_basic).
 */
typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN]; // Geraete-ID
    uint8_t  relay_1;                    // Relais-1-Zustand (0=aus, 1=ein)
    uint8_t  fault;                      // Fehlerstatus (SH_ERROR_NONE, ...)
    uint8_t  _pad[2];                    // Padding
} StateReportPayload;

static_assert(sizeof(StateReportPayload) == 20,
    "StateReportPayload muss 20 Bytes groß sein");

/*
 * Kurzbeschreibung: Basis-STATE + Konfigurations-Rueckmeldung (22 Bytes).
 *
 * Genutzt von: SH_CLASS_NET_ERL als Antwort auf SH_MSG_CFG-Abfrage.
 * Enthaelt zusaetzlich report_interval_s und auto_on_lux_threshold.
 */
typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN]; // Geraete-ID
    uint8_t  relay_1;                    // Relais-1-Zustand (0=aus, 1=ein)
    uint8_t  fault;                      // Fehlerstatus (SH_ERROR_NONE, ...)
    uint16_t report_interval_s;          // Aktuelles Report-Intervall (Sekunden)
    uint16_t auto_on_lux_threshold;      // Aktuelle Einschaltschwelle (Lux)
} StateConfigReportPayload;

static_assert(sizeof(StateConfigReportPayload) == 22,
    "StateConfigReportPayload muss 22 Bytes gross sein");

/*
 * Kurzbeschreibung: Relay-Comfort-STATE: Relais + Umweltsensorik (27 Bytes).
 *
 * Genutzt von: net_erl_hall_module (mit PIR + Lux).
 * Erweitert den Basis-State um Temperatur, Luftfeuchte, Lux, Bewegung und Auto-Flags.
 */
typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN]; // Geraete-ID
    uint8_t  relay_1;                    // Relais-1-Zustand (0=aus, 1=ein)
    int16_t  temp_01c;                   // Temperatur (1/100 °C)
    uint16_t hum_01pct;                  // Luftfeuchte (1/100 %)
    uint16_t lux;                        // Beleuchtungsstaerke (Lux)
    uint8_t  motion;                     // Bewegungserkennung (0=keine, 1=erkannt)
    uint8_t  auto_flags;                 // Automatik-Status (SH_RELAY_COMFORT_FLAG_*)
    uint8_t  fault;                      // Fehlerstatus (SH_ERROR_NONE, ...)
    uint8_t  _pad;                       // Padding
} RelayComfortStateReportPayload;

static_assert(sizeof(RelayComfortStateReportPayload) == 27,
    "RelayComfortStateReportPayload muss 27 Bytes gross sein");

/*
 * Kurzbeschreibung: Relay-Comfort-STATE + Konfigurations-Rueckmeldung (31 Bytes).
 *
 * Genutzt von: net_erl_hall_module als CFG-Abfrage-Antwort.
 */
typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN]; // Geraete-ID
    uint8_t  relay_1;                    // Relais-1-Zustand (0=aus, 1=ein)
    int16_t  temp_01c;                   // Temperatur (1/100 °C)
    uint16_t hum_01pct;                  // Luftfeuchte (1/100 %)
    uint16_t lux;                        // Beleuchtungsstaerke (Lux)
    uint8_t  motion;                     // Bewegungserkennung (0=keine, 1=erkannt)
    uint8_t  auto_flags;                 // Automatik-Status (SH_RELAY_COMFORT_FLAG_*)
    uint8_t  fault;                      // Fehlerstatus (SH_ERROR_NONE, ...)
    uint8_t  _pad;                       // Padding
    uint16_t report_interval_s;          // Aktuelles Report-Intervall (Sekunden)
    uint16_t auto_on_lux_threshold;      // Aktuelle Einschaltschwelle (Lux)
} RelayComfortConfigStateReportPayload;

static_assert(sizeof(RelayComfortConfigStateReportPayload) == 31,
    "RelayComfortConfigStateReportPayload muss 31 Bytes gross sein");

/*
 * Kurzbeschreibung: Extended-Relay-Comfort-STATE: Vollausbau mit BME680-Umweltdaten (37 Bytes).
 *
 * Genutzt von: net_erl_hall_module_led_ring (BME680 ohne Gas-OHM, mit ENS160 fuer AQI/tVOC/eCO2).
 * Erweitert RelayComfort um pressure_pa, aqi, tvoc_ppb, eco2_ppm.
 */
typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN]; // Geraete-ID
    uint8_t  relay_1;                    // Relais-1-Zustand (0=aus, 1=ein)
    int16_t  temp_01c;                   // Temperatur (1/100 °C)
    uint16_t hum_01pct;                  // Luftfeuchte (1/100 %)
    uint16_t lux;                        // Beleuchtungsstaerke (Lux)
    uint32_t pressure_pa;                // Luftdruck (Pascal)
    uint16_t aqi;                        // Luftqualitaetsindex (AQI)
    uint16_t tvoc_ppb;                   // Fluechtige organische Verbindungen (ppb)
    uint16_t eco2_ppm;                   // CO2-Aequivalent (ppm)
    uint8_t  motion;                     // Bewegungserkennung (0=keine, 1=erkannt)
    uint8_t  auto_flags;                 // Automatik-Status (SH_RELAY_COMFORT_FLAG_*)
    uint8_t  fault;                      // Fehlerstatus (SH_ERROR_NONE, ...)
    uint8_t  _pad;                       // Padding
} ExtendedRelayComfortStateReportPayload;

static_assert(sizeof(ExtendedRelayComfortStateReportPayload) == 37,
    "ExtendedRelayComfortStateReportPayload muss 37 Bytes gross sein");

/*
 * Kurzbeschreibung: Extended-Relay-Comfort-STATE + Konfigurations-Rueckmeldung (41 Bytes).
 *
 * Genutzt von: net_erl_hall_module_led_ring als CFG-Abfrage-Antwort (BME680 ohne Gas-OHM).
 */
typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN]; // Geraete-ID
    uint8_t  relay_1;                    // Relais-1-Zustand (0=aus, 1=ein)
    int16_t  temp_01c;                   // Temperatur (1/100 °C)
    uint16_t hum_01pct;                  // Luftfeuchte (1/100 %)
    uint16_t lux;                        // Beleuchtungsstaerke (Lux)
    uint32_t pressure_pa;                // Luftdruck (Pascal)
    uint16_t aqi;                        // Luftqualitaetsindex (AQI)
    uint16_t tvoc_ppb;                   // Fluechtige organische Verbindungen (ppb)
    uint16_t eco2_ppm;                   // CO2-Aequivalent (ppm)
    uint8_t  motion;                     // Bewegungserkennung (0=keine, 1=erkannt)
    uint8_t  auto_flags;                 // Automatik-Status (SH_RELAY_COMFORT_FLAG_*)
    uint8_t  fault;                      // Fehlerstatus (SH_ERROR_NONE, ...)
    uint8_t  _pad;                       // Padding
    uint16_t report_interval_s;          // Aktuelles Report-Intervall (Sekunden)
    uint16_t auto_on_lux_threshold;      // Aktuelle Einschaltschwelle (Lux)
} ExtendedRelayComfortConfigStateReportPayload;

static_assert(sizeof(ExtendedRelayComfortConfigStateReportPayload) == 41,
    "ExtendedRelayComfortConfigStateReportPayload muss 41 Bytes gross sein");

/*
 * Kurzbeschreibung: Extended-Relay-Gas-STATE: Vollausbau mit BME680-Gaswiderstand (41 Bytes).
 *
 * Genutzt von: net_erl_hall_module_led_ring (BME680 + ENS160 mit Gas-OHM).
 * Erweitert ExtendedRelayComfort um gas_ohm (BME680-Gaswiderstand in Ohm).
 */
typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN]; // Geraete-ID
    uint8_t  relay_1;                    // Relais-1-Zustand (0=aus, 1=ein)
    int16_t  temp_01c;                   // Temperatur (1/100 °C)
    uint16_t hum_01pct;                  // Luftfeuchte (1/100 %)
    uint16_t lux;                        // Beleuchtungsstaerke (Lux)
    uint32_t pressure_pa;                // Luftdruck (Pascal)
    uint32_t gas_ohm;                    // BME680-Gaswiderstand (Ohm)
    uint16_t aqi;                        // Luftqualitaetsindex (AQI)
    uint16_t tvoc_ppb;                   // Fluechtige organische Verbindungen (ppb)
    uint16_t eco2_ppm;                   // CO2-Aequivalent (ppm)
    uint8_t  motion;                     // Bewegungserkennung (0=keine, 1=erkannt)
    uint8_t  auto_flags;                 // Automatik-Status (SH_RELAY_COMFORT_FLAG_*)
    uint8_t  fault;                      // Fehlerstatus (SH_ERROR_NONE, ...)
    uint8_t  _pad;                       // Padding
} ExtendedRelayComfortGasStateReportPayload;

static_assert(sizeof(ExtendedRelayComfortGasStateReportPayload) == 41,
    "ExtendedRelayComfortGasStateReportPayload muss 41 Bytes gross sein");

/*
 * Kurzbeschreibung: Extended-Relay-Gas-STATE + Konfigurations-Rueckmeldung (45 Bytes).
 *
 * Genutzt von: net_erl_hall_module_led_ring (BME680 + ENS160 mit Gas-OHM) als CFG-Abfrage-Antwort.
 */
typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN]; // Geraete-ID
    uint8_t  relay_1;                    // Relais-1-Zustand (0=aus, 1=ein)
    int16_t  temp_01c;                   // Temperatur (1/100 °C)
    uint16_t hum_01pct;                  // Luftfeuchte (1/100 %)
    uint16_t lux;                        // Beleuchtungsstaerke (Lux)
    uint32_t pressure_pa;                // Luftdruck (Pascal)
    uint32_t gas_ohm;                    // BME680-Gaswiderstand (Ohm)
    uint16_t aqi;                        // Luftqualitaetsindex (AQI)
    uint16_t tvoc_ppb;                   // Fluechtige organische Verbindungen (ppb)
    uint16_t eco2_ppm;                   // CO2-Aequivalent (ppm)
    uint8_t  motion;                     // Bewegungserkennung (0=keine, 1=erkannt)
    uint8_t  auto_flags;                 // Automatik-Status (SH_RELAY_COMFORT_FLAG_*)
    uint8_t  fault;                      // Fehlerstatus (SH_ERROR_NONE, ...)
    uint8_t  _pad;                       // Padding
    uint16_t report_interval_s;          // Aktuelles Report-Intervall (Sekunden)
    uint16_t auto_on_lux_threshold;      // Aktuelle Einschaltschwelle (Lux)
} ExtendedRelayComfortGasConfigStateReportPayload;

static_assert(sizeof(ExtendedRelayComfortGasConfigStateReportPayload) == 45,
    "ExtendedRelayComfortGasConfigStateReportPayload muss 45 Bytes gross sein");

/*
 * Kurzbeschreibung: ZRL-STATE: Cover/Rollladen-Zustand (23 Bytes).
 *
 * Genutzt von: SH_CLASS_NET_ZRL (net_zrl_shutter_module).
 * Enthaelt Relais-Zustaende und Cover-Position/Status.
 */
typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN]; // Geraete-ID
    uint8_t  relay_1;                    // Relais-1-Zustand (0=aus, 1=ein | Cover auf)
    uint8_t  relay_2;                    // Relais-2-Zustand (0=aus, 1=ein | Cover ab)
    uint8_t  cover_mode;                 // Aktuelles Cover-Kommando (SH_COVER_CMD_*)
    uint8_t  cover_state;                // Bewegungszustand (SH_COVER_STATE_*)
    uint8_t  cover_position;             // Aktuelle Position (0=geschlossen, 100=offen)
    uint8_t  cover_calibrated;           // Kalibrierungsstatus (0=nicht kalibriert)
    uint8_t  fault;                      // Fehlerstatus (SH_ERROR_NONE, ...)
} ZrlStateReportPayload;

static_assert(sizeof(ZrlStateReportPayload) == 23,
    "ZrlStateReportPayload muss 23 Bytes gross sein");

/*
 * Kurzbeschreibung: ZRL-STATE + Konfigurations-Rueckmeldung (25 Bytes).
 *
 * Genutzt von: SH_CLASS_NET_ZRL (net_zrl_shutter_module) als CFG-Abfrage-Antwort.
 */
typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN]; // Geraete-ID
    uint8_t  relay_1;                    // Relais-1-Zustand (0=aus, 1=ein | Cover auf)
    uint8_t  relay_2;                    // Relais-2-Zustand (0=aus, 1=ein | Cover ab)
    uint8_t  cover_mode;                 // Aktuelles Cover-Kommando (SH_COVER_CMD_*)
    uint8_t  cover_state;                // Bewegungszustand (SH_COVER_STATE_*)
    uint8_t  cover_position;             // Aktuelle Position (0=geschlossen, 100=offen)
    uint8_t  cover_calibrated;           // Kalibrierungsstatus (0=nicht kalibriert)
    uint8_t  fault;                      // Fehlerstatus (SH_ERROR_NONE, ...)
    uint16_t report_interval_s;          // Aktuelles Report-Intervall (Sekunden)
} ZrlConfigStateReportPayload;

static_assert(sizeof(ZrlConfigStateReportPayload) == 25,
    "ZrlConfigStateReportPayload muss 25 Bytes gross sein");

/*
 * Kurzbeschreibung: Sensor-STATE: Reiner Sensor-Node (24 Bytes).
 *
 * Genutzt von: SH_CLASS_NET_SEN (net_sen_*).
 * Misst Temperatur, Luftfeuchte, Lux, Bewegung und Fehlerstatus.
 */
typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN]; // Geraete-ID
    int16_t  temp_01c;                   // Temperatur (1/100 °C)
    uint16_t hum_01pct;                  // Luftfeuchte (1/100 %)
    uint16_t lux;                        // Beleuchtungsstaerke (Lux)
    uint8_t  motion;                     // Bewegungserkennung (0=keine, 1=erkannt)
    uint8_t  fault;                      // Fehlerstatus (SH_ERROR_NONE, ...)
} SensorStateReportPayload;

static_assert(sizeof(SensorStateReportPayload) == 24,
    "SensorStateReportPayload muss 24 Bytes groß sein");

/*
 * Kurzbeschreibung: Sensor-STATE + Konfigurations-Rueckmeldung (26 Bytes).
 *
 * Genutzt von: SH_CLASS_NET_SEN (net_sen_*) als CFG-Abfrage-Antwort.
 */
typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN]; // Geraete-ID
    int16_t  temp_01c;                   // Temperatur (1/100 °C)
    uint16_t hum_01pct;                  // Luftfeuchte (1/100 %)
    uint16_t lux;                        // Beleuchtungsstaerke (Lux)
    uint8_t  motion;                     // Bewegungserkennung (0=keine, 1=erkannt)
    uint8_t  fault;                      // Fehlerstatus (SH_ERROR_NONE, ...)
    uint16_t report_interval_s;          // Aktuelles Report-Intervall (Sekunden)
} SensorConfigStateReportPayload;

static_assert(sizeof(SensorConfigStateReportPayload) == 26,
    "SensorConfigStateReportPayload muss 26 Bytes gross sein");

/*
 * Kurzbeschreibung: Extended-Sensor-STATE: Sensor mit BME680-Umweltdaten (34 Bytes).
 *
 * Genutzt von: net_sen_* mit BME680 (ohne Gas-OHM, mit ENS160 fuer AQI/tVOC/eCO2).
 * Erweitert SensorState um pressure_pa, aqi, tvoc_ppb, eco2_ppm.
 */
typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN]; // Geraete-ID
    int16_t  temp_01c;                   // Temperatur (1/100 °C)
    uint16_t hum_01pct;                  // Luftfeuchte (1/100 %)
    uint16_t lux;                        // Beleuchtungsstaerke (Lux)
    uint32_t pressure_pa;                // Luftdruck (Pascal)
    uint16_t aqi;                        // Luftqualitaetsindex (AQI)
    uint16_t tvoc_ppb;                   // Fluechtige organische Verbindungen (ppb)
    uint16_t eco2_ppm;                   // CO2-Aequivalent (ppm)
    uint8_t  motion;                     // Bewegungserkennung (0=keine, 1=erkannt)
    uint8_t  fault;                      // Fehlerstatus (SH_ERROR_NONE, ...)
} ExtendedSensorStateReportPayload;

static_assert(sizeof(ExtendedSensorStateReportPayload) == 34,
    "ExtendedSensorStateReportPayload muss 34 Bytes gross sein");

/*
 * Kurzbeschreibung: Extended-Sensor-STATE + Konfigurations-Rueckmeldung (36 Bytes).
 *
 * Genutzt von: net_sen_* mit BME680 (ohne Gas-OHM) als CFG-Abfrage-Antwort.
 */
typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN]; // Geraete-ID
    int16_t  temp_01c;                   // Temperatur (1/100 °C)
    uint16_t hum_01pct;                  // Luftfeuchte (1/100 %)
    uint16_t lux;                        // Beleuchtungsstaerke (Lux)
    uint32_t pressure_pa;                // Luftdruck (Pascal)
    uint16_t aqi;                        // Luftqualitaetsindex (AQI)
    uint16_t tvoc_ppb;                   // Fluechtige organische Verbindungen (ppb)
    uint16_t eco2_ppm;                   // CO2-Aequivalent (ppm)
    uint8_t  motion;                     // Bewegungserkennung (0=keine, 1=erkannt)
    uint8_t  fault;                      // Fehlerstatus (SH_ERROR_NONE, ...)
    uint16_t report_interval_s;          // Aktuelles Report-Intervall (Sekunden)
} ExtendedSensorConfigStateReportPayload;

static_assert(sizeof(ExtendedSensorConfigStateReportPayload) == 36,
    "ExtendedSensorConfigStateReportPayload muss 36 Bytes gross sein");

/*
 * Kurzbeschreibung: Extended-Sensor-Gas-STATE: Sensor mit BME680-Gaswiderstand (38 Bytes).
 *
 * Genutzt von: net_sen_* mit BME680 + ENS160 mit Gas-OHM.
 * Erweitert ExtendedSensorState um gas_ohm (BME680-Gaswiderstand in Ohm).
 */
typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN]; // Geraete-ID
    int16_t  temp_01c;                   // Temperatur (1/100 °C)
    uint16_t hum_01pct;                  // Luftfeuchte (1/100 %)
    uint16_t lux;                        // Beleuchtungsstaerke (Lux)
    uint32_t pressure_pa;                // Luftdruck (Pascal)
    uint32_t gas_ohm;                    // BME680-Gaswiderstand (Ohm)
    uint16_t aqi;                        // Luftqualitaetsindex (AQI)
    uint16_t tvoc_ppb;                   // Fluechtige organische Verbindungen (ppb)
    uint16_t eco2_ppm;                   // CO2-Aequivalent (ppm)
    uint8_t  motion;                     // Bewegungserkennung (0=keine, 1=erkannt)
    uint8_t  fault;                      // Fehlerstatus (SH_ERROR_NONE, ...)
} ExtendedSensorGasStateReportPayload;

static_assert(sizeof(ExtendedSensorGasStateReportPayload) == 38,
    "ExtendedSensorGasStateReportPayload muss 38 Bytes gross sein");

/*
 * Kurzbeschreibung: Extended-Sensor-Gas-STATE + Konfigurations-Rueckmeldung (40 Bytes).
 *
 * Genutzt von: net_sen_* mit BME680 + ENS160 (Gas-OHM) als CFG-Abfrage-Antwort.
 */
typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN]; // Geraete-ID
    int16_t  temp_01c;                   // Temperatur (1/100 °C)
    uint16_t hum_01pct;                  // Luftfeuchte (1/100 %)
    uint16_t lux;                        // Beleuchtungsstaerke (Lux)
    uint32_t pressure_pa;                // Luftdruck (Pascal)
    uint32_t gas_ohm;                    // BME680-Gaswiderstand (Ohm)
    uint16_t aqi;                        // Luftqualitaetsindex (AQI)
    uint16_t tvoc_ppb;                   // Fluechtige organische Verbindungen (ppb)
    uint16_t eco2_ppm;                   // CO2-Aequivalent (ppm)
    uint8_t  motion;                     // Bewegungserkennung (0=keine, 1=erkannt)
    uint8_t  fault;                      // Fehlerstatus (SH_ERROR_NONE, ...)
    uint16_t report_interval_s;          // Aktuelles Report-Intervall (Sekunden)
} ExtendedSensorGasConfigStateReportPayload;

static_assert(sizeof(ExtendedSensorGasConfigStateReportPayload) == 40,
    "ExtendedSensorGasConfigStateReportPayload muss 40 Bytes gross sein");

/*
 * Kurzbeschreibung: Battery-STATE: Batterie-Sensor (24 Bytes).
 *
 * Genutzt von: SH_CLASS_BAT_SEN (bat_sen_*).
 * Misst Batterieladung, Spannung, Fensterstatus, Regen und Taster-Flags.
 */
typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN]; // Geraete-ID
    uint8_t  battery_pct;               // Batterieladung (0-100 %)
    uint16_t battery_mv;                // Batteriespannung (mV)
    uint8_t  window_open;               // Fensterstatus (0=geschlossen, 1=offen)
    uint16_t rain_raw;                  // Regen-Sensor-Rohwert
    uint8_t  button_flags;              // Taster-Status-Flags
    uint8_t  fault;                     // Fehlerstatus (SH_ERROR_NONE, ...)
} BatteryStateReportPayload;

static_assert(sizeof(BatteryStateReportPayload) == 24,
    "BatteryStateReportPayload muss 24 Bytes groß sein");

/*
 * Kurzbeschreibung: Battery-STATE + Konfigurations-Rueckmeldung (26 Bytes).
 *
 * Genutzt von: SH_CLASS_BAT_SEN (bat_sen_*) als CFG-Abfrage-Antwort.
 */
typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN]; // Geraete-ID
    uint8_t  battery_pct;               // Batterieladung (0-100 %)
    uint16_t battery_mv;                // Batteriespannung (mV)
    uint8_t  window_open;               // Fensterstatus (0=geschlossen, 1=offen)
    uint16_t rain_raw;                  // Regen-Sensor-Rohwert
    uint8_t  button_flags;              // Taster-Status-Flags
    uint8_t  fault;                     // Fehlerstatus (SH_ERROR_NONE, ...)
    uint16_t report_interval_s;         // Aktuelles Report-Intervall (Sekunden)
} BatteryConfigStateReportPayload;

static_assert(sizeof(BatteryConfigStateReportPayload) == 26,
    "BatteryConfigStateReportPayload muss 26 Bytes gross sein");

/*
 * Kurzbeschreibung: EVENT-Payload: Asynchrone Ereignismeldung (22 Bytes).
 *
 * Genutzt von allen Device-Classes fuer Ereignisse wie Button-Druck,
 * Bewegungserkennung, Automatik-Schaltung, Cover-Bewegung, Fehler.
 *
 * Hinweis: event_type und trigger bestimmen die Semantik von param1 und param2:
 *       - SH_EVENT_RELAY_CHANGED: param1=Relay-Index, param2=Zustand
 *       - SH_EVENT_BUTTON_PRESS:  param1=Button-Index, param2=Button-Typ
 *       - SH_EVENT_COVER_UP/DOWN: param1=Zielposition, param2=Trigger
 */
typedef struct __attribute__((packed)) {
    char     node_id[SH_DEVICE_ID_LEN]; // Geraete-ID
    uint8_t  event_type;                // Ereignistyp (SH_EVENT_BUTTON_PRESS, ...)
    uint8_t  trigger;                   // Ausloeser (SH_TRIGGER_MANUAL_BUTTON, ...)
    uint8_t  param1;                    // Parameter 1 (z.B. Relay-Index, Button-Index)
    uint16_t param2;                    // Parameter 2 (z.B. Relay-Status, Zielposition)
    uint8_t  _pad;                      // Padding
} EventReportPayload;

static_assert(sizeof(EventReportPayload) == 22,
    "EventReportPayload muss 22 Bytes groß sein");


/*
 * Kurzbeschreibung: Befuellt einen MsgHeader mit den Grundwerten.
 *
 * Setzt Magic, Protokollversion, Nachrichtentyp, Sequenznummer, Flags
 * und Payload-Laenge. _reserved wird auf 0, crc16 auf 0 initialisiert.
 *
 * Eingabe/Ausgabe h: Referenz auf den zu befuellenden Header
 * Eingabewert msg_type: Nachrichtentyp (SH_MSG_HELLO, ...)
 * Eingabewert seq: Sequenznummer (wrappt bei 255)
 * Eingabewert flags: Steuerbits (SH_FLAG_ACK_REQUEST, ...)
 * Eingabewert payload_len: Laenge des nachfolgenden Payloads in Bytes
 */
static inline void fillHeader(
    MsgHeader& h,
    uint8_t msg_type,
    uint8_t seq,
    uint8_t flags,
    uint16_t payload_len)
{
    h.magic       = SH_PROTO_MAGIC;
    h.proto_ver   = SH_PROTO_VERSION;
    h.msg_type    = msg_type;
    h.seq         = seq;
    h.flags       = flags;
    h._reserved   = 0;
    h.payload_len = payload_len;
    h.crc16       = 0;
}

/*
 * Kurzbeschreibung: Validiert die Grundfelder eines MsgHeader.
 *
 * Prueft Magic-Byte, Protokollversion und Payload-Laenge.
 * Die CRC wird hier NICHT geprueft - dafuer hasValidPacketCrc() verwenden.
 *
 * Eingabewert h: Referenz auf den zu pruefenden Header
 * Ausgabewert: true, wenn Magic, Version und Payload-Laenge gueltig sind
 */
static inline bool isValidHeader(const MsgHeader& h) {
    if (h.magic != SH_PROTO_MAGIC) return false;
    if (h.proto_ver != SH_PROTO_VERSION) return false;
    if (h.payload_len > SH_MAX_PAYLOAD_BYTES) return false;
    return true;
}

/*
 * Kurzbeschreibung: Kopiert eine Device-ID sicher in einen Zielpuffer.
 *
 * Begrenzt auf SH_DEVICE_ID_LEN-1 Zeichen, stellt Nullterminierung sicher
 * und verwirft ungueltige Zeichen. Erlaubt sind nur Grossbuchstaben,
 * Ziffern und das Minuszeichen ('-'). Jedes andere Zeichen beendet die Kopie.
 *
 * Eingabewert src: Quell-String (darf nullptr sein, Funktion tut dann nichts)
 * Eingabe/Ausgabe dst: Ziel-Puffer (muss mindestens SH_DEVICE_ID_LEN Bytes haben)
 */
static inline void safeCopyDeviceId(const char* src, char* dst) {
    if (!src || !dst) return;
    strncpy(dst, src, SH_DEVICE_ID_LEN - 1);
    dst[SH_DEVICE_ID_LEN - 1] = '\0';
    for (int i = 0; dst[i] != '\0'; i++) {
        const char c = dst[i];
        if (!isupper((unsigned char)c) &&
            !isdigit((unsigned char)c) &&
            c != '-') {
            dst[i] = '\0';
            break;
        }
    }
}

/*
 * Kurzbeschreibung: Kopiert eine Maske sicher mit Fallback-Fuellzeichen.
 *
 * Kopiert bis zu dst_len-1 Zeichen aus src nach dst. Jedes Zeichen wird
 * auf Gueltigkeit geprueft: erlaubt sind Grossbuchstaben, Ziffern, 'X' und '_'.
 * Ungueltige Zeichen werden durch fill_char ersetzt. Wenn src null oder
 * kuerzer als erforderlich ist, wird mit fill_char aufgefuellt.
 *
 * Eingabewert src: Quell-String (darf nullptr sein)
 * Eingabe/Ausgabe dst: Ziel-Puffer
 * Eingabewert dst_len: Groesse des Ziel-Puffers in Bytes
 * Eingabewert fill_char: Ersatzzeichen fuer ungueltige oder fehlende Zeichen
 */
static inline void safeCopyMask(const char* src, char* dst, size_t dst_len, char fill_char) {
    if (!dst || dst_len == 0U) return;
    const size_t max_chars = dst_len - 1U;
    size_t i = 0U;
    for (; i < max_chars; ++i) {
        char c = fill_char;
        if (src != nullptr && src[i] != '\0') {
            c = src[i];
        }
        if (!isupper((unsigned char)c) && !isdigit((unsigned char)c) && c != 'X' && c != '_') {
            c = fill_char;
        }
        dst[i] = c;
    }
    dst[max_chars] = '\0';
}

/*
 * Kurzbeschreibung: Prueft ob eine Device-ID dem erwarteten Format entspricht.
 *
 * Gueltiges Format:
 * - 1 bis SH_DEVICE_ID_LEN-1 Zeichen
 * - keine Schraegstriche, Leerzeichen oder Sonderzeichen, die MQTT-Topics
 *   oder JSON-Felder unnoetig erschweren
 *
 * Erlaubte Zeichen: Buchstaben (A-Z, a-z), Ziffern (0-9), Minus ('-')
 * und Unterstrich ('_'). Damit sind sowohl NET-ERL-001 als auch bat_sen_01
 * gueltige IDs. Die ID ist Identitaet, keine Typableitung.
 *
 * Eingabewert id: Zu pruefender String
 * Ausgabewert: true, wenn die ID dem Format entspricht
 */
static inline bool isValidDeviceId(const char* id) {
    if (!id) return false;
    const size_t len = strlen(id);
    if (len == 0 || len >= SH_DEVICE_ID_LEN) return false;
    bool hasAlnum = false;
    for (size_t i = 0; i < len; i++) {
        const char c = id[i];
        if (c == '-' || c == '_') continue;
        if (!isdigit((unsigned char)c) && !isalpha((unsigned char)c)) return false;
        hasAlnum = true;
    }
    return hasAlnum;
}

/*
 * Kurzbeschreibung: Prueft ob eine MAC-Adresse gueltig ist.
 *
 * Eine MAC-Adresse ist ungueltig, wenn sie nur aus Nullen (00:00:00:00:00:00)
 * oder nur aus Einsen (FF:FF:FF:FF:FF:FF) besteht.
 *
 * Eingabewert mac: Zeiger auf 6-Byte MAC-Array (darf nullptr sein)
 * Ausgabewert: true, wenn die MAC weder Null- noch Broadcast-Adresse ist
 */
static inline bool isValidMac(const uint8_t* mac) {
    if (!mac) return false;
    bool allZero = true;
    bool allFf   = true;
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0x00) allZero = false;
        if (mac[i] != 0xFF) allFf = false;
    }
    return !(allZero || allFf);
}

/*
 * Kurzbeschreibung: Wandelt eine MAC-Adresse in einen String um.
 *
 * Zielformat: "XX:XX:XX:XX:XX:XX" (hexadezimale Grossbuchstaben,
 * 17 Zeichen + Nullterminator = 18 Bytes Zielpuffer).
 *
 * Eingabewert mac: Zeiger auf 6-Byte MAC-Array (darf nullptr sein)
 * Eingabe/Ausgabe buffer: Ziel-Puffer (muss mindestens 18 Bytes gross sein, darf nullptr sein)
 */
static inline void macToString(const uint8_t* mac, char* buffer) {
    if (!mac || !buffer) return;
    snprintf(buffer, 18,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/*
 * Kurzbeschreibung: Berechnet CRC16-CCITT (Polynom 0x1021) ueber einen Datenpuffer.
 *
 * Implementierung mit initialem CRC-Wert 0xFFFF und byteweisem XOR
 * der oberen 8 Bit, gefolgt von 8 Bit-verschobenen XOR-Schritten
 * mit dem Generatorpolynom 0x1021.
 *
 * Eingabewert data: Zeiger auf den Datenpuffer
 * Eingabewert len: Anzahl Bytes im Puffer
 * Ausgabewert: 16-Bit CRC-Wert
 */
static inline uint16_t calcCrc16(const uint8_t* data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc ^= (uint16_t)(*data++) << 8;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

/*
 * Kurzbeschreibung: Berechnet den CRC16-Wert fuer ein komplettes Paket (Header + Payload).
 *
 * Erstellt eine temporaere Kopie des Headers mit crc16=0,
 * konkateniert Header und Payload in einem Puffer und berechnet
 * die CRC16 ueber die Gesamtlaenge.
 *
 * Eingabewert header: Referenz auf den MsgHeader (dessen crc16-Feld wird ignoriert)
 * Eingabewert payload: Zeiger auf den Payload (darf nullptr sein wenn header.payload_len == 0)
 * Ausgabewert: Berechneter CRC16-Wert ueber Header + Payload
 */
static inline uint16_t calcPacketCrc(
    const MsgHeader& header,
    const uint8_t* payload)
{
    MsgHeader temp = header;
    temp.crc16 = 0;

    uint8_t buffer[SH_ESPNOW_MAX_BYTES] = {0};
    memcpy(buffer, &temp, sizeof(MsgHeader));

    if (temp.payload_len > 0U && payload != nullptr) {
        memcpy(buffer + sizeof(MsgHeader), payload, temp.payload_len);
    }

    return calcCrc16(buffer, (uint16_t)(sizeof(MsgHeader) + temp.payload_len));
}

/*
 * Kurzbeschreibung: Berechnet und setzt den CRC16-Wert im Header.
 *
 * Convenience-Funktion: ruft calcPacketCrc() auf und schreibt das
 * Ergebnis ins crc16-Feld des Headers.
 *
 * Eingabe/Ausgabe: header  Referenz auf den Header (crc16-Feld wird ueberschrieben)
 * Eingabewert payload: Zeiger auf den Payload (darf nullptr sein)
 */
static inline void finalizePacketCrc(
    MsgHeader& header,
    const uint8_t* payload)
{
    header.crc16 = calcPacketCrc(header, payload);
}

/*
 * Kurzbeschreibung: Prueft ob ein empfangenes Paket eine gueltige CRC16 hat.
 *
 * Validiert zusaetzlich den Header (Magic, Version, Payload-Laenge
 * via isValidHeader()) und die Konsistenz von angegebener Payload-Laenge
 * und tatsaechlicher Puffergroesse.
 *
 * Eingabewert packet: Zeiger auf den Rohdaten-Puffer (Header + Payload)
 * Eingabewert len: Tatsaechliche Laenge des Puffers in Bytes
 * Ausgabewert: true, wenn Header gueltig, Laengen konsistent und CRC korrekt ist
 */
static inline bool hasValidPacketCrc(const uint8_t* packet, size_t len) {
    if (!packet || len < sizeof(MsgHeader)) return false;

    MsgHeader header;
    memcpy(&header, packet, sizeof(MsgHeader));

    if (!isValidHeader(header)) return false;
    if (len != (sizeof(MsgHeader) + header.payload_len)) return false;

    const uint8_t* payload = packet + sizeof(MsgHeader);
    return calcPacketCrc(header, payload) == header.crc16;
}

} // namespace SmartHome
