// =============================================================================
// BatSenRuntime.h – BAT-SEN Basistyp: Batterie-Sensor (ESP-NOW Runtime)
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/basetypes/bat_sen/BatSenRuntime.h
//
// Datei-Funktion:
//   ESP-NOW-Sensor-Implementierung fuer batteriebetriebene Geraete.
//   Header-only (wie NetSenRuntime). Nutzt Deep-Sleep fuer
//   Energiesparmodus, periodischen Timer-Wake und optionalen GPIO-Wake.
//   Bietet Custom-Hooks fuer konkrete Devices (Init, Poll, State,
//   Events, Wake-Konfiguration). Unterstuetzt Batterie-ADC-Messung
//   mit konfigurierbaren Profilen (CR2032, AA, AAA, LiIon).
//
// Protokoll-Nachrichten:
//   Senden:   HELLO, STATE (BatteryConfigStateReport), EVENT, ACK
//   Empfangen: HELLO_ACK, CMD (STATE_REQUEST), CFG (wake_interval, rx_window)
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
//
// Aenderungshistorie:
//   [2026-05-14] DevOpsOfChaos – Kommentierung (Deutsch, Doxygen-Stil)
// =============================================================================

#pragma once

#include <Arduino.h>
#include <ShNodeProvisioning.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <esp_task_wdt.h>
#include <stdarg.h>
#include <string.h>

#if __has_include(<esp_arduino_version.h>)
  #include <esp_arduino_version.h>
#endif

#ifndef ESP_ARDUINO_VERSION_MAJOR
  #define ESP_ARDUINO_VERSION_MAJOR 2
#endif

#include "BatSenProvisioning.h"
#include "DeviceConfig.h"
#include "PinConfig.h"
#include "../../../include/DebugConfig.h"
#include "../../../include/ProjectVersion.h"
#include "../../../lib/sh_protocol/src/DeviceTypes.h"
#include "../../../lib/sh_protocol/src/Protocol.h"

// =============================================================================
// FORWARD DECLARATIONS – nodeProvisioning (used before definition)
// =============================================================================

extern SmartHome::ShNodeProvisioning::NodeProvisioningController nodeProvisioning;

// =============================================================================
// KONSTANTEN – Debug, Version, RTC-Speicher, Broadcast, Custom-Hooks
// =============================================================================

constexpr bool DEBUG_LOKAL_AKTIV = DEVICE_DEBUG_AKTIV && DEBUG_AKTIV;
constexpr char DATEI_GERAET[] = "BAT-SEN";
constexpr char DATEI_VERSION[] = "0.1.0";

#ifndef BAT_SEN_WDT_TIMEOUT_S
#define BAT_SEN_WDT_TIMEOUT_S 15
#endif
const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
constexpr size_t SETUP_AP_SSID_BUFFER_SIZE = 32U;
static_assert(
    sizeof(DEVICE_ID) <= SETUP_AP_SSID_BUFFER_SIZE,
    "BAT_SEN_DEVICE_ID muss als Setup-SSID in den AP-SSID-Puffer passen.");

// Boot-Counter in RTC-Speicher (bleibt ueber Deep-Sleep hinweg erhalten)
RTC_DATA_ATTR uint32_t RTC_BOOT_COUNTER = 0U;

// Custom-Hook-Flags (koennen von Devices gesetzt werden)
#ifndef BAT_SEN_DEVICE_HAS_CUSTOM_HOOKS
#define BAT_SEN_DEVICE_HAS_CUSTOM_HOOKS 0
#endif

#ifndef BAT_SEN_GPIO_WAKE_LEVEL_HIGH
#define BAT_SEN_GPIO_WAKE_LEVEL_HIGH 1  // Default: Wake bei HIGH-Pegel
#endif

#ifndef BAT_SEN_DEVICE_HAS_DYNAMIC_WAKE_LEVEL
#define BAT_SEN_DEVICE_HAS_DYNAMIC_WAKE_LEVEL 0
#endif

// =============================================================================
// STRUKTUREN – GenericStateChannels, GenericEventData, NodeState
// =============================================================================

// GenericStateChannels – Neutrale Zustandskanaele (device-agnostisch)
struct GenericStateChannels {
    uint8_t channel_bool_1;  // Bool-Kanal (z.B. Fensterkontakt)
    uint16_t channel_u16_1;  // U16-Kanal (z.B. ADC-Rohwert)
    uint8_t channel_mask_1;  // Mask-Kanal (z.B. Tastenflags)
    bool fault;              // Fehlerstatus
};

// GenericEventData – Austehrendes Device-Event
struct GenericEventData {
    bool vorhanden;          // true = Event liegt an
    uint8_t event_type;      // Event-Typ (SH_EVENT_*)
    uint8_t trigger;         // Ausloeser (SH_TRIGGER_*)
    uint8_t param1;          // Parameter 1
    uint16_t param2;         // Parameter 2
};

// NodeState – Zentraler Geraetezustand (Batterie-spezifisch)
struct NodeState {
    bool provisioning_bereit;       // NodeProvisioning initialisiert
    bool setup_mode;                // Setup-Modus aktiv
    bool setup_ap_aktiv;            // Setup-AP laeuft
    bool stay_awake;                // true = Deep-Sleep per kurzem Tastendruck gesperrt
    bool stay_button_last_active;   // Letzter Tasterzustand fuer Short-Press
    bool stay_button_hold_consumed; // true = langer Druck wurde als Setup-Hold verbraucht
    bool restart_pending;           // Neustart angefordert
    bool master_bekannt;            // HELLO_ACK empfangen
    bool master_mac_gueltig;        // Master-MAC provisioniert
    bool state_report_offen;        // STATE muss gesendet werden
    bool event_report_offen;        // EVENT muss gesendet werden
    unsigned long boot_ms;          // Zeitstempel Boot (millis)
    unsigned long stay_button_pressed_at_ms; // Zeitstempel Setup-Taster gedrueckt
    unsigned long letztes_hello_ms; // Zeitstempel letztes HELLO
    unsigned long letzter_state_ms; // Zeitstempel letzter STATE
    unsigned long letzte_batterie_probe_ms; // Zeitstempel letzte ADC-Messung
    unsigned long schlaf_ab_ms;     // Frueheste Zeit zum Deep-Sleep
    unsigned long restart_requested_at_ms; // Zeitstempel Restart
    uint32_t wake_interval_s;       // Wake-Intervall (Sekunden)
    uint32_t rx_window_ms;          // RX-Fenster nach Wake (ms)
    uint8_t master_mac[6];          // Provisionierte Master-MAC
    uint8_t naechste_seq;           // Naechste ESP-NOW-Sequenz
    uint8_t battery_pct;            // Batterie in Prozent
    uint16_t battery_mv;            // Batteriespannung in mV
    bool battery_fault;             // Fehler bei Batteriemessung
    uint8_t wake_reason;            // Wake-Grund (0=unbekannt, 1=Timer, 2=GPIO)
    uint32_t boot_counter;          // Boot-Zaehler (RTC-persistent)
    GenericStateChannels kanaele;   // Device-Kanaele
    GenericEventData event;         // Ausstehendes Event
    char setup_ap_ssid[SETUP_AP_SSID_BUFFER_SIZE]; // Setup-AP-SSID
};

