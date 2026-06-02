/*
===============================================================================
 Datei: ShNodeProvisioning.h
 Code-Name: ShNodeProvisioning
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / gemeinsame Bibliothek
 Ersteller: DevOpsOfChaos
 Letzte Bearbeitung: 2026-05-18

 Zweck: Schnittstelle der Node-Provisionierung
 Beschreibung: Beschreibt Konfiguration, Speicherstruktur und Controller fuer das Setup-Portal.

 Genutzte Bibliotheken:
 - Arduino.h: Arduino-Grundtypen wie String und GPIO-Funktionen.
 - Preferences.h: ESP32-Speicher im Flash fuer dauerhafte Einstellungen.
 - WebServer.h: kleiner HTTP-Server fuer das Setup-Portal.

 Aenderungsverlauf:
 - 2026-05-18: Kommentarstil vereinheitlicht und Doxygen-Metakommentare entfernt.
===============================================================================
*/
#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>

namespace SmartHome {
namespace ShNodeProvisioning {

// Laenge fuer eine MAC-Adresse als Text: "AA:BB:CC:DD:EE:FF" sind 17 Zeichen
// plus abschliessendes Null-Zeichen. Deshalb braucht der Puffer 18 Zeichen.
constexpr size_t MASTER_MAC_TEXT_LEN = 18U;

/*
    Datenblock: Gemeinsame Basis-Einstellungen eines Nodes

    Diese Daten werden im ESP32-Flash gespeichert.
    Sie enthalten nur Werte, die mehrere Geraetetypen gemeinsam brauchen.
*/
struct NodeBasisSettings {
    uint32_t magic;               // Kennwert gegen fremde/alte NVS-Bloecke
    uint16_t version;             // Strukturversion fuer spaetere Migrationen
    uint16_t reserved;            // bewusst frei, haelt Layout erweiterbar
    uint32_t flags;               // Bitmaske, z.B. ob masterMac gueltig ist
    uint8_t masterMac[6];         // rohe 6-Byte-MAC, nicht als Text gespeichert
    uint8_t reservedMac[2];       // Auffuellung auf 4-Byte-Grenze
    uint32_t statusSendIntervalS; // Sekundenwert fuer Statusmeldungen
    uint32_t sensorSendIntervalS; // Sekundenwert fuer Sensormeldungen
};

// Kurzzeit-Sicherung der Basis-Einstellungen.
// Wird genutzt, damit beim fehlgeschlagenen Speichern der alte Zustand
// wiederhergestellt werden kann.
struct NodeBasisSnapshot {
    bool masterMacValid;
    uint8_t masterMac[6];
    uint32_t statusSendIntervalS;
    uint32_t sensorSendIntervalS;
};

/*
    Datenblock: Konfiguration fuer das Setup-Portal

    Diese Struktur wird vom Geraete-Code gefuellt und an den Controller uebergeben.
    Zeitwerte mit Endung S sind Sekunden.
    Zeitwerte mit Endung Ms sind Millisekunden.
*/
struct NodeProvisioningConfig {
    NodeProvisioningConfig() = default;

    // Aufgabe: Erstellt eine Konfiguration mit den wichtigsten Pflichtwerten.
    // Eingabewerte: SSID, Passwort, Speichernamen, Standardintervalle und AP-Kanal.
    // Ausgabewert: kein Rueckgabewert; die Struktur wird direkt gefuellt.
    NodeProvisioningConfig(
        const char* setupApSsidValue,
        const char* setupApPasswordValue,
        const char* storageNamespaceValue,
        const char* basisStorageKeyValue,
        uint32_t defaultStatusSendIntervalSValue,
        uint32_t defaultSensorSendIntervalSValue,
        uint32_t minSendIntervalSValue,
        uint32_t maxSendIntervalSValue,
        unsigned long restartDelayMsValue,
        int apChannelValue)
        : setupApSsid(setupApSsidValue),
          setupApPassword(setupApPasswordValue),
          storageNamespace(storageNamespaceValue),
          basisStorageKey(basisStorageKeyValue),
          defaultStatusSendIntervalS(defaultStatusSendIntervalSValue),
          defaultSensorSendIntervalS(defaultSensorSendIntervalSValue),
          minSendIntervalS(minSendIntervalSValue),
          maxSendIntervalS(maxSendIntervalSValue),
          restartDelayMs(restartDelayMsValue),
          apChannel(apChannelValue) {}

