/*
===============================================================================
 Datei: main.cpp
 Code-Name: NET-ERL Hall Module LED Ring
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / Device-Code / Netzbetriebener Relais-Komfortaktor
 Ersteller: DevOpsOfChaos
 Datum: 2026-05-14
 Letzte Bearbeitung: 2026-05-18

 Zweck: Flurmodul mit Radar-Praesenz, Luftqualitaet, Luxmessung, Relais und LED-Ring
 Beschreibung: Dieser Device-Adapter ist die umfangreichste NET-ERL-Variante.
 Er verbindet den NET-ERL-Basistyp mit LD2410-Radar, BME680, VEML7700, ENS160
 und NeoPixel-LED-Ring. Der BME680 liefert Temperatur, Feuchte, Luftdruck und
 Gaswiderstand. Der ENS160 nutzt Temperatur und Feuchte als Kompensation und
 liefert AQI, TVOC und eCO2. Der VEML7700 liefert Lux fuer die Auto-Light-
 Entscheidung. Der LED-Ring zeigt den Relaiszustand an.

 Wichtige Zeitwerte:
 - 400 ms VEML7700-Integrationszeit: laengere Lichtmessung fuer stabilere Luxwerte.
 - 150 ms BME680-Heizprofil: Heizdauer fuer eine Gas-Messung.
 - 180 s BME680-Gaswarmup: 180 Sekunden, also 3 Minuten, bis Gaswerte gemeldet werden.
 - SENSOR_RECOVERY_RETRY_INTERVAL_MS: Millisekunden-Abstand fuer Sensor-Recovery.

 Hardware:
 - ESP32-C3
 - BME680 fuer Temperatur, relative Feuchte, Luftdruck und Gaswiderstand
 - VEML7700 fuer Beleuchtungsstaerke in Lux
 - ENS160 fuer AQI, TVOC und eCO2
 - LD2410-Radar fuer Praesenz
 - NeoPixel-LED-Ring
 - Ein Relaisausgang
 - Ein lokaler Button

 Genutzte Bibliotheken:
 - Arduino.h: Arduino-Funktionen wie pinMode, digitalRead und digitalWrite.
 - Wire.h: I2C-Bus fuer BME680, VEML7700 und ENS160.
 - Adafruit_BME680.h: Fremdbibliothek fuer den BME680.
 - Adafruit_VEML7700.h: Fremdbibliothek fuer den VEML7700.
 - Adafruit_NeoPixel.h: Fremdbibliothek fuer den LED-Ring.
 - ScioSense_ENS160.h: Fremdbibliothek fuer den ENS160-Luftqualitaetssensor.
 - DeviceConfig.h: eigene Device-Konfiguration mit IDs, Adressen, Intervallen und Schwellwerten.
 - PinConfig.h: eigene Pin-Zuordnung fuer Sensoren, Relais, Button und LED-Ring.
 - NetErlRuntime.h: eigener NET-ERL-Basistyp; liefert setup(), loop(), Funklogik,
   Auto-Light-Grundlogik und ruft die Device-Hooks aus dieser Datei auf.

 Aenderungsverlauf:
 - 2026-05-14: Device-Code fuer NET-ERL Hall Module LED Ring angelegt.
 - 2026-05-18: Kommentarstil vereinheitlicht und Doxygen-Metakommentare entfernt.
===============================================================================
*/

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME680.h>
#include <Adafruit_VEML7700.h>
#include <Adafruit_NeoPixel.h>
#include <ScioSense_ENS160.h>

#ifndef ENS160_REG_TEMP_IN
#define ENS160_REG_TEMP_IN 0x13
#endif

#include "DeviceConfig.h"
#include "PinConfig.h"