NodeState nodeStatus = {};

// =============================================================================
// CUSTOM-DEVICE-HOOKS – Werden von konkreten Devices ueberschrieben
// =============================================================================

#if BAT_SEN_DEVICE_HAS_CUSTOM_HOOKS
void device_init_io();
bool device_poll_inputs();
void device_build_state_channels(
    uint8_t* channelBool1,
    uint16_t* channelU16_1,
    uint8_t* channelMask1,
    bool* fault);
bool device_map_event(
    uint8_t* eventType,
    uint8_t* trigger,
    uint8_t* param1,
    uint16_t* param2);
uint64_t device_wake_candidates();
#if BAT_SEN_DEVICE_HAS_DYNAMIC_WAKE_LEVEL
bool device_wake_level_high();
#endif
#else
void device_init_io() {}
bool device_poll_inputs() { return false; }

void device_build_state_channels(
    uint8_t* channelBool1,
    uint16_t* channelU16_1,
    uint8_t* channelMask1,
    bool* fault)
{
    if (channelBool1 != nullptr) *channelBool1 = 0U;
    if (channelU16_1 != nullptr) *channelU16_1 = 0U;
    if (channelMask1 != nullptr) *channelMask1 = 0U;
    if (fault != nullptr) *fault = false;
}

bool device_map_event(
    uint8_t* eventType,
    uint8_t* trigger,
    uint8_t* param1,
    uint16_t* param2)
{
    if (eventType != nullptr) *eventType = 0U;
    if (trigger != nullptr) *trigger = SH_TRIGGER_UNKNOWN;
    if (param1 != nullptr) *param1 = 0U;
    if (param2 != nullptr) *param2 = 0U;
    return false;
}

uint64_t device_wake_candidates() { return 0ULL; }
#endif

bool deviceWakeLevelHigh() {
#if BAT_SEN_DEVICE_HAS_DYNAMIC_WAKE_LEVEL
    return device_wake_level_high();
#else
    return BAT_SEN_GPIO_WAKE_LEVEL_HIGH != 0;
#endif
}

// =============================================================================
// HILFSFUNKTIONEN – Logging, Strings, Delta, Broadcast, MAC, Wake-Reason
// =============================================================================

// logf – Formatiertes Logging (nur bei aktiviertem Debug)
void logf(const char* level, const char* format, ...) {
    if (!DEBUG_LOKAL_AKTIV) return;

    char message[224];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    Serial.print("[");
    Serial.print(level);
    Serial.print("] ");
    Serial.println(message);
}

// provisioningLog – Logging-Callback fuer Provisioning-Framework
void provisioningLog(const char* level, const char* message) {
    if (!DEBUG_LOKAL_AKTIV || level == nullptr || message == nullptr) return;

    Serial.print("[");
    Serial.print(level);
    Serial.print("] ");
    Serial.println(message);
}

// copyText – Sicheres Kopieren eines null-terminierten Strings
void copyText(char* target, size_t targetSize, const char* source) {
    if (!target || targetSize == 0U) return;
    if (!source) {
        target[0] = '\0';
        return;
    }
    strncpy(target, source, targetSize - 1U);
    target[targetSize - 1U] = '\0';
}

// deltaU16 – Absolute Differenz zwischen zwei uint16-Werten
uint16_t deltaU16(uint16_t a, uint16_t b) {
    return (a >= b) ? (a - b) : (b - a);
}

// deltaU8 – Absolute Differenz zwischen zwei uint8-Werten
uint8_t deltaU8(uint8_t a, uint8_t b) {
    // Cast nur fuer uint8_t noetig (Integer-Promotion)
    return (uint8_t)((a >= b) ? (a - b) : (b - a));
}

// istBroadcastMac – Prueft ob MAC die Broadcast-Adresse ist
bool istBroadcastMac(const uint8_t* mac) {
    return mac != nullptr && memcmp(mac, BROADCAST_MAC, sizeof(BROADCAST_MAC)) == 0;
}

// helloZielMac – Ziel-MAC fuer HELLO (Master oder Broadcast)
const uint8_t* helloZielMac() {
    return nodeStatus.master_mac_gueltig ? nodeStatus.master_mac : BROADCAST_MAC;
}

// istZeitErreicht – Prueft ob millis()-Zeit erreicht/ueberschritten ist (Wrap-sicher)
bool istZeitErreicht(unsigned long jetzt, unsigned long ziel) {
    return (long)(jetzt - ziel) >= 0;
}

// wakeReasonCode – Ermittelt den Wake-Grund des letzten Deep-Sleeps
//   Rueckgabe: 0=unbekannt/Reset, 1=Timer-Wake, 2=GPIO-Wake
uint8_t wakeReasonCode() {
    const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_TIMER) return 1U;
#if CONFIG_IDF_TARGET_ESP32C3
    if (cause == ESP_SLEEP_WAKEUP_GPIO) return 2U;
#else
    if (cause == ESP_SLEEP_WAKEUP_EXT1) return 2U;
#endif
    return 0U;
}

// =============================================================================
// WAKE-KONFIGURATION – GPIO-Wake-Pins validieren und setzen
// =============================================================================

// validiereWakeMask – Filtert ungueltige Wake-Pins aus einer Bitmaske
uint64_t validiereWakeMask(uint64_t kandidatMask) {
    if (kandidatMask == 0ULL) return 0ULL;

    uint64_t gueltigMask = 0ULL;
    for (uint8_t pin = 0U; pin < 64U; ++pin) {
        const uint64_t bit = (1ULL << pin);
        if ((kandidatMask & bit) == 0ULL) continue;

        // Prueft ob dieser Pin Deep-Sleep-Wake unterstuetzt
        if (esp_sleep_is_valid_wakeup_gpio((gpio_num_t)pin)) {
            gueltigMask |= bit;
        } else {
            logf("WARN", "GPIO %u ist kein gueltiger Deep-Sleep-Wake-Pin", pin);
        }
    }
    return gueltigMask;
}

// fuegeWakePinHinzu – Fuegt einen Pin zu den Wake-Masken (HIGH/LOW) hinzu
void fuegeWakePinHinzu(uint64_t* highMask, uint64_t* lowMask, int pin, bool wakeHigh) {
    if (highMask == nullptr || lowMask == nullptr) return;
    if (pin < 0 || pin >= 64) return;

    if (wakeHigh) {
        *highMask |= (1ULL << (uint8_t)pin);
    } else {
        *lowMask |= (1ULL << (uint8_t)pin);
    }
}

// =============================================================================
// BATTERIE – Prozentberechnung, ADC-Messung, Zustand aktualisieren
// =============================================================================