    // Diese const-char-Zeiger zeigen auf Stringliterale oder dauerhaft gueltige
    // Puffer des Geraete-Codes. Der Controller kopiert die Texte nicht komplett.
    const char* setupApSsid = nullptr;
    const char* setupApPassword = nullptr;
    const char* storageNamespace = nullptr;
    const char* basisStorageKey = nullptr;
    uint32_t defaultStatusSendIntervalS = 0UL;
    uint32_t defaultSensorSendIntervalS = 0UL;
    uint32_t minSendIntervalS = 0UL;
    uint32_t maxSendIntervalS = 0UL;
    unsigned long restartDelayMs = 0UL;
    int apChannel = 1;
    const char* statusSendIntervalFieldName = "status_send_interval_s";
    const char* sensorSendIntervalFieldName = "sensor_send_interval_s";
    const char* statusSendIntervalLabel = "status_send_interval_s";
    const char* sensorSendIntervalLabel = "sensor_send_interval_s";
    const char* statusSendIntervalHint = "Statusintervall in Sekunden.";
    const char* sensorSendIntervalHint = "Sensorintervall in Sekunden.";
    uint32_t minSensorSendIntervalS = 0UL;
    uint32_t maxSensorSendIntervalS = 0UL;
    int setupButtonPin = -1;
    bool setupButtonActiveLow = true;

    // 5000 Millisekunden sind 5 Sekunden.
    // So lange muss der Setup-Taster gehalten werden, bevor das Setup startet.
    unsigned long setupButtonHoldMs = 5000UL;

    int setupIndicatorLedPin = -1;
    bool setupIndicatorLedActiveHigh = true;

    // 500 Millisekunden sind 0,5 Sekunden.
    // Die Setup-LED blinkt damit zweimal pro Sekunde: 0,5 s an, 0,5 s aus.
    unsigned long setupIndicatorBlinkMs = 500UL;
};

// Funktionszeiger fuer Log-Ausgaben.
// Eingabewerte: level ist zum Beispiel "INFO" oder "WARN", message ist der Text.
// Ausgabewert: keiner; die konkrete Ausgabe macht der Geraete-Code.
using SetupLogFn = void (*)(const char* level, const char* message);

/*
    Schnittstelle: Geraetespezifischer Teil des Setup-Portals

    Die gemeinsame Bibliothek kennt nur die Basiswerte.
    Alles, was ein konkretes Geraet zusaetzlich braucht, liefert diese Klasse.
*/
class DeviceProvisioningHandler {
  public:
    virtual ~DeviceProvisioningHandler() = default;

    // Aufgabe: Liefert den Seitentitel des Setup-Portals.
    // Eingabewert: keiner.
    // Ausgabewert: Text fuer die Webseite.
    virtual const char* pageTitle() const = 0;

    // Aufgabe: Liefert den Einleitungstext des Setup-Portals.
    // Eingabewert: keiner.
    // Ausgabewert: Text fuer die Webseite.
    virtual const char* pageIntro() const = 0;

    // Aufgabe: Liefert die Ueberschrift fuer den geraetespezifischen Bereich.
    // Eingabewert: keiner.
    // Ausgabewert: Text fuer die Webseite.
    virtual const char* deviceSectionTitle() const = 0;

    // Aufgabe: Liefert die Erklaerung fuer den geraetespezifischen Bereich.
    // Eingabewert: keiner.
    // Ausgabewert: Text fuer die Webseite.
    virtual const char* deviceSectionIntro() const = 0;

    // Aufgabe: Setzt geraetespezifische Standardwerte.
    // Eingabewert: keiner.
    // Ausgabewert: keiner.
    virtual void loadDeviceDefaults() = 0;

