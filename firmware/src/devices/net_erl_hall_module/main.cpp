/*
===============================================================================
 Datei: main.cpp
 Code-Name: NET-ERL Hall Module
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / Device-Code / Netzbetriebener Relais-Komfortaktor
 Ersteller: DevOpsOfChaos
 Datum: 2026-05-14
 Letzte Bearbeitung: 2026-05-18

 Zweck: Flurmodul mit PIR-Bewegung, Luxmessung, Klima-Messwerten und Relais
 Beschreibung: Dieser Device-Adapter ergaenzt den NET-ERL-Basistyp um konkrete
 Sensoren fuer den Flur. Der PIR meldet Anwesenheit, der VEML7700 liefert Lux
 fuer die Auto-Light-Entscheidung, der BME280 liefert Temperatur und Feuchte.
 Die Late-Lux-Logik wartet bei Bedarf auf den ersten gueltigen Lux-Wert, bevor
 das Relais automatisch eingeschaltet oder wegen zu hoher Helligkeit blockiert
 wird. Jede erneute PIR-Erkennung innerhalb der Nachlaufzeit startet den
 Auto-Off-Timer neu.

 Hardware:
 - ESP32-C3
 - BME280 fuer Temperatur und relative Feuchte
 - VEML7700 fuer Beleuchtungsstaerke in Lux
 - PIR-Bewegungssensor
 - Ein Relaisausgang

 Genutzte Bibliotheken:
 - Arduino.h: Arduino-Funktionen wie pinMode, digitalRead, digitalWrite und millis.
 - Wire.h: I2C-Bus fuer BME280 und VEML7700.
 - Adafruit_BME280.h: Fremdbibliothek fuer Temperatur- und Feuchtesensor BME280.
 - Adafruit_VEML7700.h: Fremdbibliothek fuer den Luxsensor VEML7700.
 - DeviceConfig.h: eigene Device-Konfiguration mit IDs, Intervallen und Schwellwerten.
 - PinConfig.h: eigene Pin-Zuordnung fuer Sensoren, Relais und Status-LED.
 - NetErlRuntime.h: eigener NET-ERL-Basistyp; liefert setup(), loop(), Funklogik,
   Auto-Light-Grundlogik und ruft die Device-Hooks aus dieser Datei auf.

 Aenderungsverlauf:
 - 2026-05-14: Device-Code fuer NET-ERL Hall Module angelegt.
 - 2026-05-18: Kommentarstil vereinheitlicht und Doxygen-Metakommentare entfernt.
===============================================================================
*/

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_VEML7700.h>

#include "DeviceConfig.h"
#include "PinConfig.h"

// Baukasten-Defines fuer den NET-ERL-Basistyp. Diese Werte muessen vor
// NetErlRuntime.h gesetzt sein, weil der Basistyp sie beim Einbinden auswertet.
#define NET_ERL_STORAGE_NS              "net_erl_hl"
#define NET_ERL_SENSOR_MASK             "THLMXXXXXX"
#define NET_ERL_INPUT_MASK              "XXXXX"
#define NET_ERL_PERSISTED_MAGIC         0x484C4C31UL
#define NET_ERL_PERSISTED_KEY           "hall_setup_v1"
#define NET_ERL_DEVICE_PAGE_TITLE       "NET-ERL Hall Module"
#define NET_ERL_DEVICE_SECTION_TITLE    "Hall Module"
#define NET_ERL_DEVICE_SECTION_INTRO    "Lux-Schwelle und Nachlauf."

// Hall-Modul-spezifische Unterschiede zum LED-Ring-Modul.
#define NET_ERL_USE_ISR_CMD_QUEUE       1   // CMD-Queue darf aus Interrupt-Kontext genutzt werden.
#define NET_ERL_OFF_TIMER_EXTENDS_ON_MOTION 1  // Neue PIR-Erkennung setzt den Nachlauf-Timer zurueck.
#define NET_ERL_WDT_TIMEOUT_S           8UL // Watchdog-Zeit in Sekunden.