// berechneBatterieProzent – Wandelt Spannung in Prozent (linear zwischen empty/full)
uint8_t berechneBatterieProzent(uint16_t batteryMv) {
    if (batteryMv <= BATTERY_EMPTY_MV) return 0U;
    if (batteryMv >= BATTERY_FULL_MV) return 100U;

    const int32_t span = (int32_t)BATTERY_FULL_MV - (int32_t)BATTERY_EMPTY_MV;
    const int32_t pos = (int32_t)batteryMv - (int32_t)BATTERY_EMPTY_MV;
    const int32_t pct = (pos * 100) / span;
    if (pct <= 0) return 0U;
    if (pct >= 100) return 100U;
    return (uint8_t)pct;
}

// leseBatterie – Fuehrt ADC-Messung durch (gemittelt, skaliert)
//   Gibt Spannung in mV und Prozent zurueck. Nutzt Spannungsteiler-Korrektur.
bool leseBatterie(uint16_t* batteryMv, uint8_t* batteryPct) {
    if (batteryMv == nullptr || batteryPct == nullptr) return false;
    if (!BATTERY_ADC_AKTIV || PIN_BATTERY_ADC < 0) return false;

    // ADC-Samples mitteln (rauschaerm)
    uint32_t adcSumMv = 0UL;
    for (uint8_t i = 0U; i < BATTERY_ADC_SAMPLE_COUNT; ++i) {
        adcSumMv += (uint32_t)analogReadMilliVolts(PIN_BATTERY_ADC);
    }
    const uint32_t adcMv = (adcSumMv + ((uint32_t)BATTERY_ADC_SAMPLE_COUNT / 2UL)) /
                           (uint32_t)BATTERY_ADC_SAMPLE_COUNT;
    if (adcMv == 0UL) return false;

    // Spannungsteiler korrigieren: scaled = adc * num / den
    uint32_t scaledMv = ((adcMv * (uint32_t)BATTERY_DIVIDER_NUM) + ((uint32_t)BATTERY_DIVIDER_DEN / 2UL)) /
                        (uint32_t)BATTERY_DIVIDER_DEN;
    scaledMv = ((scaledMv * (uint32_t)BATTERY_CALIBRATION_NUM) + ((uint32_t)BATTERY_CALIBRATION_DEN / 2UL)) /
               (uint32_t)BATTERY_CALIBRATION_DEN;
    if (scaledMv > 65535UL) scaledMv = 65535UL;

    *batteryMv = (uint16_t)scaledMv;
    *batteryPct = berechneBatterieProzent(*batteryMv);
    return true;
}

// aktualisiereBatterie – Misst Batterie und aktualisiert nodeStatus (mit Hysterese)
//   Setzt *geaendert=true wenn sich Werte signifikant geaendert haben.
void aktualisiereBatterie(bool* geaendert) {
    if (geaendert != nullptr) *geaendert = false;

    uint16_t neueMv = nodeStatus.battery_mv;
    uint8_t neuerPct = nodeStatus.battery_pct;
    const bool messungOk = leseBatterie(&neueMv, &neuerPct);
    const bool neuerFault = !messungOk;

    if (messungOk) {
        // Prueft Hysterese: nur bei ausreichender Aenderung als "geaendert" markieren
        const bool mvDelta = deltaU16(neueMv, nodeStatus.battery_mv) >= BATTERY_STATE_DELTA_MV;
        const bool pctDelta = deltaU8(neuerPct, nodeStatus.battery_pct) >= BATTERY_STATE_DELTA_PCT;
        const bool faultDelta = nodeStatus.battery_fault != neuerFault;
        if (geaendert != nullptr && (mvDelta || pctDelta || faultDelta)) {
            *geaendert = true;
        }
        nodeStatus.battery_mv = neueMv;
        nodeStatus.battery_pct = neuerPct;
        nodeStatus.battery_fault = false;
        return;
    }

    // Messung fehlgeschlagen
    if (geaendert != nullptr && nodeStatus.battery_fault != neuerFault) {
        *geaendert = true;
    }
    nodeStatus.battery_fault = true;
}

// =============================================================================
// DEVICE-KANAELES – Zustandskanaele lesen und auf Aenderungen pruefen
// =============================================================================

// leseDeviceKanaele – Liest die generischen Kanaele via Custom-Hook
GenericStateChannels leseDeviceKanaele() {
    GenericStateChannels channels = {};
    device_build_state_channels(
        &channels.channel_bool_1,
        &channels.channel_u16_1,
        &channels.channel_mask_1,
        &channels.fault);
    return channels;
}

// sindKanaeleUnterschiedlich – Prueft ob zwei Channel-Zustaende abweichen
bool sindKanaeleUnterschiedlich(const GenericStateChannels& a, const GenericStateChannels& b) {
    return a.channel_bool_1 != b.channel_bool_1 ||
           a.channel_u16_1 != b.channel_u16_1 ||
           a.channel_mask_1 != b.channel_mask_1 ||
           a.fault != b.fault;
}

// pruefeDeviceEvent – Ruft Event-Hook auf und speichert Event falls vorhanden
void pruefeDeviceEvent() {
    uint8_t eventType = 0U;
    uint8_t trigger = SH_TRIGGER_UNKNOWN;
    uint8_t param1 = 0U;
    uint16_t param2 = 0U;
    if (!device_map_event(&eventType, &trigger, &param1, &param2)) return;
    if (eventType == 0U) return;

    nodeStatus.event.vorhanden = true;
    nodeStatus.event.event_type = eventType;
    nodeStatus.event.trigger = trigger;
    nodeStatus.event.param1 = param1;
    nodeStatus.event.param2 = param2;
    nodeStatus.event_report_offen = true;
    nodeStatus.state_report_offen = true;
}

// aktualisiereSchlafFenster – Setzt die frueheste Deep-Sleep-Zeit
void aktualisiereSchlafFenster(unsigned long fensterMs) {
    nodeStatus.schlaf_ab_ms = millis() + fensterMs;
}

// =============================================================================
// MASKEN & PEER – Sensor-/Input-Masken bauen, ESP-NOW-Peer registrieren
// =============================================================================

// buildSensorMask/buidInputMask – Erzeugen Sensormasken-Strings.
// Wenn BAT_SEN_SENSOR_MASK / BAT_SEN_INPUT_MASK nicht per DeviceConfig.h
// ueberschrieben werden, liefern diese Funktionen konstante Fallback-Strings.
// Eine constexpr-Optimierung ist moeglich, aber die Laufzeit-Flexibilitaet
// fuer Device-Hooks wird beibehalten.

// buildSensorMask – Baut Sensor-Maske (BAT-SEN: keine festen Sensoren)
void buildSensorMask(char* target, size_t targetSize) {
    if (!target || targetSize == 0U) return;
    copyText(target, targetSize, "XXXXXXXXXX");
}

// buildInputMask – Baut Input-Maske (BAT-SEN: keine festen Eingaenge)
void buildInputMask(char* target, size_t targetSize) {
    if (!target || targetSize == 0U) return;
    copyText(target, targetSize, "XXXXX");
}

// =============================================================================
// KOMMUNIKATION – ESP-NOW Senden: Peer, Paket, ACK
// =============================================================================