    // Aufgabe: Laedt geraetespezifische Werte aus dem Flash-Speicher.
    // Eingabewert: prefs ist der geoeffnete ESP32-Preferences-Speicher.
    // Ausgabewert: true wenn passende Werte gefunden wurden.
    virtual bool loadDeviceSettings(Preferences& prefs) = 0;

    // Aufgabe: Speichert geraetespezifische Werte im Flash-Speicher.
    // Eingabewert: prefs ist der geoeffnete ESP32-Preferences-Speicher.
    // Ausgabewert: true bei erfolgreichem Speichern.
    virtual bool saveDeviceSettings(Preferences& prefs) = 0;

    // Aufgabe: Loescht geraetespezifische Werte aus dem Flash-Speicher.
    // Eingabewert: prefs ist der geoeffnete ESP32-Preferences-Speicher.
    // Ausgabewert: true bei erfolgreichem Loeschen.
    virtual bool clearDeviceSettings(Preferences& prefs) = 0;

    // Aufgabe: Sichert den aktuellen geraetespezifischen Zustand fuer Rollback.
    // Eingabewert: keiner.
    // Ausgabewert: keiner.
    virtual void captureDeviceSnapshot() = 0;

    // Aufgabe: Stellt den vorher gesicherten geraetespezifischen Zustand wieder her.
    // Eingabewert: keiner.
    // Ausgabewert: keiner.
    virtual void restoreDeviceSnapshot() = 0;

    // Aufgabe: Liest und prueft die geraetespezifischen Formularwerte.
    // Eingabewerte: server enthaelt den HTTP-Request, errorText nimmt Fehlermeldungen auf.
    // Ausgabewert: true wenn die Eingaben gueltig sind.
    virtual bool parseDeviceSave(WebServer& server, String& errorText) = 0;

    // Aufgabe: Uebernimmt die vorher erfolgreich gelesenen Formularwerte.
    // Eingabewert: keiner.
    // Ausgabewert: keiner.
    virtual void applyParsedDeviceSettings() = 0;

    // Aufgabe: Verwirft gelesene Formularwerte, wenn Speichern oder Validierung scheitert.
    // Eingabewert: keiner.
    // Ausgabewert: keiner.
    virtual void discardParsedDeviceSettings() = 0;

    // Aufgabe: Haengt geraetespezifische HTML-Formularfelder an die Setup-Seite an.
    // Eingabewerte: page wird erweitert, sourceServer kann aktuelle Formularwerte liefern.
    // Ausgabewert: keiner.
    virtual void appendDeviceFieldsHtml(String& page, WebServer* sourceServer) const = 0;

    // Aufgabe: Haengt optionale geraetespezifische Aktionsbuttons an.
    // Eingabewert: page wird erweitert.
    // Ausgabewert: keiner.
    virtual void appendDeviceActionsHtml(String& page) const { (void)page; }

    // Aufgabe: Verarbeitet eine optionale geraetespezifische Aktion aus dem Formular.
    // Eingabewerte: server enthaelt den Request; Titel, Nachricht, Statuscode und Neustart-Flag werden beschrieben.
    // Ausgabewert: true wenn die Aktion erfolgreich war.
    virtual bool handleDeviceAction(
        WebServer& server,
        String& titleText,
        String& messageText,
        int& statusCode,
        bool& restartRequired) {
        (void)server;
        titleText = F("Aktion ungueltig");
        messageText = F("Dieses Geraet kennt keine separate Setup-Aktion.");
        statusCode = 400;
        restartRequired = false;
        return false;
    }

};

/*
    Klasse: Gemeinsamer Controller fuer das Setup-Portal

    Diese Klasse verbindet Setup-Taster, Setup-LED, Webformular und Flash-Speicher.
    Der normale Geraete-Code ruft begin() beim Start und update() regelmaessig in loop().
*/
class NodeProvisioningController {
  public:
    NodeProvisioningController();