// Baukasten-Defines fuer den NET-ERL-Basistyp. Diese Werte muessen vor
// NetErlRuntime.h gesetzt sein, weil der Basistyp sie beim Einbinden auswertet.
#define NET_ERL_STORAGE_NS              "net_erl_hlr"
#define NET_ERL_SENSOR_MASK             "THLPGAMXXX"
#define NET_ERL_INPUT_MASK              "BXXXX"
#define NET_ERL_PERSISTED_MAGIC         0x4B544331UL
#define NET_ERL_PERSISTED_KEY           "hall_led_cfg_v1"
#define NET_ERL_DEVICE_PAGE_TITLE       "NET-ERL Hall Module LED Ring"
#define NET_ERL_DEVICE_SECTION_TITLE    "Hall Module LED Ring"
#define NET_ERL_DEVICE_SECTION_INTRO    "Lux-Schwelle und Nachlauf."
#define NET_ERL_HAS_BUTTON
#define NET_ERL_HAS_INDICATOR_UPDATE

// Aktiviert die Device-Hooks in dieser Datei.
#define NET_ERL_DEVICE_HAS_CUSTOM_HOOKS 1

// =============================================================================
// RUNTIME - Basistyp liefert setup(), loop() und gemeinsame NET-ERL-Logik
// =============================================================================
#include "../../basetypes/net_erl/NetErlRuntime.h"

using SmartHome::absDiffU32;

// =============================================================================
// DEVICE-SPEZIFISCHE OBJEKTE
// =============================================================================

Adafruit_BME680 bme680;
Adafruit_VEML7700 veml = Adafruit_VEML7700();
ScioSense_ENS160 ens160Addr52(NET_ERL_ENS160_PRIMARY_ADDRESS);
ScioSense_ENS160 ens160Addr53(NET_ERL_ENS160_FALLBACK_ADDRESS);
ScioSense_ENS160* ens160 = nullptr;
Adafruit_NeoPixel ledRing(LED_RING_COUNT, PIN_LED_RING, NEO_GRB + NEO_KHZ800);

// =============================================================================
// DEVICE-SPEZIFISCHER ZUSTAND
// =============================================================================
namespace {
    bool bme_ok = false, lux_ok = false, ens_ok = false;
    uint8_t bme680_adresse = 0, ens160_adresse = 0;
    uint8_t bme680_gueltige_messungen = 0;
    unsigned long letzter_bme_recovery_ms = 0, letzter_lux_recovery_ms = 0, letzter_ens_recovery_ms = 0;
    unsigned long letztes_env_sample_ms = 0, letzter_ens_gueltig_ms = 0;
    bool ring_initialized = false;

    // Sensor-Messwerte
    int16_t temp_01c = INT16_MIN;
    uint16_t hum_01pct = 0xFFFFU, lux = 0xFFFFU;
    uint32_t pressure_pa = 0xFFFFFFFFUL, gas_ohm = 0xFFFFFFFFUL;
    uint16_t aqi = 0xFFFFU, tvoc_ppb = 0xFFFFU, eco2_ppm = 0xFFFFU;
    bool ld2410_raw = false;

    constexpr uint32_t PRESSURE_UNGUELTIG = 0xFFFFFFFFUL;
    constexpr uint32_t GAS_OHM_UNGUELTIG = 0xFFFFFFFFUL;
    constexpr uint16_t AIR_METRIC_UNGUELTIG = 0xFFFFU;
    constexpr uint16_t ENS160_AQI_MAX_BASIC = 5U;
    constexpr uint8_t LED_RING_HELLIGKEIT = 24U; // NeoPixel-Helligkeit auf Skala 0 bis 255.
    constexpr uint16_t I2C_TIMEOUT_MS = 50U;     // 50 Millisekunden I2C-Timeout.
}

// =============================================================================
// SENSOR-HILFSFUNKTIONEN (device-spezifisch)
// =============================================================================
namespace {
    // Aufgabe: Mappt den ENS160-AQI-Rohwert von 1 bis 5 auf die Projekt-Skala 0 bis 500.
    // Eingabewert: r ist der AQI-Rohwert des ENS160.
    // Ausgabewert: 100 bis 500 bei gueltigem Rohwert, 0 bei ungueltigem Rohwert.
    uint16_t mapAqi500(uint16_t r) { return (r >= 1 && r <= ENS160_AQI_MAX_BASIC) ? (uint16_t)(r * 100U) : 0U; }