// stellePeerSicher – Registriert eine MAC als ESP-NOW-Peer
bool stellePeerSicher(const uint8_t* mac) {
    if (mac == nullptr) return false;
    if (!istBroadcastMac(mac) && !SmartHome::isValidMac(mac)) return false;
    if (esp_now_is_peer_exist(mac)) return true;

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = (uint8_t)WLAN_KANAL;
    peerInfo.encrypt = false;

    const esp_err_t err = esp_now_add_peer(&peerInfo);
    if (err != ESP_OK) {
        logf("WARN", "Peer konnte nicht angelegt werden (err=%d)", (int)err);
        return false;
    }
    return true;
}

// sendePaket – Zentrale ESP-NOW-Sendefunktion (baut Header+CRC, sendet)
bool sendePaket(
    const uint8_t* zielMac,
    uint8_t msgType,
    const void* payload,
    size_t payloadLen,
    const char* label)
{
    if (zielMac == nullptr || payloadLen > SH_MAX_PAYLOAD_BYTES) return false;
    if (!stellePeerSicher(zielMac)) return false;

    uint8_t packet[SH_ESPNOW_MAX_BYTES] = {0};
    SmartHome::MsgHeader header = {};
    SmartHome::fillHeader(header, msgType, nodeStatus.naechste_seq++, 0U, (uint16_t)payloadLen);

    uint8_t* payloadBuffer = packet + SH_HEADER_SIZE;
    if (payloadLen > 0U && payload != nullptr) {
        memcpy(payloadBuffer, payload, payloadLen);
    }

    SmartHome::finalizePacketCrc(header, payloadBuffer);
    memcpy(packet, &header, sizeof(header));

    const esp_err_t err = esp_now_send(zielMac, packet, SH_HEADER_SIZE + payloadLen);
    if (err != ESP_OK) {
        logf("WARN", "%s konnte nicht gesendet werden (err=%d)", label, (int)err);
        return false;
    }
    return true;
}

// sendePaketMitRetry – Sendet mit bis zu 2 Wiederholungen bei Fehler
#ifndef NODE_ESPNOW_RETRY_COUNT
#define NODE_ESPNOW_RETRY_COUNT 2
#endif
#ifndef NODE_ESPNOW_RETRY_DELAY_MS
#define NODE_ESPNOW_RETRY_DELAY_MS 50UL
#endif

bool sendePaketMitRetry(const uint8_t* zielMac, uint8_t msgType, const void* payload, size_t payloadLen, const char* label) {
    // Sequenznummer sichern, damit Wiederholungen dieselbe Seq verwenden.
    // sendePaket() inkrementiert nodeStatus.naechste_seq intern;
    // bei Misserfolg wird der gesicherte Wert wiederhergestellt.
    const uint8_t savedSeq = nodeStatus.naechste_seq;

    for (int attempt = 0; attempt <= NODE_ESPNOW_RETRY_COUNT; attempt++) {
        if (attempt > 0) {
            nodeStatus.naechste_seq = savedSeq;  // Gleiche Seq fuer Retry.
        }
        if (sendePaket(zielMac, msgType, payload, payloadLen, label)) return true;
        if (attempt < NODE_ESPNOW_RETRY_COUNT) {
            logf("WARN", "%s Retry %d/%d", label, attempt + 1, NODE_ESPNOW_RETRY_COUNT);
            delay(NODE_ESPNOW_RETRY_DELAY_MS);
        }
    }
    logf("ERROR", "%s nach %d Versuchen fehlgeschlagen", label, NODE_ESPNOW_RETRY_COUNT + 1);
    return false;
}

// sendeAck – Sendet eine ACK-Bestaetigung
bool sendeAck(const uint8_t* zielMac, uint8_t ackSeq, uint8_t ackMsgType, uint8_t status) {
    SmartHome::AckPayload payload = {};
    payload.ack_seq = ackSeq;
    payload.ack_msg_type = ackMsgType;
    payload.status = status;
    return sendePaket(zielMac, SH_MSG_ACK, &payload, sizeof(payload), "ACK");
}

// =============================================================================
// PROTOKOLL-NACHRICHTEN – HELLO, STATE, EVENT
// =============================================================================

// sendeHello – Sendet HELLO mit Batterie-Boot-Counter
bool sendeHello() {
    SmartHome::HelloPayload payload = {};
    char sensorMask[SH_SENSOR_MASK_LEN] = {0};
    char inputMask[SH_INPUT_MASK_LEN] = {0};

    buildSensorMask(sensorMask, sizeof(sensorMask));
    buildInputMask(inputMask, sizeof(inputMask));

    copyText(payload.device_id, sizeof(payload.device_id), DEVICE_ID);
    copyText(payload.device_name, sizeof(payload.device_name), DEVICE_NAME);
    payload.device_class = SH_CLASS_BAT_SEN;
    payload.caps_hi = (uint8_t)((DEVICE_CAPS >> 8) & 0xFFU);
    payload.caps_lo = (uint8_t)(DEVICE_CAPS & 0xFFU);
    payload.power_type = SH_POWER_BATTERY;
    payload.fw_version = 1U;
    payload.boot_counter = nodeStatus.boot_counter;
    payload.meta_schema_version = DEVICE_META_SCHEMA_VERSION;
    payload.control_mode = DEVICE_CONTROL_MODE;
    payload.config_profile = DEVICE_CONFIG_PROFILE;
    payload.reporting_mode = DEVICE_REPORTING_MODE;
    copyText(payload.sensor_mask, sizeof(payload.sensor_mask), sensorMask);
    copyText(payload.input_mask, sizeof(payload.input_mask), inputMask);

    nodeStatus.letztes_hello_ms = millis();
    return sendePaketMitRetry(helloZielMac(), SH_MSG_HELLO, &payload, sizeof(payload), "HELLO");
}

// sendeState – Sendet STATE mit Batterie- und Device-Kanaelen
//   Nutzt BatteryConfigStateReportPayload (mit report_interval_s).
//   Die Kanaele window_open/rain_raw/button_flags sind protokoll-historisch
//   benannt, der Basistyp behandelt sie als neutrale Device-Kanaele.
bool sendeState() {
    if (!nodeStatus.master_mac_gueltig) return false;

    SmartHome::BatteryConfigStateReportPayload payload = {};
    copyText(payload.node_id, sizeof(payload.node_id), DEVICE_ID);
    payload.battery_pct = nodeStatus.battery_pct;
    payload.battery_mv = nodeStatus.battery_mv;
    payload.window_open = nodeStatus.kanaele.channel_bool_1;
    payload.rain_raw = nodeStatus.kanaele.channel_u16_1;
    payload.button_flags = nodeStatus.kanaele.channel_mask_1;
    payload.fault = (nodeStatus.battery_fault || nodeStatus.kanaele.fault) ? 1U : 0U;
    payload.report_interval_s = (uint16_t)nodeStatus.wake_interval_s;

    if (!sendePaketMitRetry(nodeStatus.master_mac, SH_MSG_STATE, &payload, sizeof(payload), "STATE")) {
        return false;
    }

    nodeStatus.state_report_offen = false;
    nodeStatus.letzter_state_ms = millis();
    return true;
}