    // Aufgabe: Initialisiert den Controller und laedt gespeicherte Einstellungen.
    // Eingabewerte: config, Zeiger auf Runtime-Werte, SSID-Puffer, Device-Handler und optionaler Logger.
    // Ausgabewert: true bei erfolgreicher Initialisierung, false bei fehlenden Pflichtwerten.
    bool begin(
        const NodeProvisioningConfig& config,
        bool* masterMacValid,
        uint8_t* masterMac,
        uint32_t* statusSendIntervalS,
        uint32_t* sensorSendIntervalS,
        bool* setupMode,
        bool* setupApActive,
        bool* restartPending,
        unsigned long* restartRequestedAtMs,
        char* setupApSsid,
        size_t setupApSsidSize,
        DeviceProvisioningHandler* deviceHandler,
        SetupLogFn logFn = nullptr);

    // Aufgabe: Muss regelmaessig im loop() laufen.
    // Eingabewert: keiner.
    // Ausgabewert: keiner; verarbeitet Taster, Webserver, LED und Neustart.
    void update();

    // Aufgabe: Setzt die gemeinsamen Basiswerte auf die Standardwerte aus der Konfiguration.
    // Eingabewert: keiner.
    // Ausgabewert: keiner.
    void applyDefaultBasisValues();

    // Aufgabe: Sichert aktuelle Basiswerte in einen Snapshot.
    // Eingabewert: snapshot wird beschrieben.
    // Ausgabewert: keiner.
    void captureBasisSnapshot(NodeBasisSnapshot& snapshot) const;

    // Aufgabe: Stellt Basiswerte aus einem Snapshot wieder her.
    // Eingabewert: snapshot mit dem alten Zustand.
    // Ausgabewert: keiner.
    void restoreBasisSnapshot(const NodeBasisSnapshot& snapshot);

    // Aufgabe: Startet den Setup-Modus mit Access Point und Webserver.
    // Eingabewert: keiner.
    // Ausgabewert: keiner.
    void enterSetupMode();

    // Aufgabe: Beendet den Setup-Modus und schaltet den Access Point aus.
    // Eingabewert: reason ist ein kurzer Text fuer das Log.
    // Ausgabewert: keiner.
    void exitSetupMode(const char* reason);

    // Aufgabe: Speichert aktuelle Basis- und Geraetewerte dauerhaft.
    // Eingabewert: keiner.
    // Ausgabewert: true bei erfolgreichem Speichern.
    bool saveCurrentState();

    // Aufgabe: Loescht gespeicherte Basis- und Geraetewerte.
    // Eingabewert: keiner.
    // Ausgabewert: true bei erfolgreichem Loeschen.
    bool clearStoredSettings();

    // Aufgabe: Meldet, ob der Setup-Modus gerade aktiv ist.
    // Eingabewert: keiner.
    // Ausgabewert: true wenn das Setup-Portal laeuft.
    bool isSetupModeActive() const;

    // Aufgabe: Meldet, ob eine Master-MAC gespeichert ist.
    // Eingabewert: keiner.
    // Ausgabewert: true wenn eine gueltige Master-Bindung existiert.
    bool hasStoredMasterMac() const;

    // Aufgabe: Gibt die gespeicherte Master-MAC als lesbaren Text zurueck.
    // Eingabewert: keiner.
    // Ausgabewert: Text im Format AA:BB:CC:DD:EE:FF oder leerer Text.
    String buildStoredMasterMacText() const;

    // Aufgabe: Prueft ein Sendeintervall gegen den allgemeinen erlaubten Bereich.
    // Eingabewert: valueS in Sekunden.
    // Ausgabewert: true wenn der Wert erlaubt ist.
    bool isSendIntervalValid(uint32_t valueS) const;

    // Aufgabe: Korrigiert ein Statusintervall auf einen gueltigen Wert.
    // Eingabewert: valueS in Sekunden.
    // Ausgabewert: gueltiger Sekundenwert, sonst der konfigurierte Standard.
    uint32_t sanitizeStatusSendInterval(uint32_t valueS) const;

