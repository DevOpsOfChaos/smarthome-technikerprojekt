/*
===============================================================================
 Datei: main.cpp
 Code-Name: NET-SEN Weather Station
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / Device-Code / Netzbetriebener Sensor
 Ersteller: DevOpsOfChaos
 Datum: 2026-05-14
 Letzte Bearbeitung: 2026-05-18

 Zweck: Wetter-/Umweltsensor mit BME280, VEML7700 und digitalem Regensignal
 Beschreibung: Dieser Device-Adapter erweitert den NET-SEN-Basistyp um BME280-
 Messwerte fuer Temperatur, Feuchte und Luftdruck, VEML7700-Luxmessung und ein
 digitales Regen-Event. Sensorfehler werden gedrosselt geloggt und nach einem
 festen Intervall automatisch erneut initialisiert. 30000UL bedeutet hier
 30000 Millisekunden, also 30 Sekunden Recovery-Abstand. 1050UL bedeutet
 1050 Millisekunden, also 1,05 Sekunden Warmup vor dem ersten VEML7700-Luxwert.

 Hardware:
 - ESP32-C3
 - BME280 fuer Temperatur, relative Feuchte und Luftdruck
 - VEML7700 fuer Beleuchtungsstaerke in Lux
 - Digitaler Regensensor an GPIO3, active-LOW mit Pullup

 Genutzte Bibliotheken:
 - Arduino.h: Arduino-Funktionen wie pinMode, digitalRead, millis, LOW und HIGH.
 - Wire.h: I2C-Bus fuer BME280 und VEML7700.
 - math.h: isfinite und lroundf fuer Plausibilitaet und Rundung.
 - Adafruit_BME280.h: Fremdbibliothek fuer BME280-Messwerte.
 - Adafruit_VEML7700.h: Fremdbibliothek fuer VEML7700-Luxmessung.
 - DeviceConfig.h: eigene Device-Konfiguration mit Adressen, Intervallen und Deltas.
 - PinConfig.h: eigene Pin-Zuordnung fuer diesen konkreten Node.
 - MathUtils.h: eigene Hilfsbibliothek fuer Begrenzung und Delta-Erkennung.
 - SensorUtils.h: eigene Sensor-Hilfsbibliothek, hier fuer recoveryIsDue.
 - NetSenRuntime.h: eigener NET-SEN-Basistyp; liefert setup(), loop(), Funklogik
   und ruft die Sensor-, Extended-State- und Event-Hooks aus dieser Datei auf.

 Aenderungsverlauf:
 - 2026-05-14: Device-Code fuer NET-SEN Weather Station angelegt.
 - 2026-05-18: Kommentarstil vereinheitlicht und Doxygen-Metakommentare entfernt.
===============================================================================
*/

// Arduino.h: GPIO, millis(), HIGH/LOW und Arduino-Grundtypen.
#include <Arduino.h>
// Wire.h: I2C-Bus fuer BME280 und VEML7700.
#include <Wire.h>
// math.h: isfinite(), isnan() und lroundf() fuer Messwertvalidierung.
#include <math.h>

// Adafruit-Sensorbibliotheken: Die Klassen kapseln die I2C-Registerzugriffe.
#include <Adafruit_BME280.h>
#include <Adafruit_VEML7700.h>

// Eigene Konfigurations- und Utility-Dateien. MathUtils/SensorUtils kommen aus
// dem Projekt und werden unten per using in den lokalen Namensraum geholt.
#include "DeviceConfig.h"
#include "PinConfig.h"
#include "MathUtils.h"
#include "SensorUtils.h"