// sendeEvent – Sendet ein ausstehendes Device-Event
bool sendeEvent() {
    if (!nodeStatus.master_mac_gueltig || !nodeStatus.event.vorhanden) return false;

    SmartHome::EventReportPayload payload = {};
    copyText(payload.node_id, sizeof(payload.node_id), DEVICE_ID);
    payload.event_type = nodeStatus.event.event_type;
    payload.trigger = nodeStatus.event.trigger;
    payload.param1 = nodeStatus.event.param1;
    payload.param2 = nodeStatus.event.param2;

    if (!sendePaketMitRetry(nodeStatus.master_mac, SH_MSG_EVENT, &payload, sizeof(payload), "EVENT")) {
        return false;
    }

    nodeStatus.event_report_offen = false;
    nodeStatus.event.vorhanden = false;
    return true;
}

// =============================================================================
// PERSISTENZ – Konfiguration mit Rollback speichern (wake_interval, rx_window)
// =============================================================================

// speichereNodeBasisMitRollback – Persistiert und stellt bei Fehler Vorzustand her
bool speichereNodeBasisMitRollback(
    const SmartHome::ShNodeProvisioning::NodeBasisSnapshot& snapshot,
    uint32_t vorherWakeIntervalS,
    uint32_t vorherRxWindowMs) {
    if (nodeProvisioning.saveCurrentState()) return true;

    // Rollback: alte Werte wiederherstellen
    nodeProvisioning.restoreBasisSnapshot(snapshot);
    nodeStatus.wake_interval_s = vorherWakeIntervalS;
    nodeStatus.rx_window_ms = vorherRxWindowMs;
    return false;
}

// =============================================================================
// CFG-VERARBEITUNG – Wake-Intervall und RX-Fenster setzen
// =============================================================================

// uebernehmeWakeInterval – Setzt neues Wake-Intervall (mit Rollback)
bool uebernehmeWakeInterval(uint32_t valueS) {
    if (valueS < MIN_WAKE_INTERVAL_S || valueS > MAX_WAKE_INTERVAL_S) return false;

    SmartHome::ShNodeProvisioning::NodeBasisSnapshot snapshot = {};
    nodeProvisioning.captureBasisSnapshot(snapshot);
    const uint32_t vorherWakeIntervalS = nodeStatus.wake_interval_s;
    const uint32_t vorherRxWindowMs = nodeStatus.rx_window_ms;

    nodeStatus.wake_interval_s = valueS;
    nodeStatus.state_report_offen = true;
    aktualisiereSchlafFenster(nodeStatus.rx_window_ms);
    return speichereNodeBasisMitRollback(snapshot, vorherWakeIntervalS, vorherRxWindowMs);
}

// uebernehmeRxWindow – Setzt neues RX-Fenster (mit Rollback)
bool uebernehmeRxWindow(uint32_t valueMs) {
    if (valueMs < MIN_RX_WINDOW_MS || valueMs > MAX_RX_WINDOW_MS) return false;

    SmartHome::ShNodeProvisioning::NodeBasisSnapshot snapshot = {};
    nodeProvisioning.captureBasisSnapshot(snapshot);
    const uint32_t vorherWakeIntervalS = nodeStatus.wake_interval_s;
    const uint32_t vorherRxWindowMs = nodeStatus.rx_window_ms;

    nodeStatus.rx_window_ms = valueMs;
    nodeStatus.state_report_offen = true;
    aktualisiereSchlafFenster(nodeStatus.rx_window_ms);
    return speichereNodeBasisMitRollback(snapshot, vorherWakeIntervalS, vorherRxWindowMs);
}

// =============================================================================
// PROVISIONING-HANDLER – Web-Konfiguration (keine zusaetzlichen Felder)
// =============================================================================
class BatSenProvisioningHandler final
    : public SmartHome::ShNodeProvisioning::DeviceProvisioningHandler {
  public:
    const char* pageTitle() const override { return "BAT-SEN Provisioning"; }
    const char* pageIntro() const override {
        return "Master-Bindung, Wake-Takt und RX-Fenster fuer den Batteriepfad.";
    }
    const char* deviceSectionTitle() const override { return "BAT-SEN-Spezifisch"; }
    const char* deviceSectionIntro() const override {
        return "Batterieprofil und Pins sind Compile-Time-Geraetewerte.";
    }

    void loadDeviceDefaults() override {}
    bool loadDeviceSettings(Preferences& /*prefs*/) override { return false; }
    bool saveDeviceSettings(Preferences& /*prefs*/) override { return true; }
    bool clearDeviceSettings(Preferences& /*prefs*/) override { return true; }
    void captureDeviceSnapshot() override {}
    void restoreDeviceSnapshot() override {}
    bool parseDeviceSave(WebServer& /*server*/, String& /*errorText*/) override { return true; }
    void applyParsedDeviceSettings() override {}
    void discardParsedDeviceSettings() override {}
    void appendDeviceFieldsHtml(String& /*page*/, WebServer* /*sourceServer*/) const override {}
};

SmartHome::ShNodeProvisioning::NodeProvisioningController nodeProvisioning;
BatSenProvisioningHandler batSenProvisioningHandler;

// =============================================================================
// PROTOKOLL-VERARBEITUNG – HELLO_ACK, CMD, CFG, ESP-NOW Dispatch
// =============================================================================

// senderIstProvisionierterMaster – Prueft ob Sender der provisionierte Master ist
bool senderIstProvisionierterMaster(const uint8_t* senderMac) {
    return nodeStatus.master_mac_gueltig &&
           senderMac != nullptr &&
           memcmp(senderMac, nodeStatus.master_mac, sizeof(nodeStatus.master_mac)) == 0;
}

// verarbeiteHelloAck – Verarbeitet HELLO_ACK (markiert Master als bekannt)
void verarbeiteHelloAck(const uint8_t* senderMac, const SmartHome::HelloAckPayload& payload) {
    // Prueft ob ACK ok
    if (payload.ack_status != SH_ACK_OK) {
        logf("WARN", "HELLO_ACK abgelehnt");
        return;
    }

    // Prueft ob Master-MAC provisioniert
    if (!nodeStatus.master_mac_gueltig) {
        logf("WARN", "HELLO_ACK ignoriert: keine provisionierte Master-Bindung");
        return;
    }

    // Prueft ob Sender der provisionierte Master ist
    if (senderMac == nullptr ||
        memcmp(senderMac, nodeStatus.master_mac, sizeof(nodeStatus.master_mac)) != 0) {
        logf("WARN", "HELLO_ACK ignoriert: Sender ist nicht der provisionierte Master");
        return;
    }

    nodeStatus.master_bekannt = true;
    nodeStatus.state_report_offen = true;
    stellePeerSicher(nodeStatus.master_mac);
    aktualisiereSchlafFenster(nodeStatus.rx_window_ms);
    logf("INFO", "HELLO_ACK empfangen");
}