    // Aufgabe: Korrigiert ein Sensorintervall auf einen gueltigen Wert.
    // Eingabewert: valueS in Sekunden.
    // Ausgabewert: gueltiger Sekundenwert, sonst der konfigurierte Standard.
    uint32_t sanitizeSensorSendInterval(uint32_t valueS) const;

    // Aufgabe: Wandelt einen MAC-Text in 6 Bytes um.
    // Eingabewert: text im Format AA:BB:CC:DD:EE:FF.
    // Ausgabewert: true bei Erfolg; outMac enthaelt dann 6 Bytes.
    static bool parseMacText(const char* text, uint8_t outMac[6]);

    // Aufgabe: Wandelt 6 MAC-Bytes in lesbaren Text um.
    // Eingabewerte: mac, Gueltigkeitsflag, Zielpuffer und Puffergroesse.
    // Ausgabewert: keiner; buffer enthaelt danach den MAC-Text oder einen leeren Text.
    static void formatMacText(
        const uint8_t* mac,
        bool isValid,
        char* buffer,
        size_t bufferSize);

  private:
    // Aufgabe: Registriert die HTTP-Wege fuer Startseite, Speichern und Fehlerfaelle.
    // Eingabewert: keiner.
    // Ausgabewert: keiner.
    void configureRoutes();

    // Aufgabe: Verarbeitet HTTP-GET auf der Startseite des Setup-Portals.
    // Eingabewert: keiner; die Daten kommen aus dem internen WebServer.
    // Ausgabewert: keiner; sendet eine Webseite an den Browser.
    void handleRoot();

    // Aufgabe: Verarbeitet HTTP-POST vom Speicherformular.
    // Eingabewert: keiner; die Formulardaten kommen aus dem internen WebServer.
    // Ausgabewert: keiner; sendet Erfolgs- oder Fehlerseite.
    void handleSave();

    // Aufgabe: Baut und sendet das Setup-Formular.
    // Eingabewerte: vorausgefuellte Texte, Info-/Fehlertext, HTTP-Status und optionaler Quellserver.
    // Ausgabewert: keiner; Antwort geht direkt an den Browser.
    void sendForm(
        const String& masterMacText,
        const String& statusIntervalText,
        const String& sensorIntervalText,
        const String& infoText,
        const String& errorText,
        int statusCode,
        WebServer* sourceServer);

    // Aufgabe: Baut die komplette HTML-Seite des Setup-Formulars.
    // Eingabewerte: vorausgefuellte Texte, Info-/Fehlertext und optionaler Quellserver.
    // Ausgabewert: fertiger HTML-Text.
    String buildPage(
        const String& masterMacText,
        const String& statusIntervalText,
        const String& sensorIntervalText,
        const String& infoText,
        const String& errorText,
        WebServer* sourceServer) const;

    // Aufgabe: Haengt die gemeinsamen CSS-Regeln an eine HTML-Seite an.
    // Eingabewert: page wird erweitert.
    // Ausgabewert: keiner.
    void appendSharedStyles(String& page) const;

    // Aufgabe: Sendet eine einfache Ergebnis-Seite nach Speichern oder Fehler.
    // Eingabewerte: Titel, Nachricht, Fehlerflag, HTTP-Status und Zurueck-Button-Flag.
    // Ausgabewert: keiner.
    void sendResultPage(
        const String& titleText,
        const String& messageText,
        bool isError,
        int statusCode,
        bool showBackButton);

    // Aufgabe: Baut die HTML-Ergebnis-Seite.
    // Eingabewerte: Titel, Nachricht, Fehlerflag und Zurueck-Button-Flag.
    // Ausgabewert: fertiger HTML-Text.
    String buildResultPage(
        const String& titleText,
        const String& messageText,
        bool isError,
        bool showBackButton) const;

    // Aufgabe: Laedt gemeinsame Basisdaten aus Preferences.
    // Eingabewert: prefs ist der geoeffnete Flash-Speicher.
    // Ausgabewert: true wenn Daten gefunden und angewendet wurden.
    bool loadBasisFromStorage(Preferences& prefs);