// using-Deklarationen vermeiden lange SmartHome::-Praefixe im Messwertpfad.
// Sie importieren nur einzelne Hilfsfunktionen, nicht den ganzen Namespace.
using SmartHome::clampToU16;       // begrenzt Messwerte auf uint16_t-Protokollfeld.
using SmartHome::clampHum01pct;    // begrenzt Feuchte auf 0..1000 (= 0,1 %-Schritte).
using SmartHome::absDiffU16;       // Delta-Vergleich fuer unsigned 16-bit-Werte.
using SmartHome::absDiffI16;       // Delta-Vergleich fuer signed 16-bit-Werte.
using SmartHome::recoveryIsDue;    // Intervallpruefung fuer Sensor-Recovery.
using SmartHome::updateAndCheckU32;// schreibt Extended-State und erkennt relevante Aenderungen.

// Compile-Time-Hook-Schalter fuer NetSenRuntime.h. Sie muessen vor dem Include
// stehen, weil der Basistyp damit entscheidet, welche Funktionen er aufruft.
#define NET_SEN_DEVICE_HAS_CUSTOM_SENSOR_HOOKS 1
#define NET_SEN_DEVICE_HAS_CUSTOM_EXTENDED_STATE_HOOKS 1
#define NET_SEN_DEVICE_HAS_CUSTOM_EVENT_HOOKS 1
#define NET_SEN_DEVICE_HAS_CUSTOM_EVENT_SEND_RESULT_HOOK 1

// Forward-Deklarationen fuer die Hooks. NetSenRuntime.h wird direkt danach
// eingebunden und kann die Funktionen dadurch schon referenzieren, obwohl ihre
// Implementierung weiter unten in dieser Datei steht.
void netSenDeviceSensorInit();
bool netSenDeviceSensorPoll(int16_t*, uint16_t*, uint16_t*, uint8_t*, bool*);
void netSenDeviceExtendedStateInit();
bool netSenDeviceExtendedStatePoll(uint32_t*, uint32_t*, uint16_t*, uint16_t*, uint16_t*);
bool netSenDevicePollEvent(uint8_t*, uint8_t*, uint8_t*, uint16_t*);
void netSenDeviceEventSendResult(bool, uint8_t, uint8_t, uint8_t, uint16_t);

// Der Runtime-Include liefert setup()/loop(), ESP-NOW, MQTT-Bridge-Vertrag und
// ruft die oben aktivierten Device-Hooks zyklisch auf.
#include "../../basetypes/net_sen/NetSenRuntime.h"

static_assert(NET_SEN_ENV_BME280_VEML_RAIN_SIGNAL_PIN >= 0,
    "net_sen_weather_station braucht einen gueltigen Regen-Pin.");

// =============================================================================
// KONSTANTEN + LOKALER ZUSTAND
// =============================================================================

