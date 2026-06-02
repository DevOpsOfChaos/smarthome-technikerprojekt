/*
===============================================================================
 Datei: ShStorage.h
 Code-Name: ShStorage
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / gemeinsame Bibliothek
 Ersteller: DevOpsOfChaos
 Letzte Bearbeitung: 2026-05-18

 Zweck: Persistente Node-Einstellungen
 Beschreibung: Speichert gemeinsame Einstellungen wie Master-MAC, Anzeigename und Intervalle dauerhaft im Flash.

 Genutzte Bibliotheken:
 - Arduino.h: Arduino-Grundtypen und Hilfsfunktionen.
 - Preferences.h: ESP32-Flashspeicher fuer dauerhafte Einstellungen.
 - Protocol.h: eigene Protokollbibliothek fuer Laengen und gemeinsame Konstanten.

 Aenderungsverlauf:
 - 2026-05-18: Kommentarstil vereinheitlicht und Doxygen-Metakommentare entfernt.
===============================================================================
 */
// Hinweis: SharedNodeSettings und NodeBasisSettings (aus ShNodeProvisioning.h)
// duplizieren teilweise dieselben Felder (magic, version, flags, master_mac).
// Dies ist historisch gewachsen: ShStorage ist der aeltere NVS-Treiber,
// NodeProvisioningController wurde spaeter als Abstraktion daruebergelegt.
// Eine Zusammenfuehrung ist wuenschenswert, aber nicht trivial (unterschiedliche
// Feldbreiten und Default-Werte).
#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include <Protocol.h>