    // Aufgabe: Kodiert Celsius fuer die ENS160-Temperaturkompensation.
    // Eingabewert: c ist Temperatur in Grad Celsius.
    // Ausgabewert: Temperatur als Kelvin mal 64, wie vom ENS160-Register erwartet.
    uint16_t encT(float c) { return (uint16_t)((c + 273.15f) * 64.0f); }

    // Aufgabe: Kodiert relative Feuchte fuer die ENS160-Feuchtekompensation.
    // Eingabewert: h ist relative Feuchte in Prozent.
    // Ausgabewert: Feuchte mal 512, wie vom ENS160-Register erwartet.
    uint16_t encH(float h) { return (uint16_t)(h * 512.0f); }

    // Aufgabe: Schreibt Temperatur- und Feuchtekompensation direkt in ENS160-Register.
    // Eingabewerte:
    // - a: I2C-Adresse des ENS160.
    // - t: Temperatur in Grad Celsius.
    // - h: relative Feuchte in Prozent.
    // Ausgabewert: I2C-Fehlercode von Wire.endTransmission(); 0 bedeutet erfolgreich.
    int writeEnsEnv(uint8_t a, float t, float h) {
        uint8_t b[4]; uint16_t te = encT(t), he = encH(h);
        b[0] = te & 0xFF; b[1] = (te >> 8) & 0xFF; b[2] = he & 0xFF; b[3] = (he >> 8) & 0xFF;
        Wire.beginTransmission(a); Wire.write(ENS160_REG_TEMP_IN); Wire.write(b, 4); return Wire.endTransmission();
    }

    // Aufgabe: Konfiguriert den VEML7700 fuer Luxmessungen.
    // Eingabewerte: keine.
    // Ausgabewert: keiner; 400 ms Integrationszeit bedeuten 0,4 Sekunden Lichtmessung.
    void konfVeml() { veml.setGain(VEML7700_GAIN_1); veml.setIntegrationTime(VEML7700_IT_400MS); }

    // Aufgabe: Initialisiert den BME680 ueber primaere oder Fallback-I2C-Adresse.
    // Eingabewerte: keine; Adressen und I2C-Bus kommen aus DeviceConfig.h und Wire.
    // Ausgabewert: true bedeutet, der Sensor wurde gefunden und konfiguriert.
    // Das Heizprofil nutzt 320 Grad Celsius fuer 150 Millisekunden Gas-Messzeit.
    bool initBme() {
        uint8_t addrs[] = {(uint8_t)NET_ERL_BME680_PRIMARY_ADDRESS, (uint8_t)NET_ERL_BME680_FALLBACK_ADDRESS};
        for (uint8_t a : addrs) {
            if (!bme680.begin(a, &Wire)) continue;
            bme680.setTemperatureOversampling(BME680_OS_8X); bme680.setHumidityOversampling(BME680_OS_2X);
            bme680.setPressureOversampling(BME680_OS_4X); bme680.setIIRFilterSize(BME680_FILTER_SIZE_3);
            bme680.setGasHeater(320U, 150U); bme680_adresse = a; return true;
        }
        return false;
    }

    // Aufgabe: Initialisiert den ENS160 ueber primaere oder Fallback-I2C-Adresse.
    // Eingabewerte: keine; die Adressen kommen aus DeviceConfig.h.
    // Ausgabewert: true bedeutet, ein ENS160 wurde gefunden.
    bool initEns() {
        ens160 = &ens160Addr52; if (ens160->begin()) { ens160_adresse = NET_ERL_ENS160_PRIMARY_ADDRESS; return true; }
        ens160 = &ens160Addr53; if (ens160->begin()) { ens160_adresse = NET_ERL_ENS160_FALLBACK_ADDRESS; return true; }
        ens160 = nullptr; return false;
    }