// verarbeiteCmd – Verarbeitet CMD (STATE_REQUEST)
void verarbeiteCmd(const uint8_t* senderMac, const SmartHome::CmdPayload& payload) {
    if (!senderIstProvisionierterMaster(senderMac)) {
        logf("WARN", "CMD ignoriert: Sender ist nicht der provisionierte Master");
        return;
    }

    if (payload.cmd_type == SH_CMD_STATE_REQUEST) {
        nodeStatus.state_report_offen = true;
        aktualisiereSchlafFenster(nodeStatus.rx_window_ms);
        return;
    }

    if (payload.cmd_type == SH_CMD_HELLO_REQUEST) {
        sendeHello();
        aktualisiereSchlafFenster(nodeStatus.rx_window_ms);
        return;
    }
}

// uebernehmeCfg – Wendet CFG-Parameter an (wake_interval, rx_window)
bool uebernehmeCfg(const SmartHome::CfgPayload& payload) {
    switch (payload.param_id) {
        case SH_CFG_REPORT_INTERVAL_S:
        case SH_CFG_WAKE_INTERVAL_S:
            return uebernehmeWakeInterval(payload.value);

        case SH_CFG_RX_WINDOW_MS:
            return uebernehmeRxWindow(payload.value);

        default:
            return false;
    }
}

// verarbeiteCfg – Verarbeitet CFG (mit Sender-Prüfung und ACK)
void verarbeiteCfg(const uint8_t* senderMac, const SmartHome::MsgHeader& header, const SmartHome::CfgPayload& payload) {
    if (!senderIstProvisionierterMaster(senderMac)) {
        logf("WARN", "CFG ignoriert: Sender ist nicht der provisionierte Master");
        return;
    }

    const bool ok = uebernehmeCfg(payload);
    if (header.flags & SH_FLAG_ACK_REQUEST) {
        sendeAck(senderMac, header.seq, header.msg_type, ok ? SH_ACK_OK : SH_ACK_ERROR);
    }
}

// verarbeiteEspNowPaket – CRC-Prüfung + Dispatch an Handler (switch/msg_type)
void verarbeiteEspNowPaket(const uint8_t* senderMac, const uint8_t* data, int len) {
    if (!senderMac || !data || len < (int)sizeof(SmartHome::MsgHeader)) return;
    if (!SmartHome::hasValidPacketCrc(data, (size_t)len)) return;

    const SmartHome::MsgHeader* header = reinterpret_cast<const SmartHome::MsgHeader*>(data);
    const uint8_t* payload = data + SH_HEADER_SIZE;

    switch (header->msg_type) {
        case SH_MSG_HELLO_ACK:
            if (header->payload_len == sizeof(SmartHome::HelloAckPayload)) {
                verarbeiteHelloAck(senderMac, *reinterpret_cast<const SmartHome::HelloAckPayload*>(payload));
            }
            break;

        case SH_MSG_CMD:
            if (header->payload_len == sizeof(SmartHome::CmdPayload)) {
                verarbeiteCmd(senderMac, *reinterpret_cast<const SmartHome::CmdPayload*>(payload));
            }
            break;

        case SH_MSG_CFG:
            if (header->payload_len == sizeof(SmartHome::CfgPayload)) {
                verarbeiteCfg(senderMac, *header, *reinterpret_cast<const SmartHome::CfgPayload*>(payload));
            }
            break;

        default:
            break;
    }
}

// ESP-NOW Callbacks (Core v3/v2, Recv+Send)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onEspNowReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (!info) return;
    verarbeiteEspNowPaket(info->src_addr, data, len);
}
#else
void onEspNowReceive(const uint8_t* senderMac, const uint8_t* data, int len) {
    verarbeiteEspNowPaket(senderMac, data, len);
}
#endif

void onEspNowSend(const wifi_tx_info_t* /*mac*/, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        logf("WARN", "ESP-NOW Versand fehlgeschlagen");
    }
}

// =============================================================================
// INITIALISIERUNG – Funk, IO-GPIOs
// =============================================================================

// initialisiereFunk – ESP-NOW initialisieren (WLAN, Callbacks, Peers)
void initialisiereFunk() {
    static uint8_t espNowInitFails = 0;
    constexpr uint8_t MAX_ESPNOW_INIT_FAILURES = 5;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.setSleep(false);

    const esp_err_t kanalErr = esp_wifi_set_channel((uint8_t)WLAN_KANAL, WIFI_SECOND_CHAN_NONE);
    if (kanalErr != ESP_OK) {
        logf("WARN", "WLAN-Kanal %d konnte nicht gesetzt werden (err=%d)", WLAN_KANAL, (int)kanalErr);
    }

    if (esp_now_init() != ESP_OK) {
        espNowInitFails++;
        logf("WARN", "ESP-NOW Initialisierung fehlgeschlagen (%u/%u)", espNowInitFails, MAX_ESPNOW_INIT_FAILURES);
        if (espNowInitFails >= MAX_ESPNOW_INIT_FAILURES) {
            logf("ERROR", "ESP-NOW init nach %u Versuchen fehlgeschlagen, restart", MAX_ESPNOW_INIT_FAILURES);
            ESP.restart();
        }
        return;
    }
    espNowInitFails = 0;  // Reset on success

    esp_now_register_send_cb(onEspNowSend);
    esp_now_register_recv_cb(onEspNowReceive);
    stellePeerSicher(BROADCAST_MAC);
    if (nodeStatus.master_mac_gueltig) {
        stellePeerSicher(nodeStatus.master_mac);
    }
}

// initialisiereIO – Initialisiert GPIOs (LED, ADC, Wake-Input, Custom-Hooks)
void initialisiereIO() {
    if (PIN_STATUS_LED >= 0) {
        pinMode(PIN_STATUS_LED, OUTPUT);
        digitalWrite(PIN_STATUS_LED, LOW);
    }

    if (PIN_BATTERY_ADC >= 0) {
        pinMode(PIN_BATTERY_ADC, INPUT);
    }

    if (PIN_WAKE_INPUT >= 0) {
        pinMode(PIN_WAKE_INPUT, INPUT);
    }

    device_init_io();
}

// =============================================================================
// LOOP-LOGIK – Polling, Events, Batterie
// =============================================================================

// pollLokaleHooks – Fragt Device-Hooks ab und aktualisiert Kanaele/Events
void pollLokaleHooks() {
    const bool hookDelta = device_poll_inputs();
    const GenericStateChannels neueKanaele = leseDeviceKanaele();
    if (hookDelta || sindKanaeleUnterschiedlich(nodeStatus.kanaele, neueKanaele)) {
        nodeStatus.kanaele = neueKanaele;
        nodeStatus.state_report_offen = true;
    }
    pruefeDeviceEvent();
}

// =============================================================================
// DEEP-SLEEP – Bedingungen pruefen, Wake-Quellen aktivieren, einschlafen
// =============================================================================