namespace {
constexpr uint32_t I2C_CLOCK_HZ = 100000UL; // 100 kHz I2C-Standardtakt.
constexpr unsigned long SENSOR_RECOVERY_RETRY_INTERVAL_MS = 30000UL; // 30000 ms = 30 s Recovery-Retry-Abstand.

struct ErweiterterState {
    uint32_t pressure_pa;
    uint32_t gas_ohm;
    uint16_t aqi;
    uint16_t tvoc_ppb;
    uint16_t eco2_ppm;
};

Adafruit_BME280 sensorBme280;
Adafruit_VEML7700 sensorVeml7700 = Adafruit_VEML7700();

bool bme280Bereit = false;
bool veml7700Bereit = false;
bool regenNass = false;             // Aktueller Regen-Status
bool regenEventOffen = false;       // true = Event noch nicht gesendet
uint8_t regenEventStatus = 0U;      // Gemerkter Event-Status (1=nass)

unsigned long bootMs = 0UL;
unsigned long letzterSensorPollMs = 0UL;
unsigned long letzterBmeFehlerLogMs = 0UL;
unsigned long letzterVemlFehlerLogMs = 0UL;
unsigned long letzterSnapshotLogMs = 0UL;
unsigned long letzterBmeRecoveryMs = 0UL;
unsigned long letzterVemlRecoveryMs = 0UL;
unsigned long veml7700BereitSeitMs = 0UL;

constexpr float NET_SEN_EMA_ALPHA = 0.2f;
float net_sen_temp_ema = NAN, net_sen_hum_ema = NAN;

ErweiterterState erweiterterState = {
    NET_SEN_PRESSURE_UNGUELTIG, NET_SEN_GAS_OHM_UNGUELTIG,
    NET_SEN_AIR_METRIC_UNGUELTIG, NET_SEN_AIR_METRIC_UNGUELTIG,
    NET_SEN_AIR_METRIC_UNGUELTIG};
bool erweiterterStateGeaendert = true;

// Aufgabe: Loggt BME280-Fehler gedrosselt, damit ein Dauerfehler das Log nicht flutet.
// Eingabewerte:
// - j: aktueller millis()-Zeitstempel in Millisekunden.
// - g: Fehlerbeschreibung als C-String.
// Ausgabewert: keiner; logf kommt aus dem NET-SEN-Basistyp.
void logBmeFehler(unsigned long j, const char* g) {
    if ((j - letzterBmeFehlerLogMs) < NET_SEN_ENV_BME280_VEML_RAIN_ERROR_LOG_INTERVAL_MS) return;
    logf("WARN", "BME280 FEHLER: %s", g);
    letzterBmeFehlerLogMs = j;
}

// Aufgabe: Loggt VEML7700-Fehler gedrosselt, damit ein Dauerfehler das Log nicht flutet.
// Eingabewerte:
// - j: aktueller millis()-Zeitstempel in Millisekunden.
// - g: Fehlerbeschreibung als C-String.
// Ausgabewert: keiner; logf kommt aus dem NET-SEN-Basistyp.
void logVemlFehler(unsigned long j, const char* g) {
    if ((j - letzterVemlFehlerLogMs) < NET_SEN_ENV_BME280_VEML_RAIN_ERROR_LOG_INTERVAL_MS) return;
    logf("WARN", "VEML7700 FEHLER: %s", g);
    letzterVemlFehlerLogMs = j;
}

// Aufgabe: Initialisiert den BME280 ueber primaere oder Fallback-I2C-Adresse.
// Eingabewerte: keine; die Adressen kommen aus DeviceConfig.h.
// Ausgabewert: true bedeutet, der Sensor wurde an einer Adresse gefunden.
bool initialisiereBme280() {
    const uint8_t addrs[] = {
        (uint8_t)NET_SEN_ENV_BME280_PRIMARY_ADDRESS,
        (uint8_t)NET_SEN_ENV_BME280_FALLBACK_ADDRESS};
    for (uint8_t a : addrs) {
        if (!sensorBme280.begin(a, &Wire)) continue;
        logf("INFO", "BME280 init OK (0x%02X)", a);
        return true;
    }
    return false;
}

// Aufgabe: Konfiguriert den VEML7700 fuer Luxmessungen.
// Eingabewerte: keine.
// Ausgabewert: keiner; Gain x1 und 100 ms Integrationszeit werden gesetzt.
void konfiguriereVeml7700() {
    sensorVeml7700.setGain(VEML7700_GAIN_1);
    sensorVeml7700.setIntegrationTime(VEML7700_IT_100MS);
}

// Aufgabe: Initialisiert den VEML7700 und merkt den Start der Warmup-Phase.
// Eingabewert: jetzt ist der aktuelle millis()-Zeitstempel in Millisekunden.
// Ausgabewert: true bedeutet, der Sensor antwortet und ist konfiguriert.
bool initialisiereVeml7700(unsigned long jetzt) {
    if (!sensorVeml7700.begin()) return false;
    konfiguriereVeml7700();
    veml7700BereitSeitMs = jetzt;
    return true;
}

// Aufgabe: Versucht BME280-Recovery, wenn der Sensor ausgefallen ist.
// Eingabewert: jetzt ist der aktuelle millis()-Zeitstempel in Millisekunden.
// Ausgabewert: keiner; bme280Bereit wird bei Erfolg wieder true.
// Das Retry-Intervall ist 30000 ms, also 30 Sekunden.
void versucheBmeRecovery(unsigned long jetzt) {
    if (bme280Bereit || !recoveryIsDue(letzterBmeRecoveryMs, jetzt, SENSOR_RECOVERY_RETRY_INTERVAL_MS)) return;
    letzterBmeRecoveryMs = jetzt;
    bme280Bereit = initialisiereBme280();
    if (bme280Bereit) { net_sen_temp_ema = NAN; net_sen_hum_ema = NAN; }
    logf(bme280Bereit ? "INFO" : "WARN",
         bme280Bereit ? "BME280 Recovery ok" : "BME280 Recovery fehlgeschlagen");
}

// Aufgabe: Versucht VEML7700-Recovery, wenn der Sensor ausgefallen ist.
// Eingabewert: jetzt ist der aktuelle millis()-Zeitstempel in Millisekunden.
// Ausgabewert: keiner; veml7700Bereit wird bei Erfolg wieder true.
// Das Retry-Intervall ist 30000 ms, also 30 Sekunden.
void versucheVemlRecovery(unsigned long jetzt) {
    if (veml7700Bereit || !recoveryIsDue(letzterVemlRecoveryMs, jetzt, SENSOR_RECOVERY_RETRY_INTERVAL_MS)) return;
    letzterVemlRecoveryMs = jetzt;
    veml7700Bereit = initialisiereVeml7700(jetzt);
    logf(veml7700Bereit ? "INFO" : "WARN",
         veml7700Bereit ? "VEML7700 Recovery ok" : "VEML7700 Recovery fehlgeschlagen");
}

// Aufgabe: Liest den digitalen Regensensor ueber den konfigurierten GPIO-Pin.
// Eingabewerte: keine; Pin und Wirkrichtung kommen aus DeviceConfig.h.
// Ausgabewert: true bedeutet "nass", false bedeutet "trocken".
// Bei active-LOW bedeutet LOW am Pin "nass".
bool leseRegenNass() {
    const int p = digitalRead(NET_SEN_ENV_BME280_VEML_RAIN_SIGNAL_PIN);
#if NET_SEN_ENV_BME280_VEML_RAIN_ACTIVE_LOW
    return p == LOW;
#else
    return p == HIGH;
#endif
}
}  // namespace