    // Aufgabe: Prueft, ob der BME680-Gassensor ausreichend warmgelaufen ist.
    // Eingabewert: j ist der aktuelle millis()-Zeitstempel in Millisekunden.
    // Ausgabewert: true bedeutet, Gaswerte duerfen als gueltig gemeldet werden.
    bool gasWarmupOk(unsigned long j) {
        return gasWarmupComplete(runtime.boot_ms, j, NET_ERL_BME680_GAS_WARMUP_MS, bme680_gueltige_messungen, NET_ERL_BME680_GAS_WARMUP_MIN_READS);
    }

    // Aufgabe: Prueft, ob die letzten ENS160-Messwerte veraltet sind.
    // Eingabewert: j ist der aktuelle millis()-Zeitstempel in Millisekunden.
    // Ausgabewert: true bedeutet, AQI/TVOC/eCO2 sollen auf ungueltig gesetzt werden.
    bool ensStale(unsigned long j) {
        if (!ens_ok || !ens160) return true;
        return letzter_ens_gueltig_ms > 0 && (j - letzter_ens_gueltig_ms) > NET_ERL_ENS160_STALE_TIMEOUT_MS;
    }
}

// =============================================================================
// CUSTOM HOOKS (von NetErlRuntime.h aufgerufen)
// =============================================================================

// Aufgabe: Initialisiert I2C-Bus, alle Sensoren und den LD2410-Eingang.
// Eingabewerte: keine; Pins, Adressen und I2C-Takt kommen aus DeviceConfig.h und PinConfig.h.
// Ausgabewert: keiner; lokale OK-Flags zeigen danach die verfuegbaren Sensoren.
// Aufrufer: NetErlRuntime ruft diesen Hook einmal beim Boot auf.
void netErlDeviceInit() {
    Wire.begin(PIN_SENSOR_SDA, PIN_SENSOR_SCL);
    Wire.setClock(NET_ERL_I2C_CLOCK_HZ);
    Wire.setTimeOut(I2C_TIMEOUT_MS);

    bme_ok = initBme();
    if (!bme_ok) logMsg("WARN", "BME680 init fail");

    if (!veml.begin()) { lux_ok = false; logMsg("WARN", "VEML7700 init fail"); }
    else { lux_ok = true; konfVeml(); }

    ens_ok = initEns();
    if (!ens_ok) { logMsg("WARN", "ENS160 init fail"); }
    else if (!ens160->setMode(ENS160_OPMODE_STD)) { ens_ok = false; logMsg("WARN", "ENS160 mode fail"); }

    pinMode(PIN_LD2410_OUT, INPUT);
}

// Aufgabe: Setzt alle Sensorwerte auf ungueltige Startwerte zurueck.
// Eingabewerte: keine.
// Ausgabewert: keiner; INT16_MIN, 0xFFFFU und 0xFFFFFFFFUL markieren ungueltige Werte.
void netErlDeviceResetSensorDefaults() {
    temp_01c = INT16_MIN; hum_01pct = 0xFFFFU; lux = 0xFFFFU;
    pressure_pa = PRESSURE_UNGUELTIG; gas_ohm = GAS_OHM_UNGUELTIG;
    aqi = AIR_METRIC_UNGUELTIG; tvoc_ppb = AIR_METRIC_UNGUELTIG; eco2_ppm = AIR_METRIC_UNGUELTIG;
}

// Aufgabe: Liest den LD2410-Radar-Praesenzstatus vom digitalen Pin.
// Eingabewerte: keine; PIN_LD2410_OUT kommt aus PinConfig.h.
// Ausgabewert: true bedeutet, der Radar-Ausgang steht auf HIGH und Praesenz wurde erkannt.
bool netErlDeviceReadPresence() {
    ld2410_raw = (digitalRead(PIN_LD2410_OUT) == HIGH);
    return ld2410_raw;
}

// Aufgabe: Schaltet den Relaisausgang.
// Eingabewert: on=true bedeutet Relais soll aktiv sein.
// Ausgabewert: keiner; RELAY_1_ACTIVE_HIGH legt fest, ob HIGH oder LOW aktiv bedeutet.
void netErlDeviceSetRelayOutput(bool on) {
    digitalWrite(PIN_RELAY_1, on == RELAY_1_ACTIVE_HIGH ? HIGH : LOW);
}