// Aktiviert die Device-Hooks in dieser Datei.
#define NET_ERL_DEVICE_HAS_CUSTOM_HOOKS 1

// =============================================================================
// RUNTIME - Basistyp liefert setup(), loop() und gemeinsame NET-ERL-Logik
// =============================================================================
#include "../../basetypes/net_erl/NetErlRuntime.h"

// =============================================================================
// DEVICE-SPEZIFISCHE OBJEKTE
// =============================================================================

Adafruit_BME280 bme280;
Adafruit_VEML7700 veml7700 = Adafruit_VEML7700();

// =============================================================================
// DEVICE-SPEZIFISCHER ZUSTAND
// =============================================================================
namespace {
    bool bme280_ok = false, veml7700_ok = false;
    unsigned long letzter_bme_recovery_ms = 0, letzter_veml_recovery_ms = 0;
    unsigned long letztes_env_sample_ms = 0;
    unsigned long veml7700_bereit_seit_ms = 0;

    // Sensor-Messwerte
    int16_t temp_01c = INT16_MIN;
    uint16_t hum_01pct = 0xFFFFU, lux = 0xFFFFU;
    bool pir_raw = false;

    constexpr uint32_t SENSOR_POLL_INTERVAL_MS = 250UL; // 250 Millisekunden.
}

// =============================================================================
// SENSOR-HILFSFUNKTIONEN (device-spezifisch)
// =============================================================================
namespace {
    // Aufgabe: Initialisiert den BME280 an der konfigurierten I2C-Adresse.
    // Eingabewerte: keine; NET_ERL_BME280_ADDRESS kommt aus DeviceConfig.h.
    // Ausgabewert: true bedeutet, der Sensor wurde gefunden und ist nutzbar.
    bool initBme280() {
        const uint8_t addrs[] = {(uint8_t)NET_ERL_BME280_ADDRESS};
        for (uint8_t a : addrs) {
            if (!bme280.begin(a, &Wire)) continue;
            return true;
        }
        return false;
    }

    // Aufgabe: Konfiguriert den VEML7700 fuer die Luxmessung.
    // Eingabewerte: keine.
    // Ausgabewert: keiner; die Adafruit_VEML7700-Instanz wird intern konfiguriert.
    // 100 ms Integrationszeit bedeuten, dass eine einzelne Messung etwa 0,1 Sekunden Licht sammelt.
    void konfVeml7700() {
        veml7700.setGain(VEML7700_GAIN_1);
        veml7700.setIntegrationTime(VEML7700_IT_100MS);
    }

    // Aufgabe: Initialisiert den VEML7700 und merkt den Bereit-Zeitpunkt.
    // Eingabewert: jetzt ist der aktuelle millis()-Zeitstempel in Millisekunden.
    // Ausgabewert: true bedeutet, der Sensor antwortet und wurde konfiguriert.
    bool initVeml7700(unsigned long jetzt) {
        if (!veml7700.begin()) return false;
        konfVeml7700();
        veml7700_bereit_seit_ms = jetzt;
        return true;
    }
}

// =============================================================================
// CUSTOM HOOKS (von NetErlRuntime.h aufgerufen)
// =============================================================================

// Aufgabe: Initialisiert I2C-Bus, BME280, VEML7700 und PIR-Pin.
// Eingabewerte: keine; Pins und Adressen kommen aus DeviceConfig.h und PinConfig.h.
// Ausgabewert: keiner; die lokalen Sensor-OK-Flags werden gesetzt.
// Aufrufer: NetErlRuntime ruft diesen Hook einmal beim Boot auf.
void netErlDeviceInit() {
    Wire.begin(PIN_SENSOR_SDA, PIN_SENSOR_SCL);

    bme280_ok = initBme280();
    if (!bme280_ok) logMsg("WARN", "BME280 init fail");

    veml7700_ok = initVeml7700(millis());
    if (!veml7700_ok) logMsg("WARN", "VEML7700 init fail");

    pinMode(PIN_PIR, INPUT);
// Status LED not used when PIN_STATUS_LED < 0
#if PIN_STATUS_LED >= 0
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);
#endif
    