// darfInDeepSleep – Prueft ob Geraet in Deep-Sleep gehen darf
bool darfInDeepSleep(unsigned long jetzt) {
    if (!DEEP_SLEEP_AKTIV) return false;
    if (nodeStatus.stay_awake) return false;
    if (!nodeStatus.provisioning_bereit || nodeStatus.setup_mode || !nodeStatus.master_mac_gueltig) return false;

    // Nicht schlafen wenn Nachrichten gesendet werden muessen
    if (nodeStatus.state_report_offen || nodeStatus.event_report_offen || nodeStatus.event.vorhanden) {
        return false;
    }

    // Waehrend Discovery-Fenster wach bleiben
    if (!nodeStatus.master_bekannt) {
        return (jetzt - nodeStatus.boot_ms) >= DISCOVERY_WINDOW_MS;
    }

    // RX-Fenster abgelaufen?
    return istZeitErreicht(jetzt, nodeStatus.schlaf_ab_ms);
}

bool setupButtonIstAktiv() {
#if SETUP_BUTTON_PIN >= 0
    return SETUP_BUTTON_ACTIVE_LOW != 0
               ? (digitalRead(SETUP_BUTTON_PIN) == LOW)
               : (digitalRead(SETUP_BUTTON_PIN) == HIGH);
#else
    return false;
#endif
}

// Kurzer Setup-Tasterdruck toggelt Deep-Sleep-Sperre. Langer Druck bleibt Setup-Modus.
void aktualisiereStayAwakeToggle(unsigned long jetzt) {
    if (!STAY_AWAKE_TOGGLE_AKTIV) return;
#if SETUP_BUTTON_PIN >= 0
    const bool active = setupButtonIstAktiv();
    const unsigned long holdMs = SETUP_BUTTON_HOLD_MS > 0UL ? SETUP_BUTTON_HOLD_MS : 5000UL;

    if (active && !nodeStatus.stay_button_last_active) {
        nodeStatus.stay_button_pressed_at_ms = jetzt;
        nodeStatus.stay_button_hold_consumed = false;
    }

    if (active && !nodeStatus.stay_button_hold_consumed &&
        (jetzt - nodeStatus.stay_button_pressed_at_ms) >= holdMs) {
        nodeStatus.stay_button_hold_consumed = true;
    }

    if (!active && nodeStatus.stay_button_last_active) {
        const unsigned long pressedAt = nodeStatus.stay_button_pressed_at_ms;
        const unsigned long pressMs = pressedAt > 0UL ? (jetzt - pressedAt) : 0UL;
        if (!nodeStatus.stay_button_hold_consumed &&
            pressMs > 30UL &&
            pressMs < holdMs &&
            !nodeStatus.setup_mode) {
            nodeStatus.stay_awake = !nodeStatus.stay_awake;
            logf("INFO", "Stay-awake %s", nodeStatus.stay_awake ? "aktiv" : "inaktiv");
            if (!nodeStatus.stay_awake) {
                aktualisiereSchlafFenster(nodeStatus.rx_window_ms);
            }
        }
        nodeStatus.stay_button_pressed_at_ms = 0UL;
        nodeStatus.stay_button_hold_consumed = false;
    }

    nodeStatus.stay_button_last_active = active;
#endif
}

// Plattform-Helfer fuer GPIO-Wake (vermeidet #if-Labyrinth in aktiviereWakeQuellen).
static esp_err_t aktiviereGpioWakeHigh(uint64_t mask) {
#if CONFIG_IDF_TARGET_ESP32C3
    return (mask != 0ULL) ? esp_deep_sleep_enable_gpio_wakeup(mask, ESP_GPIO_WAKEUP_GPIO_HIGH) : ESP_OK;
#else
    return (mask != 0ULL) ? esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_HIGH) : ESP_OK;
#endif
}

static esp_err_t aktiviereGpioWakeLow(uint64_t mask) {
#if CONFIG_IDF_TARGET_ESP32C3
    return (mask != 0ULL) ? esp_deep_sleep_enable_gpio_wakeup(mask, ESP_GPIO_WAKEUP_GPIO_LOW) : ESP_OK;
#else
    if (mask != 0ULL) {
        logf("WARN", "LOW-Level-GPIO-Wake ist fuer dieses Ziel nicht verdrahtet");
    }
    return ESP_OK;
#endif
}

// aktiviereWakeQuellen – Konfiguriert Timer- und GPIO-Wake-Quellen
void aktiviereWakeQuellen() {
    // Timer-Wake (periodisch)
    const uint64_t wakeUs = (uint64_t)nodeStatus.wake_interval_s * 1000000ULL;
    esp_sleep_enable_timer_wakeup(wakeUs);

#if BAT_SEN_ENABLE_GPIO_WAKE || SETUP_BUTTON_PIN >= 0
    uint64_t wakeHighMask = 0ULL;
    uint64_t wakeLowMask = 0ULL;

    // GPIO-Wake von Device-Hooks
#if BAT_SEN_ENABLE_GPIO_WAKE
    const bool wakeHigh = deviceWakeLevelHigh();
    if (wakeHigh) {
        wakeHighMask |= device_wake_candidates();
    } else {
        wakeLowMask |= device_wake_candidates();
    }
    if (PIN_WAKE_INPUT >= 0 && PIN_WAKE_INPUT < 64) {
        fuegeWakePinHinzu(&wakeHighMask, &wakeLowMask, PIN_WAKE_INPUT, wakeHigh);
    }
#endif

    // Setup-Button als Wake-Quelle
#if SETUP_BUTTON_PIN >= 0
    fuegeWakePinHinzu(&wakeHighMask, &wakeLowMask, SETUP_BUTTON_PIN, SETUP_BUTTON_ACTIVE_LOW == 0);
#endif

    wakeHighMask = validiereWakeMask(wakeHighMask);
    wakeLowMask = validiereWakeMask(wakeLowMask);

    if (wakeHighMask == 0ULL && wakeLowMask == 0ULL) {
        logf("WARN", "GPIO-Wake aktiv, aber kein gueltiger Wake-Pin konfiguriert");
        return;
    }

    esp_err_t wakeErr = ESP_OK;
    wakeErr = aktiviereGpioWakeHigh(wakeHighMask);
    if (wakeErr == ESP_OK) {
        wakeErr = aktiviereGpioWakeLow(wakeLowMask);
    }

    if (wakeErr != ESP_OK) {
        logf("WARN", "GPIO-Wake konnte nicht aktiviert werden (err=%d)", (int)wakeErr);
    }
#endif
}

// starteDeepSleep – Aktiviert Wake-Quellen und geht in Deep-Sleep
void starteDeepSleep() {
    if (!DEEP_SLEEP_AKTIV) return;
    aktiviereWakeQuellen();
    logf("INFO", "Deep-Sleep fuer %lus", nodeStatus.wake_interval_s);
    if (DEBUG_LOKAL_AKTIV) {
        Serial.flush();
        delay(20);
    }
    esp_deep_sleep_start();
}

// =============================================================================
// Hilfsfunktionen fuer setup() – vermeiden Monolith-Funktion.
// =============================================================================

// Aufgabe: Initialisiert Boot-Counter, Wake-Reason und Grundzustand.
static void setupBootUndStatus(NodeState& ns) {
    ns.boot_ms = millis();
    ns.letztes_hello_ms = ns.boot_ms - HELLO_RETRY_INTERVAL_MS;
    ns.wake_interval_s = DEFAULT_WAKE_INTERVAL_S;
    ns.rx_window_ms = DEFAULT_RX_WINDOW_MS;
    ns.state_report_offen = true;
    ns.event_report_offen = false;
    ns.battery_fault = true;
    ns.wake_reason = wakeReasonCode();

    RTC_BOOT_COUNTER += 1U;
    ns.boot_counter = RTC_BOOT_COUNTER;
}