// =============================================================================
// CUSTOM-HOOKS
// =============================================================================

// Aufgabe: Initialisiert I2C, BME280, VEML7700 und den digitalen Regen-Pin.
// Eingabewerte: keine; Pins, Adressen und Pullup-Optionen kommen aus DeviceConfig.h.
// Ausgabewert: keiner; lokale Sensorflags und erster Regenstatus werden gesetzt.
// Aufrufer: NetSenRuntime ruft diesen Hook einmal beim Boot auf.
void netSenDeviceSensorInit() {
    bootMs = millis();
    letzterSensorPollMs = 0UL;
    letzterBmeFehlerLogMs = 0UL;
    letzterVemlFehlerLogMs = 0UL;
    letzterBmeRecoveryMs = 0UL;
    letzterVemlRecoveryMs = 0UL;
    regenEventOffen = false;

    erweiterterState = {NET_SEN_PRESSURE_UNGUELTIG, NET_SEN_GAS_OHM_UNGUELTIG,
                        NET_SEN_AIR_METRIC_UNGUELTIG, NET_SEN_AIR_METRIC_UNGUELTIG,
                        NET_SEN_AIR_METRIC_UNGUELTIG};
    erweiterterStateGeaendert = true;

    Wire.setClock(I2C_CLOCK_HZ);

    bme280Bereit = initialisiereBme280();
    if (bme280Bereit) { net_sen_temp_ema = NAN; net_sen_hum_ema = NAN; }
    else logf("WARN", "BME280 nicht gefunden (0x%02X/0x%02X)",
         NET_SEN_ENV_BME280_PRIMARY_ADDRESS, NET_SEN_ENV_BME280_FALLBACK_ADDRESS);

    veml7700Bereit = initialisiereVeml7700(bootMs);
    logf(veml7700Bereit ? "INFO" : "WARN",
         veml7700Bereit ? "VEML7700 init OK" : "VEML7700 nicht gefunden");

#if NET_SEN_ENV_BME280_VEML_RAIN_USE_PULLUP
    pinMode(NET_SEN_ENV_BME280_VEML_RAIN_SIGNAL_PIN, INPUT_PULLUP);
#else
    pinMode(NET_SEN_ENV_BME280_VEML_RAIN_SIGNAL_PIN, INPUT);
#endif
    regenNass = leseRegenNass();
    regenEventStatus = regenNass ? 1U : 0U;
    regenEventOffen = true;
    logf("INFO", "Regensensor init: pin=%d status=%s",
         NET_SEN_ENV_BME280_VEML_RAIN_SIGNAL_PIN,
         regenNass ? "nass" : "trocken");
}