    // Aufgabe: Schreibt die aktuellen Basisdaten in Preferences.
    // Eingabewert: prefs ist der geoeffnete Flash-Speicher.
    // Ausgabewert: true bei erfolgreichem Speichern.
    bool writeBasisToStorage(Preferences& prefs) const;

    // Aufgabe: Schreibt uebergebene Basisdaten in Preferences.
    // Eingabewerte: prefs ist der geoeffnete Flash-Speicher, settings sind die zu speichernden Daten.
    // Ausgabewert: true bei erfolgreichem Speichern.
    bool writeBasisToStorage(Preferences& prefs, const NodeBasisSettings& settings) const;

    // Aufgabe: Entfernt die Basisdaten aus Preferences.
    // Eingabewert: prefs ist der geoeffnete Flash-Speicher.
    // Ausgabewert: true wenn der Loeschaufruf ausgefuehrt wurde.
    bool removeBasisFromStorage(Preferences& prefs) const;

    // Aufgabe: Liest den rohen Basisdatenblock aus Preferences.
    // Eingabewerte: prefs ist der Speicher, outSettings wird beschrieben.
    // Ausgabewert: true wenn Groesse, Magic und Version stimmen.
    bool readBasisBlob(Preferences& prefs, NodeBasisSettings& outSettings) const;

    // Aufgabe: Uebernimmt geladene Basisdaten in die Runtime-Zeiger.
    // Eingabewert: settings mit gueltigen Basisdaten.
    // Ausgabewert: keiner.
    void applyBasisSettings(const NodeBasisSettings& settings);

    // Aufgabe: Baut aus den aktuellen Runtime-Werten einen speicherbaren Basisdatenblock.
    // Eingabewert: keiner.
    // Ausgabewert: gefuellte NodeBasisSettings-Struktur.
    NodeBasisSettings buildBasisSettings() const;

    // Aufgabe: Liest eine Master-MAC aus Formular oder URL-Query.
    // Eingabewerte: outValue und outSourceArg werden beschrieben.
    // Ausgabewert: true wenn ein passender Wert vorhanden war.
    bool readRequestedMasterMac(String& outValue, const char*& outSourceArg);

    // Aufgabe: Wandelt einen Zahlentext in uint32_t um und prueft den erlaubten Bereich.
    // Eingabewerte: value als Text, minValue, maxValue und outValue als Ausgabe.
    // Ausgabewert: true wenn der Text eine gueltige Zahl im Bereich enthaelt.
    bool parseUnsignedLongText(
        String value,
        uint32_t minValue,
        uint32_t maxValue,
        uint32_t& outValue) const;

    // Aufgabe: Markiert die gespeicherte Master-MAC als ungueltig und setzt die Bytes auf 0.
    // Eingabewert: keiner.
    // Ausgabewert: keiner.
    void clearStoredMasterMac();

    // Aufgabe: Setzt oder loescht die gespeicherte Master-MAC.
    // Eingabewert: masterMac zeigt auf 6 Bytes oder ist nullptr zum Loeschen.
    // Ausgabewert: keiner.
    void setStoredMasterMac(const uint8_t masterMac[6]);

    // Aufgabe: Schreibt eine formatierte Logmeldung ueber den optionalen Log-Callback.
    // Eingabewerte: level, Formattext und optionale Formatwerte.
    // Ausgabewert: keiner.
    void log(const char* level, const char* format, ...) const;

    // Aufgabe: Richtet Setup-Taster und Setup-LED als GPIO ein.
    // Eingabewert: keiner; nutzt die Pinwerte aus config_.
    // Ausgabewert: keiner.
    void initializeSetupIo();

    // Aufgabe: Prueft Setup-Taster und startet bei langem Druck den Setup-Modus.
    // Eingabewert: keiner; liest den konfigurierten GPIO.
    // Ausgabewert: keiner.
    void updateSetupButton();

    // Aufgabe: Aktualisiert die Setup-LED.
    // Eingabewert: keiner; nutzt millis() und die Blinkzeit in Millisekunden.
    // Ausgabewert: keiner.
    void updateSetupIndicator();