// Aufgabe: Initialisiert den Watchdog-Timer.
static void setupWatchdog() {
    esp_task_wdt_deinit();
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = (uint32_t)BAT_SEN_WDT_TIMEOUT_S * 1000UL,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL);
}

// Aufgabe: Konfiguriert und startet die Node-Provisioning-Schicht.
// Rueckgabe: true wenn Provisioning erfolgreich initialisiert wurde.
static bool setupProvisioning(
    NodeState& ns,
    BatSenProvisioningHandler& handler,
    void (*logCb)(const char*, const char*))
{
    SmartHome::ShNodeProvisioning::NodeProvisioningConfig cfg =
        SmartHome::BatSenProvisioning::makeConfig(
            DEVICE_ID,
            DEFAULT_WAKE_INTERVAL_S,
            DEFAULT_RX_WINDOW_MS,
            MIN_WAKE_INTERVAL_S,
            MAX_WAKE_INTERVAL_S,
            MIN_RX_WINDOW_MS,
            MAX_RX_WINDOW_MS);
    cfg.setupButtonPin = SETUP_BUTTON_PIN;
    cfg.setupButtonActiveLow = SETUP_BUTTON_ACTIVE_LOW != 0;
    cfg.setupButtonHoldMs = SETUP_BUTTON_HOLD_MS;
    cfg.setupIndicatorLedPin = SETUP_INDICATOR_LED_PIN;
    cfg.setupIndicatorLedActiveHigh = SETUP_INDICATOR_LED_ACTIVE_HIGH != 0;
    cfg.setupIndicatorBlinkMs = SETUP_INDICATOR_BLINK_MS;

    ns.provisioning_bereit = nodeProvisioning.begin(
        cfg,
        &ns.master_mac_gueltig,
        ns.master_mac,
        &ns.wake_interval_s,
        &ns.rx_window_ms,
        &ns.setup_mode,
        &ns.setup_ap_aktiv,
        &ns.restart_pending,
        &ns.restart_requested_at_ms,
        ns.setup_ap_ssid,
        sizeof(ns.setup_ap_ssid),
        &handler,
        logCb);

    if (!ns.provisioning_bereit) {
        logf("WARN", "Node-Provisioning-Basis konnte nicht initialisiert werden");
        return false;
    }

    // Batterie-Geraete: Config-Felder umgenutzt (Status-Intervall = Wake, Sensor-Intervall = RX).
    ns.wake_interval_s = nodeProvisioning.sanitizeStatusSendInterval(ns.wake_interval_s);
    ns.rx_window_ms     = nodeProvisioning.sanitizeSensorSendInterval(ns.rx_window_ms);
    return true;
}

// Aufgabe: Protokolliert den Geraetestart mit allen Kenndaten.
static void setupBootLog(const NodeState& ns) {
    logf("INFO", "%s v%s startet (%s)", DATEI_GERAET, DATEI_VERSION, PROJECT_VERSION);
    logf("INFO", "Node=%s Name=%s Variant=%s", DEVICE_ID, DEVICE_NAME, FW_VARIANT);
    logf("INFO",
         "WakeReason=%u BootCounter=%lu wake_interval_s=%lu rx_window_ms=%lu battery_profile=%u",
         ns.wake_reason,
         (unsigned long)ns.boot_counter,
         (unsigned long)ns.wake_interval_s,
         (unsigned long)ns.rx_window_ms,
         (unsigned)BAT_SEN_BATTERY_PROFILE);
}

// =============================================================================
// ARDUINO – setup() und loop()
// =============================================================================

void setup() {
    if (DEBUG_LOKAL_AKTIV) {
        Serial.begin(115200);
        delay(150);
    }

    nodeStatus = {};
    setupBootUndStatus(nodeStatus);

    initialisiereIO();
    nodeStatus.kanaele = leseDeviceKanaele();

    bool batteryChanged = false;
    aktualisiereBatterie(&batteryChanged);
    nodeStatus.letzte_batterie_probe_ms = millis();
    aktualisiereSchlafFenster(DISCOVERY_WINDOW_MS);

    setupWatchdog();

    if (!setupProvisioning(nodeStatus, batSenProvisioningHandler, provisioningLog)) {
        return;
    }

    setupBootLog(nodeStatus);

    if (!nodeProvisioning.hasStoredMasterMac()) {
        logf("INFO", "Keine persistierte Master-Bindung gefunden, starte Setup-Modus");
        nodeProvisioning.enterSetupMode();
        return;
    }

    initialisiereFunk();
    sendeHello();
}

void loop() {
    esp_task_wdt_reset();
    const unsigned long jetzt = millis();

    nodeProvisioning.update();
    aktualisiereStayAwakeToggle(jetzt);
    if (!nodeStatus.provisioning_bereit || nodeStatus.setup_mode) {
        delay(LOOP_INTERVAL_MS);
        return;
    }

    pollLokaleHooks();

    // Batterie periodisch pruefen (mit Hysterese)
    if ((jetzt - nodeStatus.letzte_batterie_probe_ms) >= BATTERY_SAMPLE_INTERVAL_MS) {
        nodeStatus.letzte_batterie_probe_ms = jetzt;
        bool batteryChanged = false;
        aktualisiereBatterie(&batteryChanged);
        if (batteryChanged) {
            nodeStatus.state_report_offen = true;
        }
    }

    // HELLO senden wenn Master nicht bekannt und Retry-Intervall abgelaufen
    // Erstaufruf (letztes_hello_ms == 0): Overflow-Mathe ergibt jetzt >= Interval → true.
    if (!nodeStatus.master_bekannt &&
        (jetzt - nodeStatus.letztes_hello_ms) >= HELLO_RETRY_INTERVAL_MS) {
        sendeHello();
    }

    // Ausstehende Events senden (wenn Master gueltig)
    if (nodeStatus.master_mac_gueltig && nodeStatus.event_report_offen) {
        if (sendeEvent()) {
            aktualisiereSchlafFenster(nodeStatus.rx_window_ms);
        }
    }

    // STATE senden wenn faellig (dirty oder Intervall abgelaufen)
    const bool stateFaellig =
        nodeStatus.master_mac_gueltig &&
        (nodeStatus.state_report_offen ||
         (nodeStatus.wake_interval_s > 0UL &&
          (jetzt - nodeStatus.letzter_state_ms) >=
              ((unsigned long)nodeStatus.wake_interval_s * 1000UL)));

    if (stateFaellig) {
        if (sendeState()) {
            aktualisiereSchlafFenster(nodeStatus.rx_window_ms);
        }
    }

    // Prueft ob Deep-Sleep erlaubt ist
    if (darfInDeepSleep(jetzt)) {
        starteDeepSleep();
    }

    delay(LOOP_INTERVAL_MS);
}