// Aufgabe: Aktualisiert den NeoPixel-LED-Ring passend zum Relaiszustand.
// Eingabewert: relayOn=true bedeutet, der Ring wird weiss eingeschaltet.
// Ausgabewert: keiner; Adafruit_NeoPixel schreibt die LED-Daten an PIN_LED_RING.
// Beim ersten Aufruf wird der Ring initialisiert, damit der Basistyp keine LED-Details kennen muss.
void netErlDeviceUpdateIndicators(bool relayOn) {
    if (!ring_initialized) {
        ledRing.begin(); ledRing.setBrightness(LED_RING_HELLIGKEIT);
        ledRing.clear(); ledRing.show(); ring_initialized = true;
    }
    if (relayOn) {
        uint32_t c = ledRing.Color(24, 24, 24);
        for (uint16_t i = 0; i < ledRing.numPixels(); ++i) ledRing.setPixelColor(i, c);
    } else {
        ledRing.clear();
    }
    ledRing.show();
}

// Aufgabe: Pollt alle Sensoren, aktualisiert Messwerte, Recovery, ENS160-Kompensation und Auto-Light.
// Eingabewert: nowMs ist der aktuelle millis()-Zeitstempel in Millisekunden.
// Ausgabewert: keiner; Messwerte, runtime.fault und runtime.state_report_offen werden aktualisiert.
//
// Ablauf:
// 1. NET_ERL_ENV_SAMPLE_INTERVAL_MS begrenzt die Messrate in Millisekunden.
// 2. Recovery initialisiert ausgefallene Sensoren nach dem Retry-Intervall erneut.
// 3. BME680 liefert Temperatur, Feuchte, Druck und Gaswiderstand; Gas wird erst nach Warmup gemeldet.
// 4. VEML7700 liefert Lux fuer Light-Guard und Auto-On-Entscheidungen.
// 5. BME680-Temperatur und -Feuchte werden als ENS160-Kompensation geschrieben.
// 6. ENS160 liefert AQI, TVOC und eCO2; veraltete Werte werden ungueltig gesetzt.
// 7. Late-Lux entscheidet wartendes Auto-On erst bei gueltigem Lux-Wert.
// 8. Delta-Detection spart Funkverkehr, indem STATE nur bei relevanten Aenderungen gesendet wird.
//
// Master-Kommandos duerfen Auto-Light spaeter uebersteuern. Die urspruengliche
// Auto-On-Entscheidung bleibt trotzdem sauber getrennt.
void netErlDevicePollSensors(unsigned long nowMs) {
    if ((nowMs - letztes_env_sample_ms) < NET_ERL_ENV_SAMPLE_INTERVAL_MS) return;
    letztes_env_sample_ms = nowMs;

    // Recovery: ausgefallene Sensoren erst nach dem Retry-Intervall neu initialisieren.
    if (!bme_ok && recoveryIsDue(letzter_bme_recovery_ms, nowMs, SENSOR_RECOVERY_RETRY_INTERVAL_MS)) {
        letzter_bme_recovery_ms = nowMs; bme_ok = initBme();
    }
    if (!lux_ok && recoveryIsDue(letzter_lux_recovery_ms, nowMs, SENSOR_RECOVERY_RETRY_INTERVAL_MS)) {
        letzter_lux_recovery_ms = nowMs; lux_ok = veml.begin();
        if (lux_ok) konfVeml();
    }
    if (!ens_ok && recoveryIsDue(letzter_ens_recovery_ms, nowMs, SENSOR_RECOVERY_RETRY_INTERVAL_MS)) {
        letzter_ens_recovery_ms = nowMs; ens_ok = initEns();
        if (ens_ok) ens160->setMode(ENS160_OPMODE_STD);
    }

    // BME680 lesen: Temperatur, Feuchte und Druck werden plausibilisiert.
    if (bme_ok && bme680.performReading()) {
        float t = bme680.temperature, h = bme680.humidity, p = bme680.pressure;
        uint32_t g = bme680.gas_resistance;
        if (isfinite(t) && isfinite(h) && isfinite(p) && h >= 0 && h <= 100 && p >= 30000 && p <= 110000) {
            temp_01c = (int16_t)lroundf(t * 10.0f);
            hum_01pct = clampHum01pct((long)lroundf(h * 10.0f));
            pressure_pa = (uint32_t)lroundf(p);
            if (bme680_gueltige_messungen < 255) bme680_gueltige_messungen++;
            gas_ohm = (gasWarmupOk(nowMs) && g > 0) ? g : GAS_OHM_UNGUELTIG;
        } else { bme_ok = false; logMsg("WARN", "BME680 unplausibel"); }
    }

    // VEML7700 lesen: Lux wird auf uint16_t begrenzt, damit der Protokoll-Payload passt.
    if (lux_ok) {
        float l = veml.readLux();
        if (!isnan(l) && l >= 0) lux = clampToU16((long)lroundf(l));
        else { lux_ok = false; logMsg("WARN", "VEML7700 read fail"); }
    }

    // ENS160-Kompensation schreiben: BME680-Temperatur und -Feuchte verbessern die Luftwerte.
    if (ens_ok && ens160 && bme_ok) {
        int r = writeEnsEnv(ens160_adresse, bme680.temperature, bme680.humidity);
        if (r != 0) logMsg("WARN", "ENS160 comp fail err=%d", r);
    }

    // ENS160-Messwerte lesen: AQI500 bevorzugt, AQI 1-5 als Fallback auf 0-500 skaliert.
    if (ens_ok && ens160 && ens160->measure(false)) {
        uint16_t aq5 = ens160->getAQI500(), aq = ens160->getAQI();
        uint16_t maq = (aq5 > 0 && aq5 <= 500) ? aq5 : mapAqi500(aq);
        if (maq > 0) {
            aqi = maq; tvoc_ppb = ens160->getTVOC();
            eco2_ppm = ens160->geteCO2(); letzter_ens_gueltig_ms = nowMs;
        }
    }

    // Stale-Detection: alte ENS160-Werte nicht weiter als gueltig melden.
    if (ensStale(nowMs)) { aqi = AIR_METRIC_UNGUELTIG; tvoc_ppb = AIR_METRIC_UNGUELTIG; eco2_ppm = AIR_METRIC_UNGUELTIG; }

    // Status aktualisieren: fault=true sobald ein benoetigter Sensor nicht verfuegbar ist.
    runtime.fault = !(bme_ok && lux_ok) || !ens_ok;

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
    // Spart ESP-NOW/MQTT-Bandbreite bei gleichbleibenden Messwerten
    {
        static int16_t  last_temp = INT16_MIN;
        static uint16_t last_hum = 0xFFFFU, last_lux = 0xFFFFU;
        static uint32_t last_press = 0xFFFFFFFFUL;
        static uint16_t last_aqi = 0xFFFFU;
        
        bool changed = false;
        if (temp_01c != last_temp) { last_temp = temp_01c; changed = true; }
        if (absDiffU16(hum_01pct, last_hum) >= 5U) { last_hum = hum_01pct; changed = true; }
        if (absDiffU16(lux, last_lux) >= 5U) { last_lux = lux; changed = true; }
        if (absDiffU32(pressure_pa, last_press) >= 10UL) { last_press = pressure_pa; changed = true; }
        if (last_aqi != aqi) { last_aqi = aqi; changed = true; }
        
        if (changed) runtime.state_report_offen = true;
    }
}