namespace SmartHome {
namespace ShStorage {

// Magic und Version schuetzen den Flashspeicher vor Fehlinterpretation:
// Nur wenn beide Werte passen, wird ein gelesener Byteblock als aktuelle
// SharedNodeSettings-Struktur akzeptiert.
constexpr uint32_t SH_NODE_SETTINGS_MAGIC = 0x53484E50UL;
constexpr uint16_t SH_NODE_SETTINGS_VERSION = 2U;

// Flags sind einzelne Bits in einem uint32_t. Dadurch kann ein gespeicherter
// Wert vorhanden sein, ohne dass ein eigener bool pro Feld im Flashlayout noetig
// ist. Pruefung: (settings.flags & SH_NODE_SETTINGS_FLAG_...) != 0.
constexpr uint32_t SH_NODE_SETTINGS_FLAG_MASTER_BOUND = 0x00000001UL;
constexpr uint32_t SH_NODE_SETTINGS_FLAG_AUTO_OFF_DELAY_SET = 0x00000002UL;
constexpr uint32_t SH_NODE_SETTINGS_FLAG_LIGHT_THRESHOLD_SET = 0x00000004UL;

constexpr uint32_t SH_STORED_REPORT_INTERVAL_MIN_S = 10UL;
constexpr uint32_t SH_STORED_REPORT_INTERVAL_MAX_S = 65535UL;
constexpr uint32_t SH_STORED_WAKE_INTERVAL_MIN_S = 10UL;
constexpr uint32_t SH_STORED_WAKE_INTERVAL_MAX_S = 604800UL;
constexpr uint32_t SH_STORED_AUTO_OFF_DELAY_MIN_S = 0UL;
constexpr uint32_t SH_STORED_AUTO_OFF_DELAY_MAX_S = 65535UL;
constexpr uint16_t SH_STORED_LIGHT_THRESHOLD_ON_MIN = 0U;
constexpr uint16_t SH_STORED_LIGHT_THRESHOLD_ON_MAX = 2000U;

static_assert(
    SH_STORED_REPORT_INTERVAL_MAX_S <= 65535UL,
    "Shared report interval must fit into protocol uint16 state fields.");

static_assert(
    SH_STORED_AUTO_OFF_DELAY_MAX_S <= 65535UL,
    "Shared auto-off delay must fit into protocol uint16 cfg/state fields.");

struct SharedNodeSettings {
    uint32_t magic;                         // Kennwert gegen fremde/alte NVS-Daten
    uint16_t version;                       // Strukturversion fuer Migrationen
    uint16_t reserved;                      // frei gehalten fuer spaetere Erweiterung
    uint32_t flags;                         // Bitmaske fuer optionale gespeicherte Werte
    uint8_t master_mac[6];                  // rohe MAC-Bytes, kein Textformat
    uint8_t reserved_mac[2];                // Auffuellung auf 4-Byte-Grenze
    char device_name[SH_DEVICE_NAME_LEN];   // nullterminierter Anzeigename
    uint32_t report_interval_s;             // Sekundenwert fuer Netzgeraete
    uint32_t wake_interval_s;               // Sekundenwert fuer Batteriegeraete
    uint32_t auto_off_delay_s;              // Sekundenwert, 0 bedeutet deaktiviert
    uint16_t light_threshold_on;            // Lux-Grenze fuer Auto-Licht
    uint16_t reserved_cfg;                  // frei gehalten fuer weitere Komfortwerte
};

/*
    Funktionsblock: Standardwerte und Pruefung der Speicherstruktur

    Diese Funktionen arbeiten nur auf der Struktur SharedNodeSettings.
    Sie greifen noch nicht direkt auf den Flash-Speicher zu.
*/

// Aufgabe: Fuellt eine leere SharedNodeSettings-Struktur mit sicheren Startwerten.
// Eingabewerte: settings wird beschrieben, defaultDeviceName ist der Anzeigename.
// Eingabewerte: defaultReportIntervalS/defaultWakeIntervalS sind Sekundenwerte.
// Ausgabewert: keiner; settings enthaelt danach die vorbereiteten Werte.
void makeDefaultSettings(
    SharedNodeSettings& settings,
    const char* defaultDeviceName,
    uint32_t defaultReportIntervalS,
    uint32_t defaultWakeIntervalS);

// Aufgabe: Prueft, ob Magic-Nummer und Versionsnummer zur aktuellen Struktur passen.
// Eingabewert: settings mit den aus dem Speicher gelesenen Daten.
// Ausgabewert: true bedeutet verwendbar, false bedeutet falsche oder alte Daten.
bool isSettingsStructValid(const SharedNodeSettings& settings);

// Aufgabe: Prueft, ob eine Master-MAC gespeichert und als gueltig markiert ist.
// Eingabewert: settings mit den Node-Einstellungen.
// Ausgabewert: true bedeutet, der Node ist an einen Master gebunden.
bool hasStoredMasterMac(const SharedNodeSettings& settings);

// Aufgabe: Prueft, ob eine eigene Auto-Aus-Zeit gespeichert ist.
// Eingabewert: settings mit den Node-Einstellungen.
// Ausgabewert: true bedeutet, der gespeicherte Wert soll den Standard ersetzen.
bool hasStoredAutoOffDelaySeconds(const SharedNodeSettings& settings);

// Aufgabe: Prueft, ob eine eigene Lux-Einschaltschwelle gespeichert ist.
// Eingabewert: settings mit den Node-Einstellungen.
// Ausgabewert: true bedeutet, der gespeicherte Wert soll den Standard ersetzen.
bool hasStoredLightThresholdOn(const SharedNodeSettings& settings);

/*
    Funktionsblock: Bereichspruefungen fuer gespeicherte Werte

    Alle Zeitwerte mit Endung S sind Sekundenwerte.
    Beispiel: 60 bedeutet 60 Sekunden, also 1 Minute.
    604800 Sekunden bedeutet 7 Tage.
*/

// Aufgabe: Prueft das Meldeintervall fuer netzbetriebene Geraete.
// Eingabewert: reportIntervalS in Sekunden.
// Ausgabewert: true wenn der Wert innerhalb der erlaubten Grenzen liegt.
bool isValidStoredReportIntervalSeconds(uint32_t reportIntervalS);

// Aufgabe: Prueft das Aufwachintervall fuer Batteriegeraete.
// Eingabewert: wakeIntervalS in Sekunden.
// Ausgabewert: true wenn der Wert innerhalb der erlaubten Grenzen liegt.
bool isValidStoredWakeIntervalSeconds(uint32_t wakeIntervalS);

// Aufgabe: Prueft die Auto-Aus-Zeit fuer Relais.
// Eingabewert: autoOffDelayS in Sekunden; 0 bedeutet keine Auto-Aus-Zeit.
// Ausgabewert: true wenn der Wert gespeichert werden darf.
bool isValidStoredAutoOffDelaySeconds(uint32_t autoOffDelayS);

// Aufgabe: Prueft die Lux-Schwelle fuer automatisches Einschalten.
// Eingabewert: lightThresholdOn als Lux-Wert.
// Ausgabewert: true wenn der Wert im erlaubten Bereich liegt.
bool isValidStoredLightThresholdOn(uint16_t lightThresholdOn);

/*
    Funktionsblock: Werte in der Einstellungsstruktur setzen oder loeschen

    Diese Funktionen aendern nur die uebergebene Struktur.
    Gespeichert wird erst spaeter mit SharedNodeStorage::save().
*/

// Aufgabe: Speichert eine Master-MAC in der Struktur.
// Eingabewerte: settings wird geaendert, masterMac zeigt auf 6 MAC-Bytes.
// Ausgabewert: true bei gueltiger MAC, false bei ungueltiger oder leerer MAC.
bool setStoredMasterMac(SharedNodeSettings& settings, const uint8_t* masterMac);

// Aufgabe: Loescht die Master-Bindung in der Struktur.
// Eingabewert: settings wird geaendert.
// Ausgabewert: keiner.
void clearStoredMasterMac(SharedNodeSettings& settings);

// Aufgabe: Speichert den Anzeigenamen des Geraets.
// Eingabewerte: settings wird geaendert, deviceName ist der neue Name.
// Ausgabewert: keiner; zu lange Namen werden passend gekuerzt.
void setStoredDeviceName(SharedNodeSettings& settings, const char* deviceName);

// Aufgabe: Speichert das Meldeintervall.
// Eingabewerte: settings wird geaendert, reportIntervalS ist eine Zeit in Sekunden.
// Ausgabewert: keiner; ungueltige Werte werden nicht uebernommen.
void setStoredReportIntervalSeconds(SharedNodeSettings& settings, uint32_t reportIntervalS);

// Aufgabe: Speichert das Aufwachintervall fuer Batteriegeraete.
// Eingabewerte: settings wird geaendert, wakeIntervalS ist eine Zeit in Sekunden.
// Ausgabewert: keiner; ungueltige Werte werden nicht uebernommen.
void setStoredWakeIntervalSeconds(SharedNodeSettings& settings, uint32_t wakeIntervalS);

// Aufgabe: Speichert eine Auto-Aus-Zeit fuer Relais.
// Eingabewerte: settings wird geaendert, autoOffDelayS ist eine Zeit in Sekunden.
// Ausgabewert: keiner; 0 Sekunden bedeutet Auto-Aus ist deaktiviert.
void setStoredAutoOffDelaySeconds(SharedNodeSettings& settings, uint32_t autoOffDelayS);

// Aufgabe: Entfernt die gespeicherte Auto-Aus-Zeit.
// Eingabewert: settings wird geaendert.
// Ausgabewert: keiner; danach gilt wieder der Geraetestandard.
void clearStoredAutoOffDelaySeconds(SharedNodeSettings& settings);

// Aufgabe: Speichert die Lux-Schwelle fuer automatisches Einschalten.
// Eingabewerte: settings wird geaendert, lightThresholdOn ist ein Lux-Wert.
// Ausgabewert: keiner; ungueltige Werte werden nicht uebernommen.
void setStoredLightThresholdOn(SharedNodeSettings& settings, uint16_t lightThresholdOn);

// Aufgabe: Entfernt die gespeicherte Lux-Schwelle.
// Eingabewert: settings wird geaendert.
// Ausgabewert: keiner; danach gilt wieder der Geraetestandard.
void clearStoredLightThresholdOn(SharedNodeSettings& settings);

/*
    Funktionsblock: Effektive Werte berechnen

    Effektiv bedeutet: Falls ein gueltiger Wert gespeichert ist, wird dieser genutzt.
    Sonst wird der uebergebene Standardwert zurueckgegeben.
*/

// Aufgabe: Liefert das tatsaechlich zu nutzende Meldeintervall.
// Eingabewerte: settings und defaultReportIntervalS in Sekunden.
// Ausgabewert: Intervall in Sekunden.
uint32_t effectiveReportIntervalSeconds(const SharedNodeSettings& settings, uint32_t defaultReportIntervalS);

// Aufgabe: Liefert das tatsaechlich zu nutzende Aufwachintervall.
// Eingabewerte: settings und defaultWakeIntervalS in Sekunden.
// Ausgabewert: Intervall in Sekunden.
uint32_t effectiveWakeIntervalSeconds(const SharedNodeSettings& settings, uint32_t defaultWakeIntervalS);

// Aufgabe: Liefert die tatsaechlich zu nutzende Auto-Aus-Zeit.
// Eingabewerte: settings und defaultAutoOffDelayS in Sekunden.
// Ausgabewert: Zeit in Sekunden.
uint32_t effectiveAutoOffDelaySeconds(const SharedNodeSettings& settings, uint32_t defaultAutoOffDelayS);

// Aufgabe: Liefert die tatsaechlich zu nutzende Lux-Schwelle.
// Eingabewerte: settings und defaultLightThresholdOn in Lux.
// Ausgabewert: Lux-Wert.
uint16_t effectiveLightThresholdOn(const SharedNodeSettings& settings, uint16_t defaultLightThresholdOn);

// Aufgabe: Prueft, ob beim Meldeintervall der Standardwert verwendet wird.
// Eingabewerte: settings und defaultReportIntervalS in Sekunden.
// Ausgabewert: true wenn kein abweichender Speicherwert aktiv ist.
bool usesDefaultReportIntervalSeconds(const SharedNodeSettings& settings, uint32_t defaultReportIntervalS);

// Aufgabe: Prueft, ob beim Aufwachintervall der Standardwert verwendet wird.
// Eingabewerte: settings und defaultWakeIntervalS in Sekunden.
// Ausgabewert: true wenn kein abweichender Speicherwert aktiv ist.
bool usesDefaultWakeIntervalSeconds(const SharedNodeSettings& settings, uint32_t defaultWakeIntervalS);

// Aufgabe: Prueft, ob bei Auto-Aus der Standardwert verwendet wird.
// Eingabewerte: settings und defaultAutoOffDelayS in Sekunden.
// Ausgabewert: true wenn kein abweichender Speicherwert aktiv ist.
bool usesDefaultAutoOffDelaySeconds(const SharedNodeSettings& settings, uint32_t defaultAutoOffDelayS);

// Aufgabe: Prueft, ob bei der Lux-Schwelle der Standardwert verwendet wird.
// Eingabewerte: settings und defaultLightThresholdOn in Lux.
// Ausgabewert: true wenn kein abweichender Speicherwert aktiv ist.
bool usesDefaultLightThresholdOn(const SharedNodeSettings& settings, uint16_t defaultLightThresholdOn);

/*
    Funktionsblock: Gespeicherte Werte gegen Standards aufraeumen

    Damit werden ueberfluessige gespeicherte Sonderwerte entfernt,
    wenn sie inzwischen dem normalen Standard entsprechen.
*/

// Aufgabe: Entfernt Komfort-Sonderwerte, wenn sie identisch mit den Standards sind.
// Eingabewerte: settings wird geaendert, defaultAutoOffDelayS in Sekunden, defaultLightThresholdOn in Lux.
// Ausgabewert: keiner.
void normalizeComfortOverridesAgainstDefaults(
    SharedNodeSettings& settings,
    uint32_t defaultAutoOffDelayS,
    uint16_t defaultLightThresholdOn);

// Aufgabe: Raeumt Basis- und Komfortwerte gegen die aktuellen Standards auf.
// Eingabewerte: settings wird geaendert; Zeitwerte sind Sekundenwerte.
// Ausgabewert: keiner.
void normalizeBasisValuesAgainstDefaults(
    SharedNodeSettings& settings,
    uint32_t defaultReportIntervalS,
    uint32_t defaultWakeIntervalS,
    uint32_t defaultAutoOffDelayS,
    uint16_t defaultLightThresholdOn);

/*
    Funktionsblock: MAC-Adresse als Text lesen und schreiben

    Eine MAC-Adresse besteht aus 6 Bytes.
    Als Text sieht sie zum Beispiel so aus: AA:BB:CC:DD:EE:FF.
*/

// Aufgabe: Wandelt einen MAC-Text in 6 einzelne Bytes um.
// Eingabewert: text im Format AA:BB:CC:DD:EE:FF.
// Ausgabewert: true bei Erfolg; outMac enthaelt dann 6 Bytes.
bool parseMacText(const char* text, uint8_t outMac[6]);

// Aufgabe: Wandelt 6 MAC-Bytes in lesbaren Text um.
// Eingabewerte: mac zeigt auf 6 Bytes, buffer ist der Zieltextpuffer.
// Ausgabewert: keiner; buffer enthaelt danach den Text, wenn genug Platz vorhanden ist.
void formatMacText(const uint8_t* mac, char* buffer, size_t bufferSize);

class SharedNodeStorage {
  public:
    SharedNodeStorage() = default;

    // Aufgabe: Laedt gespeicherte Einstellungen aus dem ESP32-Flash.
    // Eingabewerte: defaultDeviceName, defaultReportIntervalS und defaultWakeIntervalS als Fallbacks.
    // Ausgabewert: true bei erfolgreichem Laden; settings enthaelt danach gueltige Werte.
    bool load(
        SharedNodeSettings& settings,
        const char* defaultDeviceName,
        uint32_t defaultReportIntervalS,
        uint32_t defaultWakeIntervalS);

    // Aufgabe: Speichert Einstellungen dauerhaft im ESP32-Flash.
    // Eingabewert: settings mit den zu speichernden Werten.
    // Ausgabewert: true bei erfolgreichem Speichern.
    bool save(const SharedNodeSettings& settings);

    // Aufgabe: Loescht die gespeicherten Einstellungen.
    // Eingabewert: keiner.
    // Ausgabewert: true wenn das Loeschen erfolgreich war.
    bool factoryReset();
};

}  // namespace ShStorage
}  // namespace SmartHome