#if SETUP_INDICATOR_LED_PIN >= 0
    pinMode(SETUP_INDICATOR_LED_PIN, OUTPUT);
    /* set setup indicator off (respect active polarity) */
    digitalWrite(SETUP_INDICATOR_LED_PIN, SETUP_INDICATOR_LED_ACTIVE_HIGH ? LOW : HIGH);
#endif
}

// Aufgabe: Setzt alle Sensorwerte auf die ungueltigen Startwerte zurueck.
// Eingabewerte: keine.
// Ausgabewert: keiner; INT16_MIN und 0xFFFFU markieren ungueltige Messwerte.
void netErlDeviceResetSensorDefaults() {
    temp_01c = INT16_MIN; hum_01pct = 0xFFFFU; lux = 0xFFFFU;
}

// Aufgabe: Liest den PIR-Bewegungssensor und speichert den Rohstatus.
// Eingabewerte: keine; PIN_PIR kommt aus PinConfig.h.
// Ausgabewert: true bedeutet, der PIR-Pin steht auf HIGH und Bewegung wurde erkannt.
bool netErlDeviceReadPresence() {
    pir_raw = (digitalRead(PIN_PIR) == HIGH);
    return pir_raw;
}

// Aufgabe: Schaltet den Relaisausgang und optional die Status-LED.
// Eingabewert: on=true bedeutet Relais soll aktiv sein.
// Ausgabewert: keiner; digitalWrite setzt die GPIO-Pegel.
// RELAY_1_ACTIVE_HIGH legt fest, ob HIGH oder LOW den aktiven Relaiszustand bedeutet.
void netErlDeviceSetRelayOutput(bool on) {
    digitalWrite(PIN_RELAY_1, on == RELAY_1_ACTIVE_HIGH ? HIGH : LOW);
#if PIN_STATUS_LED >= 0
    digitalWrite(PIN_STATUS_LED, on ? HIGH : LOW);
#endif
}