// Aufgabe: Befuellt den ExtendedRelayComfortGasConfigStateReportPayload mit aktuellen Messwerten.
// Eingabewerte:
// - payload zeigt auf den vom Basistyp bereitgestellten Payload-Speicher.
// - size zeigt auf die verfuegbare Groesse und wird danach auf die benoetigte Groesse gesetzt.
// Ausgabewert: keiner; der Payload enthaelt Relais, Sensorwerte, Auto-Flags und Fehlerstatus.
void netErlDeviceFillStatePayload(void* payload, size_t* size) {
    SmartHome::ExtendedRelayComfortGasConfigStateReportPayload* p =
        static_cast<SmartHome::ExtendedRelayComfortGasConfigStateReportPayload*>(payload);
    if (p != nullptr && size && *size >= sizeof(*p)) {
        safeStrCopy(p->node_id, sizeof(p->node_id), DEVICE_ID);
        p->relay_1 = runtime.relay_1 ? 1U : 0U;
        p->temp_01c = temp_01c;
        p->hum_01pct = hum_01pct;
        p->lux = lux;
        p->pressure_pa = pressure_pa;
        p->gas_ohm = gas_ohm;
        p->aqi = aqi;
        p->tvoc_ppb = tvoc_ppb;
        p->eco2_ppm = eco2_ppm;
        p->motion = runtime.motion_aktiv ? 1U : 0U;
        p->auto_flags = netErlDeviceBuildAutoFlags();
        p->fault = runtime.fault ? 1U : 0U;
        p->report_interval_s = (uint16_t)runtime.report_interval_s;
        p->auto_on_lux_threshold = runtime.auto_on_lux_threshold;
    }
    if (size != nullptr) *size = sizeof(SmartHome::ExtendedRelayComfortGasConfigStateReportPayload);
}