// Aufgabe: Hook fuer erweiterte Zustandswerte.
// Eingabewerte: keine.
// Ausgabewert: keiner.
// Hinweis: Der lokale erweiterte Zustand wird bereits in netSenDeviceSensorInit vorbereitet.
void netSenDeviceExtendedStateInit() {}

// Aufgabe: Gibt die erweiterten Zustandswerte aus und setzt das Aenderungsflag zurueck.
// Eingabewerte: Zeiger auf Ausgabefelder; nullptr bricht ab.
// Ausgabewerte:
// - pp: Luftdruck in Pascal oder NET_SEN_PRESSURE_UNGUELTIG.
// - go: Gas-Widerstand in Ohm oder NET_SEN_GAS_OHM_UNGUELTIG, hier derzeit ungenutzt.
// - a: Luftqualitaetsindex oder NET_SEN_AIR_METRIC_UNGUELTIG, hier derzeit ungenutzt.
// - t: TVOC in ppb oder NET_SEN_AIR_METRIC_UNGUELTIG, hier derzeit ungenutzt.
// - e: eCO2 in ppm oder NET_SEN_AIR_METRIC_UNGUELTIG, hier derzeit ungenutzt.
// Rueckgabe: true bedeutet, seit dem letzten Abruf gab es eine relevante Aenderung.
bool netSenDeviceExtendedStatePoll(
    uint32_t* pp, uint32_t* go, uint16_t* a, uint16_t* t, uint16_t* e) {
    if (!pp || !go || !a || !t || !e) return false;
    *pp = erweiterterState.pressure_pa;
    *go = erweiterterState.gas_ohm;
    *a = erweiterterState.aqi;
    *t = erweiterterState.tvoc_ppb;
    *e = erweiterterState.eco2_ppm;
    const bool c = erweiterterStateGeaendert;
    erweiterterStateGeaendert = false;
    return c;
}

// Aufgabe: Uebergibt ein ausstehendes Regen-Event an den NET-SEN-Basistyp.
// Eingabewerte: Zeiger auf Ausgabefelder; nullptr wird ignoriert.
// Ausgabewerte:
// - et: SH_EVENT_RAIN_DETECTED aus Protocol.h.
// - tr: SH_TRIGGER_AUTO, weil der Sensor das Event selbst erkannt hat.
// - p1: 1 bei "nass", 0 bei "trocken".
// - p2: hier ungenutzt, bleibt 0.
// Rueckgabe: true bedeutet, ein Event wurde bereitgestellt.
bool netSenDevicePollEvent(uint8_t* et, uint8_t* tr, uint8_t* p1, uint16_t* p2) {
    if (!regenEventOffen) return false;
    regenEventOffen = false;
    if (et) *et = SH_EVENT_RAIN_DETECTED;
    if (tr) *tr = SH_TRIGGER_AUTO;
    if (p1) *p1 = regenEventStatus;
    if (p2) *p2 = 0U;
    return true;
}