// Aufgabe: Pollt die Sensoren, versucht Recovery und fuehrt die Late-Lux-Entscheidung aus.
// Eingabewert: nowMs ist der aktuelle millis()-Zeitstempel in Millisekunden.
// Ausgabewert: keiner; Messwerte, runtime.fault und runtime.state_report_offen werden aktualisiert.
//
// Ablauf:
// 1. NET_ERL_ENV_SAMPLE_INTERVAL_MS begrenzt die Messrate. Der Wert ist in Millisekunden.
// 2. Ausgefallene Sensoren werden nach SENSOR_RECOVERY_RETRY_INTERVAL_MS erneut initialisiert.
// 3. BME280 liefert Temperatur in Grad Celsius und relative Feuchte in Prozent.
// 4. VEML7700 liefert Lux; negative oder NaN-Werte gelten als Fehler.
// 5. Late-Lux entscheidet ein wartendes Auto-On erst, wenn ein gueltiger Lux-Wert vorliegt.
// 6. Delta-Detection setzt nur bei relevanten Messwertaenderungen einen STATE-Report.
//
// Master-Kommandos duerfen Auto-Light spaeter uebersteuern. Die urspruengliche
// Auto-On-Entscheidung bleibt trotzdem sauber getrennt.
void netErlDevicePollSensors(unsigned long nowMs) {
    if ((nowMs - letztes_env_sample_ms) < NET_ERL_ENV_SAMPLE_INTERVAL_MS) return;
    letztes_env_sample_ms = nowMs;

    // Recovery: ausgefallene Sensoren erst nach dem Retry-Intervall neu initialisieren.
    if (!bme280_ok && recoveryIsDue(letzter_bme_recovery_ms, nowMs, SENSOR_RECOVERY_RETRY_INTERVAL_MS)) {
        letzter_bme_recovery_ms = nowMs; bme280_ok = initBme280();
    }
    if (!veml7700_ok && recoveryIsDue(letzter_veml_recovery_ms, nowMs, SENSOR_RECOVERY_RETRY_INTERVAL_MS)) {
        letzter_veml_recovery_ms = nowMs; veml7700_ok = initVeml7700(nowMs);
    }

    // BME280 lesen: Temperatur wird spaeter als Zehntelgrad, Feuchte als 0,1 Prozent gemeldet.
    if (bme280_ok) {
        float t = bme280.readTemperature();
        float h = bme280.readHumidity();
        if (!isnan(t) && !isnan(h) && h >= 0 && h <= 100) {
            temp_01c = (int16_t)lroundf(t * 10.0f);
            hum_01pct = clampHum01pct((long)lroundf(h * 10.0f));
        } else { bme280_ok = false; logMsg("WARN", "BME280 unplausibel"); }
    }

    // VEML7700 lesen: Lux wird auf uint16_t begrenzt, damit der Protokoll-Payload passt.
    if (veml7700_ok) {
        float l = veml7700.readLux();
        if (!isnan(l) && l >= 0) lux = clampToU16((long)lroundf(l));
        else { veml7700_ok = false; logMsg("WARN", "VEML7700 read fail"); }
    }

    // Status aktualisieren: fault=true sobald einer der beiden Sensoren nicht verfuegbar ist.
    runtime.fault = !(bme280_ok && veml7700_ok);

    // Late-Lux: Auto-On-Entscheidung erst ausfuehren, wenn ein Lux-Wert verfuegbar ist.
    if (runtime.motion_aktiv && runtime.pending_auto_on_decision && !runtime.relay_1 && lux != 0xFFFFU) {
        runtime.pending_auto_on_decision = false;
        if (lux <= runtime.auto_on_lux_threshold) {
            runtime.relay_auto_owned = true; runtime.blocked_by_lux = false;
            setRelay(true, "auto_on_late_lux");
            sendRelayEvent(SH_TRIGGER_AUTO);
            runtime.state_report_offen = true;
        } else {
            runtime.blocked_by_lux = true;
            runtime.state_report_offen = true;
        }
    }

    // Delta-Detection: STATE-Trigger nur bei signifikanter Sensorwert-Aenderung.
    {
        static int16_t  last_temp = INT16_MIN;
        static uint16_t last_hum = 0xFFFFU, last_lux = 0xFFFFU;
        
        bool changed = false;
        if (temp_01c != last_temp) { last_temp = temp_01c; changed = true; }
        if (absDiffU16(hum_01pct, last_hum) >= 5U) { last_hum = hum_01pct; changed = true; }
        if (absDiffU16(lux, last_lux) >= 5U) { last_lux = lux; changed = true; }
        
        if (changed) runtime.state_report_offen = true;
    }
}

// Aufgabe: Befuellt den RelayComfortConfigStateReportPayload mit aktuellen Messwerten.
// Eingabewerte:
// - payload zeigt auf den vom Basistyp bereitgestellten Payload-Speicher.
// - size zeigt auf die verfuegbare Groesse und wird danach auf die benoetigte Groesse gesetzt.
// Ausgabewert: keiner; der Payload enthaelt danach Relais, Sensorwerte, Auto-Flags und Fehlerstatus.
void netErlDeviceFillStatePayload(void* payload, size_t* size) {
    SmartHome::RelayComfortConfigStateReportPayload* p =
        static_cast<SmartHome::RelayComfortConfigStateReportPayload*>(payload);
    if (p != nullptr && size && *size >= sizeof(*p)) {
        safeStrCopy(p->node_id, sizeof(p->node_id), DEVICE_ID);
        p->relay_1 = runtime.relay_1 ? 1U : 0U;
        p->temp_01c = temp_01c;
        p->hum_01pct = hum_01pct;
        p->lux = lux;
        p->motion = runtime.motion_aktiv ? 1U : 0U;
        p->auto_flags = netErlDeviceBuildAutoFlags();
        p->fault = runtime.fault ? 1U : 0U;
        p->report_interval_s = (uint16_t)runtime.report_interval_s;
        p->auto_on_lux_threshold = runtime.auto_on_lux_threshold;
    }
    if (size != nullptr) *size = sizeof(SmartHome::RelayComfortConfigStateReportPayload);
}