// Aufgabe: Baut die Auto-Light-Flags fuer den STATE-Report.
// Eingabewerte: keine; Status kommt aus runtime und lokalen Sensorflags.
// Ausgabewert: Bitmaske aus SH_RELAY_COMFORT_FLAG_*-Werten.
uint8_t netErlDeviceBuildAutoFlags() {
    uint8_t f = 0;
    if (runtime.motion_aktiv) f |= SH_RELAY_COMFORT_FLAG_PRESENCE_SOURCE_AVAILABLE;
    if (lux_ok) f |= SH_RELAY_COMFORT_FLAG_LIGHT_VALUE_AVAILABLE;
    f |= SH_RELAY_COMFORT_FLAG_LIGHT_GUARD_ENABLED;
    if (runtime.relay_auto_owned) f |= SH_RELAY_COMFORT_FLAG_AUTO_RELAY_OWNED;
    if (runtime.blocked_by_lux) f |= SH_RELAY_COMFORT_FLAG_BLOCKED_BY_LUX;
    return f;
}

// Aufgabe: Meldet dem Basistyp, ob ein Sensorfehler vorliegt.
// Eingabewerte: keine; lokale OK-Flags werden ausgewertet.
// Ausgabewert: true bedeutet, BME680, VEML7700 oder ENS160 ist nicht verfuegbar.
bool netErlDeviceHasSensorFault() {
    return !(bme_ok && lux_ok) || !ens_ok;
}

// Aufgabe: Schreibt einen kompakten Snapshot der aktuellen Sensor- und Relaiswerte ins Log.
// Eingabewerte: keine.
// Ausgabewert: keiner; logMsg stammt aus dem NET-ERL-Basistyp.
void netErlDeviceLogSnapshot() {
    logMsg("INFO", "snap t=%d h=%u l=%u p=%lu g=%lu a=%u tv=%u ec=%u m=%s r=%s",
        (int)temp_01c, hum_01pct, lux,
        (unsigned long)pressure_pa, (unsigned long)gas_ohm,
        aqi, tvoc_ppb, eco2_ppm,
        runtime.motion_aktiv ? "1" : "0", runtime.relay_1 ? "1" : "0");
}

// Aufgabe: Liest den lokalen Hardware-Button.
// Eingabewerte: keine; PIN_BUTTON_1 und BUTTON_1_ACTIVE_LOW kommen aus PinConfig.h.
// Ausgabewert: true bedeutet, der Button ist gedrueckt.
bool netErlDeviceReadButton() {
#if BUTTON_1_ACTIVE_LOW
    return digitalRead(PIN_BUTTON_1) == LOW;
#else
    return digitalRead(PIN_BUTTON_1) == HIGH;
#endif
}
