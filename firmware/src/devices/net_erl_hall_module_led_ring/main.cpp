/*
===============================================================================
 Datei: main.cpp
 Code-Name: NET-ERL Hall Module LED Ring
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / Device-Code / Netzbetriebener Relais-Komfortaktor
 Ersteller: DevOpsOfChaos
 Datum: 2026-05-14
 Letzte Bearbeitung: 2026-05-19

 Zweck: Flurmodul mit Radar-Praesenz, Luftqualitaet, Luxmessung, Relais und LED-Ring
 Beschreibung: Dieser Device-Adapter ist die umfangreichste NET-ERL-Variante.
 Er verbindet den NET-ERL-Basistyp mit LD2410-Radar, BME680, VEML7700, ENS160
 und NeoPixel-LED-Ring. Der BME680 liefert Temperatur, Feuchte, Luftdruck und
 Gaswiderstand. Der ENS160 nutzt Temperatur und Feuchte als Kompensation und
 liefert AQI, TVOC und eCO2. Der VEML7700 liefert Lux fuer die Auto-Light-
 Entscheidung. Der LED-Ring zeigt als Bestandteil der Technikerarbeit die
 Luftqualitaet an. Weitere Ringanzeigen fuer Temperatur, Feuchte und lokale
 Hinweise sind reine Komfort-Erweiterungen und getrennt kommentiert.

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
 - 2026-05-19: Sensor-Ausfallpfade, ENS160-Warmup und Auto-Flags bereinigt.
 - 2026-05-21: LED-Ring-Anzeige fuer AQI ergaenzt; weitere Ring-Komfortphasen abgegrenzt.
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
    uint8_t ens160_adresse = 0;
    uint8_t bme680_gueltige_messungen = 0;
    unsigned long letzter_bme_recovery_ms = 0, letzter_lux_recovery_ms = 0, letzter_ens_recovery_ms = 0;
    unsigned long letztes_env_sample_ms = 0, ens160_start_ms = 0, letzter_ens_gueltig_ms = 0;
    bool ring_initialized = false;
    bool letzter_blocked_by_lux = false;
    unsigned long ring_sequence_started_ms = 0, ring_display_until_ms = 0;
    unsigned long ring_lux_blocked_until_ms = 0, letzter_ring_frame_ms = 0;

    // Sensor-Messwerte
    int16_t temp_01c = INT16_MIN;
    uint16_t hum_01pct = 0xFFFFU, lux = 0xFFFFU;
    uint32_t pressure_pa = 0xFFFFFFFFUL, gas_ohm = 0xFFFFFFFFUL;
    uint16_t aqi = 0xFFFFU, tvoc_ppb = 0xFFFFU, eco2_ppm = 0xFFFFU;

    constexpr uint32_t PRESSURE_UNGUELTIG = 0xFFFFFFFFUL;
    constexpr uint32_t GAS_OHM_UNGUELTIG = 0xFFFFFFFFUL;
    constexpr uint16_t AIR_METRIC_UNGUELTIG = 0xFFFFU;
    constexpr uint16_t ENS160_AQI_MAX_BASIC = 5U;
    constexpr uint16_t I2C_TIMEOUT_MS = 50U;     // 50 Millisekunden I2C-Timeout.
}

// =============================================================================
// SENSOR-HILFSFUNKTIONEN (device-spezifisch)
// =============================================================================
namespace {
    // Teil der Technikerarbeit:
    // Hilfsfunktionen fuer die lokale LED-Ring-Luftqualitaetsanzeige. Sie zeigen
    // den ENS160-AQI am Geraet, ohne ESP-NOW-Payloads oder MQTT-Serververtrag zu
    // erweitern.
    uint8_t ringScale(uint8_t value) {
        constexpr uint8_t brightness =
            NET_ERL_LED_RING_BRIGHTNESS > NET_ERL_LED_RING_MAX_CFG_BRIGHTNESS
                ? NET_ERL_LED_RING_MAX_CFG_BRIGHTNESS
                : NET_ERL_LED_RING_BRIGHTNESS;
        return (uint8_t)(((uint16_t)value * brightness) / 255U);
    }

    uint32_t ringColor(uint8_t red, uint8_t green, uint8_t blue) {
        return ledRing.Color(ringScale(red), ringScale(green), ringScale(blue));
    }

    void ringSetAll(uint32_t color) {
        for (uint16_t i = 0; i < ledRing.numPixels(); ++i) {
            ledRing.setPixelColor(i, color);
        }
    }

    uint32_t ringAqiColor() {
        if (aqi == AIR_METRIC_UNGUELTIG) return ringColor(90, 90, 90);
        if (aqi <= 50U) return ringColor(0, 210, 80);
        if (aqi <= 100U) return ringColor(180, 190, 0);
        if (aqi <= 150U) return ringColor(255, 115, 0);
        if (aqi <= 200U) return ringColor(220, 0, 0);
        if (aqi <= 300U) return ringColor(130, 0, 170);
        return ringColor(90, 0, 40);
    }

    uint32_t ringDimColor(uint32_t color, float factor) {
        uint8_t red = (uint8_t)((color >> 16) & 0xFFU);
        uint8_t green = (uint8_t)((color >> 8) & 0xFFU);
        uint8_t blue = (uint8_t)(color & 0xFFU);
        return ledRing.Color(
            (uint8_t)((float)red * factor),
            (uint8_t)((float)green * factor),
            (uint8_t)((float)blue * factor)
        );
    }

    void ensureRingInitialized() {
        if (ring_initialized) return;
        ledRing.begin();
        ledRing.setBrightness(255);
        ledRing.clear();
        ledRing.show();
        ring_initialized = true;
    }

    void startRingComfortSequence(unsigned long nowMs) {
        if (ring_display_until_ms == 0 || (long)(ring_display_until_ms - nowMs) <= 0) {
            ring_sequence_started_ms = nowMs;
        }
        // Teil der Technikerarbeit ist die AQI-Phase. Die anschliessenden
        // Temperatur-/Feuchtephasen sind Komfort-Erweiterungen und bleiben
        // ausserhalb der bewerteten Projektfunktion.
        ring_display_until_ms = nowMs
            + NET_ERL_LED_RING_AQI_PHASE_MS
            + NET_ERL_LED_RING_TEMP_PHASE_MS
            + NET_ERL_LED_RING_HUM_PHASE_MS;
    }

    void updateRingComfortDisplay(unsigned long nowMs) {
        ensureRingInitialized();
        if ((nowMs - letzter_ring_frame_ms) < NET_ERL_LED_RING_FRAME_INTERVAL_MS) return;
        letzter_ring_frame_ms = nowMs;

        if (runtime.blocked_by_lux && !letzter_blocked_by_lux) {
            ring_lux_blocked_until_ms = nowMs + NET_ERL_LED_RING_LUX_BLOCKED_ALERT_MS;
        }
        letzter_blocked_by_lux = runtime.blocked_by_lux;

        // Nicht Bestandteil der Technikerarbeit:
        // Rote Daueranzeige bei Sensorfehlern ist eine lokale Komfort-/Diagnosehilfe.
        if (netErlDeviceHasSensorFault()) {
            ringSetAll(ringColor(255, 0, 0));
            ledRing.show();
            return;
        }

        // Nicht Bestandteil der Technikerarbeit:
        // Kurze Lux-Blockade-Animation. Die Auto-Light-Entscheidung selbst bleibt
        // unveraendert im NET-ERL-Basistyp und im State-Payload.
        if ((long)(ring_lux_blocked_until_ms - nowMs) > 0) {
            ledRing.clear();
            const uint16_t count = ledRing.numPixels();
            const uint16_t head = count > 0 ? (uint16_t)((nowMs / NET_ERL_LED_RING_FRAME_INTERVAL_MS) % count) : 0;
            for (uint16_t offset = 0; offset < 4U && offset < count; ++offset) {
                const uint16_t pos = (uint16_t)((head + count - offset) % count);
                ledRing.setPixelColor(pos, ringColor((uint8_t)(255U - offset * 45U), (uint8_t)(140U - offset * 25U), 0));
            }
            ledRing.show();
            return;
        }

        if (ring_display_until_ms == 0 || (long)(ring_display_until_ms - nowMs) <= 0) {
            ledRing.clear();
            ledRing.show();
            return;
        }

        const unsigned long totalPhaseMs =
            NET_ERL_LED_RING_AQI_PHASE_MS + NET_ERL_LED_RING_TEMP_PHASE_MS + NET_ERL_LED_RING_HUM_PHASE_MS;
        const unsigned long phaseMs = totalPhaseMs > 0
            ? (nowMs - ring_sequence_started_ms) % totalPhaseMs
            : 0UL;
        const uint16_t count = ledRing.numPixels();

        // Teil der Technikerarbeit:
        // Die erste Phase visualisiert ausschliesslich die Luftqualitaet per AQI.
        if (phaseMs < NET_ERL_LED_RING_AQI_PHASE_MS) {
            const float pulse = 0.55f + 0.45f * ((sinf((float)nowMs / 360.0f) + 1.0f) / 2.0f);
            ringSetAll(ringDimColor(ringAqiColor(), pulse));
            ledRing.show();
            return;
        }

        // Nicht Bestandteil der Technikerarbeit:
        // Temperaturanzeige ist eine lokale Zusatzanimation.
        if (phaseMs < (NET_ERL_LED_RING_AQI_PHASE_MS + NET_ERL_LED_RING_TEMP_PHASE_MS)) {
            ledRing.clear();
            int16_t t = temp_01c == INT16_MIN ? 0 : temp_01c;
            int active = ((int)t * (int)count) / 400;
            if (active < 1 && t > 0) active = 1;
            if (active > (int)count) active = count;
            const uint16_t sweep = count > 0 ? (uint16_t)((nowMs / 180UL) % count) : 0;
            for (int i = 0; i < active; ++i) {
                const bool highlight = (uint16_t)i == sweep || (uint16_t)i == (uint16_t)((sweep + count - 1U) % count);
                ledRing.setPixelColor((uint16_t)i, ringColor(highlight ? 255 : 150, highlight ? 55 : 12, 0));
            }
            ledRing.show();
            return;
        }

        // Nicht Bestandteil der Technikerarbeit:
        // Feuchteanzeige ist eine lokale Zusatzanimation.
        ledRing.clear();
        uint16_t h = hum_01pct == 0xFFFFU ? 0U : hum_01pct;
        int active = ((int)h * (int)count) / 1000;
        if (active < 1 && h > 0U) active = 1;
        if (active > (int)count) active = count;
        const uint16_t wave = count > 0 ? (uint16_t)((nowMs / 220UL) % count) : 0;
        for (int i = 0; i < active; ++i) {
            const uint16_t pos = (uint16_t)i;
            const uint16_t distance = pos > wave ? (uint16_t)(pos - wave) : (uint16_t)(wave - pos);
            const bool crest = distance <= 1U || distance >= (count > 0 ? count - 1U : 0U);
            ledRing.setPixelColor(pos, ringColor(0, crest ? 120 : 35, crest ? 255 : 170));
        }
        ledRing.show();
    }
    // Ende LED-Ring-Anzeige.

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
            bme680.setGasHeater(320U, 150U); return true;
        }
        return false;
    }

    void resetBmeValues() {
        temp_01c = INT16_MIN;
        hum_01pct = 0xFFFFU;
        pressure_pa = PRESSURE_UNGUELTIG;
        gas_ohm = GAS_OHM_UNGUELTIG;
        bme680_gueltige_messungen = 0;
    }

    void resetEnsValues() {
        aqi = AIR_METRIC_UNGUELTIG;
        tvoc_ppb = AIR_METRIC_UNGUELTIG;
        eco2_ppm = AIR_METRIC_UNGUELTIG;
    }

    bool useEnsAddress(ScioSense_ENS160& sensor, uint8_t address) {
        ens160 = &sensor;
        if (!ens160->begin()) return false;
        ens160_adresse = address;
        ens160_start_ms = millis();
        letzter_ens_gueltig_ms = 0;
        resetEnsValues();
        return true;
    }

    // Aufgabe: Initialisiert den ENS160 ueber primaere oder Fallback-I2C-Adresse.
    // Eingabewerte: keine; die Adressen kommen aus DeviceConfig.h.
    // Ausgabewert: true bedeutet, ein ENS160 wurde gefunden.
    bool initEns() {
        if (useEnsAddress(ens160Addr52, NET_ERL_ENS160_PRIMARY_ADDRESS)) return true;
        if (useEnsAddress(ens160Addr53, NET_ERL_ENS160_FALLBACK_ADDRESS)) return true;
        ens160 = nullptr;
        ens160_adresse = 0;
        ens160_start_ms = 0;
        resetEnsValues();
        return false;
    }

    bool setEnsStandardMode() {
        if (!ens160 || !ens160->setMode(ENS160_OPMODE_STD)) {
            ens_ok = false;
            resetEnsValues();
            return false;
        }
        return true;
    }

    // Aufgabe: Prueft, ob der BME680-Gassensor ausreichend warmgelaufen ist.
    // Eingabewert: j ist der aktuelle millis()-Zeitstempel in Millisekunden.
    // Ausgabewert: true bedeutet, Gaswerte duerfen als gueltig gemeldet werden.
    bool gasWarmupOk(unsigned long j) {
        return gasWarmupComplete(runtime.boot_ms, j, NET_ERL_BME680_GAS_WARMUP_MS, bme680_gueltige_messungen, NET_ERL_BME680_GAS_WARMUP_MIN_READS);
    }

    bool ensWarmupOk(unsigned long j) {
        return ens160_start_ms > 0 && (j - ens160_start_ms) >= NET_ERL_ENS160_WARMUP_MS;
    }

    // Aufgabe: Prueft, ob die letzten ENS160-Messwerte veraltet sind.
    // Eingabewert: j ist der aktuelle millis()-Zeitstempel in Millisekunden.
    // Ausgabewert: true bedeutet, AQI/TVOC/eCO2 sollen auf ungueltig gesetzt werden.
    bool ensStale(unsigned long j) {
        if (!ens_ok || !ens160) return true;
        if (!ensWarmupOk(j)) return true;
        if (letzter_ens_gueltig_ms == 0) return true;
        return letzter_ens_gueltig_ms > 0 && (j - letzter_ens_gueltig_ms) > NET_ERL_ENS160_STALE_TIMEOUT_MS;
    }

    bool ensFault(unsigned long j) {
        return !ens_ok || !ens160 || (ensWarmupOk(j) && letzter_ens_gueltig_ms == 0);
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
    else if (!setEnsStandardMode()) { logMsg("WARN", "ENS160 mode fail"); }

    pinMode(PIN_LD2410_OUT, INPUT);
    ensureRingInitialized();
}

// Aufgabe: Setzt alle Sensorwerte auf ungueltige Startwerte zurueck.
// Eingabewerte: keine.
// Ausgabewert: keiner; INT16_MIN, 0xFFFFU und 0xFFFFFFFFUL markieren ungueltige Werte.
void netErlDeviceResetSensorDefaults() {
    resetBmeValues();
    lux = 0xFFFFU;
    resetEnsValues();
}

// Aufgabe: Liest den LD2410-Radar-Praesenzstatus vom digitalen Pin.
// Eingabewerte: keine; PIN_LD2410_OUT kommt aus PinConfig.h.
// Ausgabewert: true bedeutet, der Radar-Ausgang steht auf HIGH und Praesenz wurde erkannt.
bool netErlDeviceReadPresence() {
    const bool presence = digitalRead(PIN_LD2410_OUT) == HIGH;
    const unsigned long nowMs = millis();
    // Teil der Technikerarbeit ist nur der Start der AQI-Anzeige bei Praesenz.
    // Die zusaetzlichen Ringphasen bleiben lokale Komfort-Erweiterungen.
    if (presence) {
        startRingComfortSequence(nowMs);
    }
    updateRingComfortDisplay(nowMs);
    return presence;
}

// Aufgabe: Schaltet den Relaisausgang.
// Eingabewert: on=true bedeutet Relais soll aktiv sein.
// Ausgabewert: keiner; RELAY_1_ACTIVE_HIGH legt fest, ob HIGH oder LOW aktiv bedeutet.
void netErlDeviceSetRelayOutput(bool on) {
    digitalWrite(PIN_RELAY_1, on == RELAY_1_ACTIVE_HIGH ? HIGH : LOW);
}

// Aktualisiert die lokale LED-Ring-Anzeige. Teil der Technikerarbeit ist nur die
// AQI-/Luftqualitaetsanzeige; Temperatur, Feuchte und Fehlerhinweise sind als
// Komfort-Erweiterungen im Anzeigehelfer abgegrenzt. Der Serververtrag und die
// ESP-NOW-Payloads bleiben davon unberuehrt.
void netErlDeviceUpdateIndicators(bool relayOn) {
    (void)relayOn;
    updateRingComfortDisplay(millis());
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
        if (ens_ok && !setEnsStandardMode()) logMsg("WARN", "ENS160 mode fail");
    }

    // BME680 lesen: Temperatur, Feuchte und Druck werden plausibilisiert.
    bool bme_read_valid = false;
    if (bme_ok) {
        if (bme680.performReading()) {
            float t = bme680.temperature, h = bme680.humidity, p = bme680.pressure;
            uint32_t g = bme680.gas_resistance;
            if (isfinite(t) && isfinite(h) && isfinite(p) && h >= 0 && h <= 100 && p >= 30000 && p <= 110000) {
                temp_01c = (int16_t)lroundf(t * 10.0f);
                hum_01pct = clampHum01pct((long)lroundf(h * 10.0f));
                pressure_pa = (uint32_t)lroundf(p);
                if (bme680_gueltige_messungen < 255) bme680_gueltige_messungen++;
                gas_ohm = (gasWarmupOk(nowMs) && g > 0) ? g : GAS_OHM_UNGUELTIG;
                bme_read_valid = true;
            } else {
                bme_ok = false;
                resetBmeValues();
                logMsg("WARN", "BME680 unplausibel");
            }
        } else {
            bme_ok = false;
            resetBmeValues();
            logMsg("WARN", "BME680 read fail");
        }
    }

    // VEML7700 lesen: Lux wird auf uint16_t begrenzt, damit der Protokoll-Payload passt.
    if (lux_ok) {
        float l = veml.readLux();
        if (!isnan(l) && l >= 0) lux = clampToU16((long)lroundf(l));
        else { lux_ok = false; lux = 0xFFFFU; logMsg("WARN", "VEML7700 read fail"); }
    }

    // ENS160-Kompensation schreiben: BME680-Temperatur und -Feuchte verbessern die Luftwerte.
    if (ens_ok && ens160 && bme_read_valid) {
        int r = writeEnsEnv(ens160_adresse, bme680.temperature, bme680.humidity);
        if (r != 0) { ens_ok = false; resetEnsValues(); logMsg("WARN", "ENS160 comp fail err=%d", r); }
    }

    // ENS160-Messwerte lesen: AQI500 bevorzugt, AQI 1-5 als Fallback auf 0-500 skaliert.
    if (ens_ok && ens160 && ens160->measure(false)) {
        uint16_t aq5 = ens160->getAQI500(), aq = ens160->getAQI();
        uint16_t maq = (aq5 > 0 && aq5 <= 500) ? aq5 : mapAqi500(aq);
        if (maq > 0 && ensWarmupOk(nowMs)) {
            aqi = maq; tvoc_ppb = ens160->getTVOC();
            eco2_ppm = ens160->geteCO2(); letzter_ens_gueltig_ms = nowMs;
        }
    }

    // Stale-Detection: alte ENS160-Werte nicht weiter als gueltig melden.
    if (ensStale(nowMs)) resetEnsValues();

    // Status aktualisieren: fault=true sobald ein benoetigter Sensor nicht verfuegbar ist.
    runtime.fault = !(bme_ok && lux_ok) || ensFault(nowMs);

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

    // Aktualisiert die lokale LED-Ring-Anzeige nach Sensorpoll und Late-Lux.
    // Nur die AQI-Anzeige zaehlt zur Technikerarbeit; Zusatzanimationen bleiben
    // ausserhalb. State-Payload und Auto-Light-Logik bleiben getrennt.
    updateRingComfortDisplay(nowMs);

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
    if (runtime.pending_auto_on_decision && lux == 0xFFFFU) f |= SH_RELAY_COMFORT_FLAG_BLOCKED_BY_MISSING_LUX;
    return f;
}

// Aufgabe: Liefert den zuletzt gemessenen Luxwert fuer eine sofortige Auto-On-Entscheidung.
// Eingabewert: luxOut zeigt auf den Ausgabespeicher fuer den Luxwert.
// Ausgabewert: true bedeutet, luxOut enthaelt einen gueltigen Luxwert.
//
// Hintergrund: Der LD2410 wird mit 50 Millisekunden deutlich schneller gepollt
// als die Umweltsensoren. Beim ersten Motion-HIGH soll die Lampe deshalb nicht
// bis zum naechsten 2500-Millisekunden-Sensorpoll warten, sondern direkt den
// letzten bekannten VEML7700-Wert verwenden.
bool netErlDeviceGetCachedLux(uint16_t* luxOut) {
    if (luxOut == nullptr || !lux_ok || lux == 0xFFFFU) {
        return false;
    }
    *luxOut = lux;
    return true;
}

// Aufgabe: Meldet dem Basistyp, ob ein Sensorfehler vorliegt.
// Eingabewerte: keine; lokale OK-Flags werden ausgewertet.
// Ausgabewert: true bedeutet, BME680, VEML7700 oder ENS160 ist nicht verfuegbar.
bool netErlDeviceHasSensorFault() {
    return !(bme_ok && lux_ok) || ensFault(millis());
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