// Aufgabe: Merkt ein Regen-Event erneut vor, wenn der Basistyp es nicht senden konnte.
// Eingabewerte: Ergebnis und urspruengliche Eventdaten.
// Ausgabewert: keiner; bei fehlgeschlagenem Regen-Event wird der Event erneut gepuffert.
void netSenDeviceEventSendResult(bool gesendet, uint8_t et, uint8_t, uint8_t p1, uint16_t) {
    if (gesendet || et != SH_EVENT_RAIN_DETECTED) return;
    regenEventStatus = p1 ? 1U : 0U;
    regenEventOffen = true;
}

// Aufgabe: Pollt alle Sensoren, aktualisiert Messwerte und setzt Event-/STATE-Bedarf.
// Eingabewerte:
// - temp_01c: Ein-/Ausgabe Temperatur in 0,1 Grad Celsius.
// - hum_01pct: Ein-/Ausgabe relative Feuchte in 0,1 Prozent.
// - lux: Ein-/Ausgabe Beleuchtungsstaerke in Lux.
// - motion: Ausgabe Bewegung; bei diesem Device ungenutzt und immer 0.
// - fault: Ein-/Ausgabe Fehlerflag fuer Sensorstatus.
// Ausgabewert: true bedeutet, mindestens ein relevanter Wert hat sich geaendert.
//
// Ablauf:
// 1. NET_SEN_ENV_BME280_VEML_RAIN_SENSOR_READ_INTERVAL_MS begrenzt die Messrate in Millisekunden.
// 2. Sensor-Recovery versucht ausgefallene Sensoren nach 30 Sekunden neu zu initialisieren.
// 3. BME280 liefert Temperatur, Feuchte und Druck; unplausible Werte deaktivieren den Sensor.
// 4. VEML7700 liefert Lux erst nach der Warmup-Zeit NET_SEN_ENV_VEML7700_FIRST_READ_DELAY_MS.
// 5. Digitaler Regensensor erzeugt bei nass/trocken-Wechsel ein Event.
// 6. Druck wird als Extended State mit Delta-Schwelle gemeldet.
bool netSenDeviceSensorPoll(
    int16_t* temp_01c, uint16_t* hum_01pct,
    uint16_t* lux, uint8_t* motion, bool* fault)
{
    if (!temp_01c || !hum_01pct || !lux || !motion || !fault) return false;

    const unsigned long jetzt = millis();
    if ((jetzt - letzterSensorPollMs) < NET_SEN_ENV_BME280_VEML_RAIN_SENSOR_READ_INTERVAL_MS)
        return false;
    letzterSensorPollMs = jetzt;

    const int16_t vT = *temp_01c;
    const uint16_t vH = *hum_01pct;
    const uint16_t vL = *lux;
    const uint8_t vM = *motion;
    const bool vF = *fault;

    int16_t nT = vT; uint16_t nH = vH; uint16_t nL = vL;
    uint32_t nP = NET_SEN_PRESSURE_UNGUELTIG;
    bool bmeOk = false, vemlOk = false;

    versucheBmeRecovery(jetzt);
    versucheVemlRecovery(jetzt);

    // BME280-Messung: Temperatur, Feuchte und Druck werden auf Plausibilitaet geprueft.
    if (bme280Bereit) {
        const float t = sensorBme280.readTemperature();
        const float h = sensorBme280.readHumidity();
        const float p = sensorBme280.readPressure();
        const bool gueltig = isfinite(t) && isfinite(h) && isfinite(p) &&
            h >= 0.0f && h <= 100.0f && p >= 30000.0f && p <= 110000.0f;
        if (gueltig) {
            const float tempConv = lroundf(t * 10.0f) + NET_SEN_TEMP_OFFSET_01C;
            const float humConv = lroundf(h * 10.0f) + NET_SEN_HUM_OFFSET_01PCT;
            if (isnan(net_sen_temp_ema)) { net_sen_temp_ema = tempConv; net_sen_hum_ema = humConv; }
            else {
                net_sen_temp_ema = NET_SEN_EMA_ALPHA * tempConv + (1.0f - NET_SEN_EMA_ALPHA) * net_sen_temp_ema;
                net_sen_hum_ema = NET_SEN_EMA_ALPHA * humConv + (1.0f - NET_SEN_EMA_ALPHA) * net_sen_hum_ema;
            }
            nT = (int16_t)lroundf(net_sen_temp_ema);
            nH = clampHum01pct((long)lroundf(net_sen_hum_ema));
            nP = (uint32_t)lroundf(p);
            bmeOk = true;
        } else {
            bme280Bereit = false;
            nT = INT16_MIN;
            nH = 0xFFFFU;
            logBmeFehler(jetzt, "Messwerte unplausibel");
        }
    } else {
        nT = INT16_MIN;
        nH = 0xFFFFU;
        logBmeFehler(jetzt, "Sensor nicht initialisiert");
    }

    // VEML7700-Messung: erster Luxwert erst nach Warmup, damit kein Start-Artefakt gemeldet wird.
    const bool warmup = veml7700Bereit &&
        (jetzt - veml7700BereitSeitMs) < NET_SEN_ENV_VEML7700_FIRST_READ_DELAY_MS;
    if (veml7700Bereit && !warmup) {
        const float l = sensorVeml7700.readLux();
        if (isfinite(l) && l >= 0.0f) { nL = clampToU16((long)lroundf(l)); vemlOk = true; }
        else { veml7700Bereit = false; nL = 0xFFFFU; logVemlFehler(jetzt, "Lux unplausibel"); }
    } else if (!veml7700Bereit) {
        nL = 0xFFFFU;
        logVemlFehler(jetzt, "Sensor nicht initialisiert");
    }

    // Regen-Digital-Eingang: jeder nass/trocken-Wechsel wird als Event gemerkt.
    const bool nRegen = leseRegenNass();
    if (nRegen != regenNass) {
        regenNass = nRegen;
        regenEventStatus = regenNass ? 1U : 0U;
        regenEventOffen = true;
        logf("INFO", "Regensensor Status: %s", regenNass ? "nass" : "trocken");
    }

    // Extended State: Druck nur bei signifikanter Aenderung als Zusatzwert melden.
    const bool extGeaendert = updateAndCheckU32(
        &erweiterterState.pressure_pa, nP, NET_SEN_PRESSURE_UNGUELTIG,
        NET_SEN_ENV_BME280_VEML_RAIN_PRESSURE_DELTA_PA);
    if (extGeaendert) erweiterterStateGeaendert = true;

    const bool nF = !bmeOk || (!vemlOk && !warmup);

    *temp_01c = nT; *hum_01pct = nH; *lux = nL;
    *motion = 0U; *fault = nF;

    if ((jetzt - letzterSnapshotLogMs) >= NET_SEN_ENV_BME280_VEML_RAIN_SNAPSHOT_LOG_INTERVAL_MS) {
        logf("INFO", "snapshot t=%d h=%u l=%u p=%lu r=%s f=%s",
             nT, nH, nL, (unsigned long)erweiterterState.pressure_pa,
             regenNass ? "nass" : "trocken", nF ? "true" : "false");
        letzterSnapshotLogMs = jetzt;
    }

    return absDiffI16(nT, vT) >= NET_SEN_ENV_BME280_VEML_RAIN_TEMP_DELTA_01C ||
           absDiffU16(nH, vH) >= NET_SEN_ENV_BME280_VEML_RAIN_HUM_DELTA_01PCT ||
           absDiffU16(nL, vL) >= NET_SEN_ENV_BME280_VEML_RAIN_LUX_DELTA ||
           vM != 0U || nF != vF;
}