// Aufgabe: Baut die Auto-Light-Flags fuer den STATE-Report.
// Eingabewerte: keine; Status kommt aus runtime und lokalen Sensorflags.
// Ausgabewert: Bitmaske aus SH_RELAY_COMFORT_FLAG_*-Werten.
//
// Gesetzte Flags:
// - PRESENCE_SOURCE_AVAILABLE: PIR meldet Bewegung.
// - LIGHT_VALUE_AVAILABLE: VEML7700 ist verfuegbar.
// - LIGHT_GUARD_ENABLED: Luxschutz ist fuer dieses Device immer aktiv.
// - AUTO_RELAY_OWNED: Auto-Light steuert aktuell das Relais.
// - BLOCKED_BY_LUX: Auto-On wurde wegen zu hoher Helligkeit blockiert.
// - 0x10: Hall-spezifisches Flag fuer fehlenden Lux-Wert waehrend Pending-Entscheidung.
uint8_t netErlDeviceBuildAutoFlags() {
    uint8_t f = 0;
    if (runtime.motion_aktiv) f |= SH_RELAY_COMFORT_FLAG_PRESENCE_SOURCE_AVAILABLE;
    if (veml7700_ok) f |= SH_RELAY_COMFORT_FLAG_LIGHT_VALUE_AVAILABLE;
    f |= SH_RELAY_COMFORT_FLAG_LIGHT_GUARD_ENABLED;
    if (runtime.relay_auto_owned) f |= SH_RELAY_COMFORT_FLAG_AUTO_RELAY_OWNED;
    if (runtime.blocked_by_lux) f |= SH_RELAY_COMFORT_FLAG_BLOCKED_BY_LUX;
    // Hall-spezifisch: BLOCKED_BY_MISSING_LUX-Flag.
    if (runtime.pending_auto_on_decision && lux == 0xFFFFU) f |= 0x10;
    return f;
}

// Aufgabe: Liefert den zuletzt gemessenen Luxwert fuer eine sofortige Auto-On-Entscheidung.
// Eingabewert: luxOut zeigt auf den Ausgabespeicher fuer den Luxwert.
// Ausgabewert: true bedeutet, luxOut enthaelt einen gueltigen Luxwert.
//
// Hintergrund: Der PIR wird schneller gepollt als der Umweltsensor. Beim ersten
// Motion-HIGH soll die Lampe trotzdem sofort entscheiden koennen, ob es dunkel
// genug ist. Deshalb nutzt die Runtime den letzten bekannten VEML7700-Wert.
bool netErlDeviceGetCachedLux(uint16_t* luxOut) {
    if (luxOut == nullptr || !veml7700_ok || lux == 0xFFFFU) {
        return false;
    }
    *luxOut = lux;
    return true;
}

// Aufgabe: Meldet dem Basistyp, ob ein Sensorfehler vorliegt.
// Eingabewerte: keine; lokale OK-Flags werden ausgewertet.
// Ausgabewert: true bedeutet, BME280 oder VEML7700 ist nicht verfuegbar.
bool netErlDeviceHasSensorFault() {
    return !(bme280_ok && veml7700_ok);
}

// Aufgabe: Schreibt einen kompakten Snapshot der aktuellen Sensor- und Relaiswerte ins Log.
// Eingabewerte: keine.
// Ausgabewert: keiner; logMsg stammt aus dem NET-ERL-Basistyp.
void netErlDeviceLogSnapshot() {
    logMsg("INFO", "snap t=%d h=%u l=%u m=%s r=%s auto=%s bl=%s fa=%s",
        (int)temp_01c, hum_01pct, lux,
        runtime.motion_aktiv ? "1" : "0", runtime.relay_1 ? "1" : "0",
        runtime.relay_auto_owned ? "1" : "0",
        runtime.blocked_by_lux ? "1" : "0",
        runtime.fault ? "1" : "0");
}