    // Aufgabe: Schaltet die Setup-LED passend zur activeHigh-Einstellung.
    // Eingabewert: active bedeutet LED logisch an oder aus.
    // Ausgabewert: keiner.
    void writeSetupIndicator(bool active);

    // Aufgabe: Prueft, ob ein Setup-Taster konfiguriert ist.
    // Eingabewert: keiner.
    // Ausgabewert: true wenn ein Pin eingetragen ist.
    bool setupButtonConfigured() const;

    // Aufgabe: Prueft, ob eine Setup-LED konfiguriert ist.
    // Eingabewert: keiner.
    // Ausgabewert: true wenn ein Pin eingetragen ist.
    bool setupIndicatorConfigured() const;

    // Aufgabe: Liefert den Formularnamen fuer das Statusintervall.
    // Eingabewert: keiner.
    // Ausgabewert: konfigurierter Name oder Standardname.
    const char* statusIntervalArgName() const;

    // Aufgabe: Liefert den Formularnamen fuer das Sensorintervall.
    // Eingabewert: keiner.
    // Ausgabewert: konfigurierter Name oder Standardname.
    const char* sensorIntervalArgName() const;

    // Aufgabe: Liefert das sichtbare Label fuer das Statusintervall.
    // Eingabewert: keiner.
    // Ausgabewert: konfiguriertes Label oder Formularname.
    const char* statusIntervalLabel() const;

    // Aufgabe: Liefert das sichtbare Label fuer das Sensorintervall.
    // Eingabewert: keiner.
    // Ausgabewert: konfiguriertes Label oder Formularname.
    const char* sensorIntervalLabel() const;

    // Aufgabe: Liefert den Hilfetext fuer das Statusintervall.
    // Eingabewert: keiner.
    // Ausgabewert: konfigurierter Hilfetext oder Standardtext.
    const char* statusIntervalHint() const;

    // Aufgabe: Liefert den Hilfetext fuer das Sensorintervall.
    // Eingabewert: keiner.
    // Ausgabewert: konfigurierter Hilfetext oder Standardtext.
    const char* sensorIntervalHint() const;

    // Aufgabe: Liefert die untere Grenze fuer das Sensorintervall.
    // Eingabewert: keiner.
    // Ausgabewert: Sekundenwert aus Sensorgrenze oder allgemeiner Grenze.
    uint32_t effectiveMinSensorSendIntervalS() const;

    // Aufgabe: Liefert die obere Grenze fuer das Sensorintervall.
    // Eingabewert: keiner.
    // Ausgabewert: Sekundenwert aus Sensorgrenze oder allgemeiner Grenze.
    uint32_t effectiveMaxSensorSendIntervalS() const;

    NodeProvisioningConfig config_;
    // Runtime-Zeiger:
    // Der Controller ist eine gemeinsame Bibliothek und besitzt diese Werte
    // nicht selbst. Er schreibt direkt in Variablen des konkreten Nodes, damit
    // bestehende Firmware-Pfade sofort dieselben Einstellungen sehen.
    bool* masterMacValid_;
    uint8_t* masterMac_;
    uint32_t* statusSendIntervalS_;
    uint32_t* sensorSendIntervalS_;
    bool* setupMode_;
    bool* setupApActive_;
    bool* restartPending_;
    unsigned long* restartRequestedAtMs_;
    char* setupApSsid_;
    size_t setupApSsidSize_;
    DeviceProvisioningHandler* deviceHandler_; // Polymorphe Grenze zum geraetespezifischen Setup
    SetupLogFn logFn_;                         // Optionaler Logger-Callback, darf nullptr sein
    WebServer server_;
    bool routesConfigured_;
    bool initialized_;
    bool setupButtonLastActive_;
    bool setupButtonHoldConsumed_;
    unsigned long setupButtonPressedAtMs_;
    bool setupIndicatorState_;
    unsigned long setupIndicatorLastBlinkMs_;
};

}  // namespace ShNodeProvisioning
}  // namespace SmartHome
