#include "ShNodeProvisioning.h"

#include <WiFi.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace SmartHome {
namespace ShNodeProvisioning {

namespace {

constexpr uint32_t NODE_BASIS_MAGIC = 0x4E505331UL;
constexpr uint16_t NODE_BASIS_VERSION = 1U;
constexpr uint32_t NODE_BASIS_FLAG_MASTER_MAC = 0x00000001UL;
constexpr const char* MASTER_MAC_ARG_PRIMARY = "master_mac";
constexpr const char* MASTER_MAC_ARG_ALIAS = "mac";
constexpr const char* STATUS_INTERVAL_ARG_DEFAULT = "status_send_interval_s";
constexpr const char* SENSOR_INTERVAL_ARG_DEFAULT = "sensor_send_interval_s";
constexpr const char* STATUS_INTERVAL_HINT_DEFAULT = "Statusintervall in Sekunden.";
constexpr const char* SENSOR_INTERVAL_HINT_DEFAULT = "Sensorintervall in Sekunden.";
constexpr unsigned long SETUP_BUTTON_HOLD_MS_DEFAULT = 5000UL;
constexpr unsigned long SETUP_INDICATOR_BLINK_MS_DEFAULT = 500UL;

// htmlEscape – HTML-Sonderzeichen maskieren (& < > " ') fuer sichere Ausgabe
String htmlEscape(const String& text) {
    String escaped;
    escaped.reserve(text.length() + 16U);

    for (size_t i = 0U; i < text.length(); ++i) {
        const char current = text[i];
        switch (current) {
            case '&': escaped += F("&amp;"); break;
            case '<': escaped += F("&lt;"); break;
            case '>': escaped += F("&gt;"); break;
            case '"': escaped += F("&quot;"); break;
            case '\'': escaped += F("&#39;"); break;
            default: escaped += current; break;
        }
    }

    return escaped;
}

// basisBlobValid – Prueft Magic und Version eines geladenen NodeBasisSettings-Blobs
bool basisBlobValid(const NodeBasisSettings& settings) {
    return settings.magic == NODE_BASIS_MAGIC && settings.version == NODE_BASIS_VERSION;
}

// basisSettingsEqual – Vergleich zweier NodeBasisSettings (alle Felder ausser Magic/Version)
bool basisSettingsEqual(const NodeBasisSettings& left, const NodeBasisSettings& right) {
    return left.magic == right.magic && left.version == right.version &&
           left.flags == right.flags && memcmp(left.masterMac, right.masterMac, sizeof(left.masterMac)) == 0 &&
           left.statusSendIntervalS == right.statusSendIntervalS &&
           left.sensorSendIntervalS == right.sensorSendIntervalS;
}

// fallbackText – Gibt text zurueck, falls nicht leer, sonst fallback
const char* fallbackText(const char* text, const char* fallback) {
    return (text != nullptr && text[0] != '\0') ? text : fallback;
}

}  // namespace

NodeProvisioningController::NodeProvisioningController()
    : config_{},
      masterMacValid_(nullptr),
      masterMac_(nullptr),
      statusSendIntervalS_(nullptr),
      sensorSendIntervalS_(nullptr),
      setupMode_(nullptr),
      setupApActive_(nullptr),
      restartPending_(nullptr),
      restartRequestedAtMs_(nullptr),
      setupApSsid_(nullptr),
      setupApSsidSize_(0U),
      deviceHandler_(nullptr),
      logFn_(nullptr),
      server_(80),
      routesConfigured_(false),
      initialized_(false),
      setupButtonLastActive_(false),
      setupButtonHoldConsumed_(false),
      setupButtonPressedAtMs_(0UL),
      setupIndicatorState_(false),
      setupIndicatorLastBlinkMs_(0UL) {}

// begin – Initialisiert das Provisioning: Pointer setzen, I/O konfigurieren,
//   Preferences oeffnen, persistierte Daten laden. Bei fehlenden/noch nicht
//   vorhandenen Preferences werden Defaults aktiviert.
//   Rueckgabe: true bei Erfolg, false bei NULL-Pointern oder ungueltigen Parametern.
bool NodeProvisioningController::begin(
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
    SetupLogFn logFn) {
    if (masterMacValid == nullptr || masterMac == nullptr || statusSendIntervalS == nullptr ||
        sensorSendIntervalS == nullptr || setupMode == nullptr || setupApActive == nullptr ||
        restartPending == nullptr || restartRequestedAtMs == nullptr || setupApSsid == nullptr ||
        setupApSsidSize == 0U || deviceHandler == nullptr) {
        return false;
    }

    config_ = config;
    masterMacValid_ = masterMacValid;
    masterMac_ = masterMac;
    statusSendIntervalS_ = statusSendIntervalS;
    sensorSendIntervalS_ = sensorSendIntervalS;
    setupMode_ = setupMode;
    setupApActive_ = setupApActive;
    restartPending_ = restartPending;
    restartRequestedAtMs_ = restartRequestedAtMs;
    setupApSsid_ = setupApSsid;
    setupApSsidSize_ = setupApSsidSize;
    deviceHandler_ = deviceHandler;
    logFn_ = logFn;

    initialized_ = true;
    routesConfigured_ = false;
    *setupMode_ = false;
    *setupApActive_ = false;
    *restartPending_ = false;
    *restartRequestedAtMs_ = 0UL;
    setupApSsid_[0] = '\0';
    setupButtonLastActive_ = false;
    setupButtonHoldConsumed_ = false;
    setupButtonPressedAtMs_ = 0UL;
    setupIndicatorState_ = false;
    setupIndicatorLastBlinkMs_ = 0UL;

    initializeSetupIo();

    applyDefaultBasisValues();
    deviceHandler_->loadDeviceDefaults();

    Preferences prefs;
    if (prefs.begin(config_.storageNamespace, true)) {
        const bool basisLoaded = loadBasisFromStorage(prefs);
        const bool deviceLoaded = deviceHandler_->loadDeviceSettings(prefs);
        prefs.end();

        if (basisLoaded) log("INFO", "Gemeinsame Node-Basis geladen.");
        else log("INFO", "Keine persistierte Node-Basis gefunden. Defaults aktiv.");

        if (deviceLoaded) log("INFO", "Geraetespezifische Setup-Daten geladen.");
        else log("INFO", "Keine geraetespezifischen Setup-Daten gefunden. Defaults aktiv.");
    } else {
        log("WARN", "Preferences konnten fuer Node-Provisioning nicht geoeffnet werden.");
    }

    *statusSendIntervalS_ = sanitizeStatusSendInterval(*statusSendIntervalS_);
    *sensorSendIntervalS_ = sanitizeSensorSendInterval(*sensorSendIntervalS_);
    return true;
}

// applyDefaultBasisValues – Setzt Master-MAC zurueck und schreibt Default-Intervalle aus config
void NodeProvisioningController::applyDefaultBasisValues() {
    clearStoredMasterMac();
    *statusSendIntervalS_ = config_.defaultStatusSendIntervalS;
    *sensorSendIntervalS_ = config_.defaultSensorSendIntervalS;
}

// captureBasisSnapshot – Aktuelle Basis-Einstellungen in Snapshot sichern (fuer Rollback)
void NodeProvisioningController::captureBasisSnapshot(NodeBasisSnapshot& snapshot) const {
    snapshot.masterMacValid = *masterMacValid_;
    memcpy(snapshot.masterMac, masterMac_, 6U);
    snapshot.statusSendIntervalS = *statusSendIntervalS_;
    snapshot.sensorSendIntervalS = *sensorSendIntervalS_;
}

// restoreBasisSnapshot – Gesicherten Snapshot wiederherstellen (nach fehlgeschlagenem Save)
void NodeProvisioningController::restoreBasisSnapshot(const NodeBasisSnapshot& snapshot) {
    *masterMacValid_ = snapshot.masterMacValid;
    memcpy(masterMac_, snapshot.masterMac, 6U);
    *statusSendIntervalS_ = snapshot.statusSendIntervalS;
    *sensorSendIntervalS_ = snapshot.sensorSendIntervalS;
}

// isSendIntervalValid – Prueft ob Wert innerhalb der konfigurierten Grenzen (min/max)
bool NodeProvisioningController::isSendIntervalValid(uint32_t valueS) const {
    return valueS >= config_.minSendIntervalS && valueS <= config_.maxSendIntervalS;
}

// sanitizeStatusSendInterval – Wert auf gueltigen Bereich begrenzen, sonst Default
uint32_t NodeProvisioningController::sanitizeStatusSendInterval(uint32_t valueS) const {
    return isSendIntervalValid(valueS) ? valueS : config_.defaultStatusSendIntervalS;
}

// sanitizeSensorSendInterval – Sensor-Intervall mit ggf. abweichenden Sensor-Grenzen sanitizen
uint32_t NodeProvisioningController::sanitizeSensorSendInterval(uint32_t valueS) const {
    return valueS >= effectiveMinSensorSendIntervalS() && valueS <= effectiveMaxSensorSendIntervalS()
               ? valueS
               : config_.defaultSensorSendIntervalS;
}

// effectiveMinSensorSendIntervalS – Minimales Sensor-Intervall (falls 0, wird minSendIntervalS genutzt)
uint32_t NodeProvisioningController::effectiveMinSensorSendIntervalS() const {
    return config_.minSensorSendIntervalS > 0UL ? config_.minSensorSendIntervalS : config_.minSendIntervalS;
}

// effectiveMaxSensorSendIntervalS – Maximales Sensor-Intervall (falls 0, wird maxSendIntervalS genutzt)
uint32_t NodeProvisioningController::effectiveMaxSensorSendIntervalS() const {
    return config_.maxSensorSendIntervalS > 0UL ? config_.maxSensorSendIntervalS : config_.maxSendIntervalS;
}

// parseMacText – Text "AA:BB:CC:DD:EE:FF" in 6-Byte-MAC parsen (mit Trim/Hex-Parsing)
//   Rueckgabe: true bei erfolgreichem Parsen
bool NodeProvisioningController::parseMacText(const char* text, uint8_t outMac[6]) {
    if (text == nullptr || outMac == nullptr) return false;

    size_t length = strlen(text);
    while (length > 0U && (text[length - 1U] == ' ' || text[length - 1U] == '\t')) {
        --length;
    }

    size_t startIndex = 0U;
    while (startIndex < length && (text[startIndex] == ' ' || text[startIndex] == '\t')) {
        ++startIndex;
    }

    if ((length - startIndex) != 17U) {
        return false;
    }

    auto parseHexNibble = [](char ch, uint8_t& outValue) -> bool {
        if (ch >= '0' && ch <= '9') {
            outValue = (uint8_t)(ch - '0');
            return true;
        }
        if (ch >= 'A' && ch <= 'F') {
            outValue = (uint8_t)(10 + (ch - 'A'));
            return true;
        }
        if (ch >= 'a' && ch <= 'f') {
            outValue = (uint8_t)(10 + (ch - 'a'));
            return true;
        }
        return false;
    };

    for (size_t index = 0U; index < 6U; ++index) {
        const size_t offset = startIndex + (index * 3U);
        uint8_t high = 0U;
        uint8_t low = 0U;
        if (!parseHexNibble(text[offset], high) || !parseHexNibble(text[offset + 1U], low)) {
            return false;
        }
        if (index < 5U && text[offset + 2U] != ':') {
            return false;
        }
        outMac[index] = (uint8_t)((high << 4U) | low);
    }

    return true;
}

// formatMacText – 6-Byte-MAC als "AA:BB:CC:DD:EE:FF" formatieren (nur wenn gueltig)
void NodeProvisioningController::formatMacText(
    const uint8_t* mac,
    bool isValid,
    char* buffer,
    size_t bufferSize) {
    if (buffer == nullptr || bufferSize == 0U) return;
    if (!isValid || mac == nullptr || bufferSize < MASTER_MAC_TEXT_LEN) {
        buffer[0] = '\0';
        return;
    }

    snprintf(
        buffer,
        bufferSize,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);
}

// hasStoredMasterMac – Zeigt ob eine gueltige Master-MAC gespeichert ist
bool NodeProvisioningController::hasStoredMasterMac() const {
    return masterMacValid_ != nullptr && *masterMacValid_;
}

// buildStoredMasterMacText – Gespeicherte MAC als Text ausgeben (leer wenn ungueltig)
String NodeProvisioningController::buildStoredMasterMacText() const {
    char buffer[MASTER_MAC_TEXT_LEN] = {0};
    formatMacText(masterMac_, hasStoredMasterMac(), buffer, sizeof(buffer));
    return String(buffer);
}

// clearStoredMasterMac – Master-MAC zuruecksetzen (ungueltig + nullen)
void NodeProvisioningController::clearStoredMasterMac() {
    *masterMacValid_ = false;
    memset(masterMac_, 0, 6U);
}

// setStoredMasterMac – Master-MAC setzen (bei nullptr -> clear)
void NodeProvisioningController::setStoredMasterMac(const uint8_t masterMac[6]) {
    if (masterMac == nullptr) {
        clearStoredMasterMac();
        return;
    }
    *masterMacValid_ = true;
    memcpy(masterMac_, masterMac, 6U);
}

// configureRoutes – HTTP-Routen registrieren (Root + Save + NotFound)
void NodeProvisioningController::configureRoutes() {
    if (routesConfigured_) return;

    server_.on("/", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/save", HTTP_POST, [this]() { handleSave(); });
    server_.onNotFound([this]() { handleRoot(); });
    routesConfigured_ = true;
}

// enterSetupMode – Setup-AP starten: WiFi auf NULL -> AP, SSID/Passwort setzen,
//   HTTP-Server starten. Bei Fehler: Aufraeumen und zuruecksetzen.
//   Hinweis: Die delay(25) zwischen WIFI_MODE_NULL und WIFI_AP ist erforderlich,
//   weil der WiFi-Stack einen stabilen Zustand braucht.
//   Bei AP-Start-Fehler: kompletter Rueckbau + Warn-Log.
void NodeProvisioningController::enterSetupMode() {
    if (!initialized_ || *setupMode_) return;

    configureRoutes();
    *restartPending_ = false;

    if (config_.setupApSsid == nullptr || config_.setupApSsid[0] == '\0' ||
        strlen(config_.setupApSsid) >= setupApSsidSize_) {
        log("WARN", "Setup-SSID fehlt oder passt nicht in den SSID-Puffer.");
        return;
    }

    const char* setupPassword =
        config_.setupApPassword != nullptr && config_.setupApPassword[0] != '\0'
            ? config_.setupApPassword
            : nullptr;
    strncpy(setupApSsid_, config_.setupApSsid, setupApSsidSize_ - 1U);
    setupApSsid_[setupApSsidSize_ - 1U] = '\0';

    WiFi.persistent(false);
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_MODE_NULL);
    delay(25);
    WiFi.mode(WIFI_AP);

    if (!WiFi.softAP(setupApSsid_, setupPassword, config_.apChannel)) {
        *setupMode_ = false;
        *setupApActive_ = false;
        setupApSsid_[0] = '\0';
        WiFi.softAPdisconnect(true);
        WiFi.disconnect(true, true);
        WiFi.mode(WIFI_MODE_NULL);
        log("WARN", "Setup-AP konnte nicht gestartet werden.");
        return;
    }

    server_.begin();
    *setupMode_ = true;
    *setupApActive_ = true;
    setupIndicatorLastBlinkMs_ = 0UL;
    log(
        "INFO",
        "Setup-AP aktiv: ssid=%s password=%s ip=%s url=http://%s/",
        setupApSsid_,
        setupPassword ? setupPassword : "open",
        WiFi.softAPIP().toString().c_str(),
        WiFi.softAPIP().toString().c_str());
}

// exitSetupMode – Setup-Modus beenden: AP stoppen, WiFi-Reset, I/O zuruecksetzen
void NodeProvisioningController::exitSetupMode(const char* reason) {
    if (!initialized_) return;

    *setupMode_ = false;
    *restartPending_ = false;
    *restartRequestedAtMs_ = 0UL;

    if (*setupApActive_) {
        server_.stop();
        WiFi.softAPdisconnect(true);
        WiFi.disconnect(true, true);
        WiFi.mode(WIFI_MODE_NULL);
        *setupApActive_ = false;
    }

    setupApSsid_[0] = '\0';
    writeSetupIndicator(false);
    log("INFO", "Setup-Modus beendet (%s)", reason ? reason : "ohne grund");
}

// readRequestedMasterMac – Liest Master-MAC aus HTTP-Query (zwei moegliche Arg-Namen)
bool NodeProvisioningController::readRequestedMasterMac(String& outValue, const char*& outSourceArg) {
    if (server_.hasArg(MASTER_MAC_ARG_PRIMARY)) {
        outValue = server_.arg(MASTER_MAC_ARG_PRIMARY);
        outSourceArg = MASTER_MAC_ARG_PRIMARY;
        return true;
    }

    if (server_.hasArg(MASTER_MAC_ARG_ALIAS)) {
        outValue = server_.arg(MASTER_MAC_ARG_ALIAS);
        outSourceArg = MASTER_MAC_ARG_ALIAS;
        return true;
    }

    outValue = String();
    outSourceArg = nullptr;
    return false;
}

// parseUnsignedLongText – Text in uint32 parsen mit Bereichspruefung und Overflow-Schutz
//   Rueckgabe: true bei Erfolg, outValue ist dann gesetzt
bool NodeProvisioningController::parseUnsignedLongText(
    String value,
    uint32_t minValue,
    uint32_t maxValue,
    uint32_t& outValue) const {
    value.trim();
    if (value.length() == 0U) {
        return false;
    }

    uint32_t parsed = 0UL;
    for (size_t i = 0U; i < value.length(); ++i) {
        const char current = value[i];
        if (current < '0' || current > '9') {
            return false;
        }

        const uint32_t digit = (uint32_t)(current - '0');
        if (parsed > ((0xFFFFFFFFUL - digit) / 10UL)) {
            return false;
        }
        parsed = (parsed * 10UL) + digit;
    }

    if (parsed < minValue || parsed > maxValue) {
        return false;
    }

    outValue = parsed;
    return true;
}

// appendSharedStyles – CSS-Stylesheet fuer Setup-Weboberflaeche anhaengen (Dark-Theme)
void NodeProvisioningController::appendSharedStyles(String& page) const {
    page += F(":root{--bg:#070b14;--bg2:#0b1220;--card:#101827;--card2:#0c1320;--text:#edf3ff;--muted:#8ea0bf;--line:#1e2c45;--accent:#35c486;--accent2:#1d8a61;--danger:#ff6b6b;--danger2:#c94949;--ok:#91f0c5;--error:#ffb1b1;}");
    page += F("*{box-sizing:border-box}html,body{margin:0;padding:0;min-height:100%;background:radial-gradient(circle at top,#15233d 0%,var(--bg) 56%,#04060d 100%);color:var(--text);font-family:\"Segoe UI\",Tahoma,sans-serif}");
    page += F("body{padding:14px}.wrap{max-width:460px;margin:0 auto}.stack{display:grid;gap:12px}.card{background:linear-gradient(180deg,rgba(20,29,45,.96) 0%,rgba(12,19,32,.98) 100%);border:1px solid var(--line);border-radius:18px;padding:16px;box-shadow:0 18px 48px rgba(0,0,0,.34)}");
    page += F(".eyebrow{font-size:.74rem;letter-spacing:.12em;text-transform:uppercase;color:var(--muted);margin-bottom:6px}.sub{margin:6px 0 0;color:var(--muted);font-size:.83rem;line-height:1.4}.status{display:grid;gap:4px;margin:14px 0 0;padding:11px 12px;border-radius:14px;border:1px solid #1f3a34;background:rgba(16,44,37,.9);color:var(--ok);font-size:.84rem;line-height:1.35}.status.error{border-color:#4d2428;background:rgba(60,19,22,.88);color:var(--error)}");
    page += F(".status strong,.status code{color:var(--text)}h1{margin:0;font-size:1.28rem;line-height:1.2}h2{margin:0;font-size:.96rem}.section{display:grid;gap:12px;margin-top:16px}.section-head{display:flex;align-items:center;justify-content:space-between;gap:10px}.tag{display:inline-flex;align-items:center;padding:3px 8px;border-radius:999px;border:1px solid var(--line);background:rgba(255,255,255,.03);color:var(--muted);font-size:.72rem;text-transform:uppercase;letter-spacing:.08em}");
    page += F(".field{display:grid;gap:6px}label{font-weight:700;font-size:.88rem;color:#d9e4f8}.hint{font-size:.76rem;line-height:1.3;color:var(--muted)}input,select{width:100%;min-height:44px;border-radius:12px;border:1px solid var(--line);background:#0a111d;color:var(--text);padding:0 12px;font-size:.96rem}input::placeholder{color:#617393}hr{border:0;border-top:1px solid var(--line);margin:16px 0 0}");
    page += F(".actions{display:grid;gap:10px;margin-top:16px}.btn,.linkbtn{display:flex;align-items:center;justify-content:center;min-height:46px;padding:0 14px;border-radius:12px;border:1px solid transparent;font-size:.95rem;font-weight:700;text-decoration:none}.btn{width:100%;cursor:pointer}.btn-primary{background:linear-gradient(180deg,var(--accent) 0%,var(--accent2) 100%);color:#06140f}.btn-danger{background:linear-gradient(180deg,var(--danger) 0%,var(--danger2) 100%);color:#fff}.btn-secondary,.linkbtn{background:transparent;border-color:var(--line);color:var(--text)}");
    page += F(".meta{display:grid;gap:4px;margin-top:10px;font-size:.78rem;color:var(--muted)}.meta code{color:var(--text)}.footer{margin-top:2px;font-size:.75rem;color:var(--muted)}");
}

// buildPage – Komplette Setup-HTML-Seite bauen (Formular mit Node-Basis + Device-Bereich)
//   Baut aus escaped-Templates eine vollstaendige HTML-Seite mit CSS + JS-Validierung.
//   Aufteilung: Kopf -> Status -> Node-Basis-Felder -> Device-Felder -> Footer.
String NodeProvisioningController::buildPage(
    const String& masterMacText,
    const String& statusIntervalText,
    const String& sensorIntervalText,
    const String& infoText,
    const String& errorText,
    WebServer* sourceServer) const {
    String page;
    page.reserve(9200U);

    const String escapedTitle = htmlEscape(String(deviceHandler_->pageTitle()));
    const String escapedIntro = htmlEscape(String(deviceHandler_->pageIntro()));
    const String escapedDeviceTitle = htmlEscape(String(deviceHandler_->deviceSectionTitle()));
    const String escapedDeviceIntro = htmlEscape(String(deviceHandler_->deviceSectionIntro()));
    const String escapedMasterMac = htmlEscape(masterMacText);
    const String escapedStatusInterval = htmlEscape(statusIntervalText);
    const String escapedSensorInterval = htmlEscape(sensorIntervalText);
    const String escapedInfo = htmlEscape(infoText);
    const String escapedError = htmlEscape(errorText);

    page += F("<!doctype html><html lang=\"de\"><head><meta charset=\"utf-8\">");
    page += F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,viewport-fit=cover\">");
    page += F("<title>");
    page += escapedTitle;
    page += F("</title><style>");
    appendSharedStyles(page);
    page += F("</style></head><body><div class=\"wrap stack\"><form class=\"card\" method=\"post\" action=\"/save\" id=\"setupForm\" novalidate>");
    page += F("<div class=\"eyebrow\">Provisioning</div><h1>");
    page += escapedTitle;
    page += F("</h1>");
    if (escapedIntro.length() > 0U) {
        page += F("<div class=\"sub\">");
        page += escapedIntro;
        page += F("</div>");
    }

    if (errorText.length() > 0U) {
        page += F("<div class=\"status error\">");
        page += escapedError;
        page += F("</div>");
    } else {
        page += F("<div class=\"status\">");
        page += escapedInfo;
        if (*setupApActive_) {
            page += F("<span>AP <strong>");
            page += htmlEscape(String(setupApSsid_));
            page += F("</strong></span><span>URL <code>http://");
            page += htmlEscape(WiFi.softAPIP().toString());
            page += F("/</code></span>");
        }
        page += F("</div>");
    }

    page += F("<section class=\"section\"><div class=\"section-head\"><h2>Node-Basis</h2><span class=\"tag\">global</span></div>");
    page += F("<div class=\"field\"><label for=\"master_mac\">master_mac</label>");
    page += F("<input id=\"master_mac\" name=\"master_mac\" type=\"text\" maxlength=\"17\" autocapitalize=\"characters\" autocomplete=\"off\" spellcheck=\"false\" placeholder=\"AA:BB:CC:DD:EE:FF\" value=\"");
    page += escapedMasterMac;
    page += F("\"><div class=\"hint\">Auch per <code>?master_mac=...</code> oder <code>?mac=...</code>.</div></div>");
    page += F("<div class=\"field\"><label for=\"");
    page += htmlEscape(String(statusIntervalArgName()));
    page += F("\">");
    page += htmlEscape(String(statusIntervalLabel()));
    page += F("</label>");
    page += F("<input id=\"");
    page += htmlEscape(String(statusIntervalArgName()));
    page += F("\" name=\"");
    page += htmlEscape(String(statusIntervalArgName()));
    page += F("\" type=\"number\" min=\"");
    page += String(config_.minSendIntervalS);
    page += F("\" max=\"");
    page += String(config_.maxSendIntervalS);
    page += F("\" step=\"1\" inputmode=\"numeric\" value=\"");
    page += escapedStatusInterval;
    page += F("\"><div class=\"hint\">");
    page += htmlEscape(String(statusIntervalHint()));
    page += F("</div></div>");
    page += F("<div class=\"field\"><label for=\"");
    page += htmlEscape(String(sensorIntervalArgName()));
    page += F("\">");
    page += htmlEscape(String(sensorIntervalLabel()));
    page += F("</label>");
    page += F("<input id=\"");
    page += htmlEscape(String(sensorIntervalArgName()));
    page += F("\" name=\"");
    page += htmlEscape(String(sensorIntervalArgName()));
    page += F("\" type=\"number\" min=\"");
    page += String(effectiveMinSensorSendIntervalS());
    page += F("\" max=\"");
    page += String(effectiveMaxSensorSendIntervalS());
    page += F("\" step=\"1\" inputmode=\"numeric\" value=\"");
    page += escapedSensorInterval;
    page += F("\"><div class=\"hint\">");
    page += htmlEscape(String(sensorIntervalHint()));
    page += F("</div></div>");
    page += F("</section><hr>");

    page += F("<section class=\"section\"><div class=\"section-head\"><h2>");
    page += escapedDeviceTitle;
    page += F("</h2><span class=\"tag\">lokal</span></div>");
    if (escapedDeviceIntro.length() > 0U) {
        page += F("<div class=\"sub\">");
        page += escapedDeviceIntro;
        page += F("</div>");
    }
    deviceHandler_->appendDeviceFieldsHtml(page, sourceServer);
    page += F("</section>");
    page += F("<div class=\"actions\"><button class=\"btn btn-primary\" id=\"saveBtn\" type=\"submit\">Speichern und neu starten</button>");
    page += F("<div class=\"footer\">Nur geaenderte Werte werden neu geschrieben.</div></div>");
    page += F("</form>");
    deviceHandler_->appendDeviceActionsHtml(page);
    page += F("</div>");
    page += F("<script>(function(){const form=document.getElementById('setupForm');const mac=document.getElementById('master_mac');");
    page += F("function norm(v){return v.trim().toUpperCase().replace(/-/g,':');}");
    page += F("function valid(v){return /^([0-9A-F]{2}:){5}[0-9A-F]{2}$/.test(v);}mac.addEventListener('blur',function(){mac.value=norm(mac.value);});");
    page += F("form.addEventListener('submit',function(e){mac.value=norm(mac.value);if(!valid(mac.value)){e.preventDefault();alert('Master-MAC ist ungueltig.');}});})();</script>");
    page += F("</body></html>");

    return page;
}

// sendForm – buildPage + HTTP-Response senden (mit Status-Code)
void NodeProvisioningController::sendForm(
    const String& masterMacText,
    const String& statusIntervalText,
    const String& sensorIntervalText,
    const String& infoText,
    const String& errorText,
    int statusCode,
    WebServer* sourceServer) {
    server_.send(
        statusCode,
        "text/html; charset=utf-8",
        buildPage(
            masterMacText,
            statusIntervalText,
            sensorIntervalText,
            infoText,
            errorText,
            sourceServer));
}

// buildResultPage – Einfache Ergebnis-HTML-Seite (Erfolg/Fehler + optionaler Zurueck-Button)
String NodeProvisioningController::buildResultPage(
    const String& titleText,
    const String& messageText,
    bool isError,
    bool showBackButton) const {
    String page;
    page.reserve(3800U);

    const String escapedTitle = htmlEscape(titleText);
    const String escapedMessage = htmlEscape(messageText);

    page += F("<!doctype html><html lang=\"de\"><head><meta charset=\"utf-8\">");
    page += F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,viewport-fit=cover\">");
    page += F("<title>");
    page += escapedTitle;
    page += F("</title><style>");
    appendSharedStyles(page);
    page += F("</style></head><body><div class=\"wrap\"><div class=\"card\">");
    page += F("<div class=\"eyebrow\">");
    page += isError ? F("Fehler") : F("Ergebnis");
    page += F("</div><h1>");
    page += escapedTitle;
    page += F("</h1><div class=\"status");
    if (isError) page += F(" error");
    page += F("\">");
    page += escapedMessage;
    page += F("</div>");

    if (showBackButton) {
        page += F("<div class=\"actions\">");
        page += F("<button class=\"btn btn-secondary\" type=\"button\" onclick=\"history.back()\">Zurueck zum Setup</button>");
        page += F("<a class=\"linkbtn\" href=\"/\">Setup neu laden</a></div>");
    }

    page += F("</div></div></body></html>");
    return page;
}

// sendResultPage – buildResultPage + HTTP-Response senden
void NodeProvisioningController::sendResultPage(
    const String& titleText,
    const String& messageText,
    bool isError,
    int statusCode,
    bool showBackButton) {
    server_.send(
        statusCode,
        "text/html; charset=utf-8",
        buildResultPage(titleText, messageText, isError, showBackButton));
}

// handleRoot – HTTP-GET / : Setup-Formular anzeigen
//   Bei Query-Parametern (master_mac, mac) wird die MAC vorausgefuellt.
//   Ungueltige MAC-Query -> Fehlermeldung im Formular.
void NodeProvisioningController::handleRoot() {
    String masterMacText = buildStoredMasterMacText();
    String infoText =
        *restartPending_ ? String(F("Setup-Daten gespeichert. Neustart laeuft gleich an."))
                         : String(F("Setup-Modus aktiv."));
    String errorText;

    String requestedMasterMac;
    const char* requestedArg = nullptr;
    if (readRequestedMasterMac(requestedMasterMac, requestedArg)) {
        uint8_t parsedMac[6] = {0};
        if (parseMacText(requestedMasterMac.c_str(), parsedMac)) {
            char normalizedMacText[MASTER_MAC_TEXT_LEN] = {0};
            formatMacText(parsedMac, true, normalizedMacText, sizeof(normalizedMacText));
            masterMacText = String(normalizedMacText);
            infoText = F("Master-MAC aus Query uebernommen.");
            log("INFO", "Master-MAC aus Query gelesen (%s): %s", requestedArg, masterMacText.c_str());
        } else {
            errorText = F("Die uebergebene Master-MAC ist ungueltig.");
            log("WARN", "Ungueltige Master-MAC aus Query verworfen (%s): %s", requestedArg, requestedMasterMac.c_str());
        }
    }

    sendForm(
        masterMacText,
        String(*statusSendIntervalS_),
        String(*sensorSendIntervalS_),
        infoText,
        errorText,
        200,
        nullptr);
}

// handleSave – HTTP-POST /save : Validieren + Speichern + Neustart
//   Pipeline: 1) MAC validieren 2) Status-Intervall validieren 3) Sensor-Intervall validieren
//             4) Device-Felder validieren 5) Snapshot (Basis + Device) 6) Neue Werte anwenden
//             7) Persistieren (saveCurrentState) 8) Bei Fehler: Rollback + Fehlerseite
//             9) Bei Erfolg: restartPending setzen + Erfolgsseite
//   Sonderfall: device_action (z.B. Reset) wird direkt an deviceHandler delegiert.
void NodeProvisioningController::handleSave() {
    if (server_.hasArg("device_action")) {
        deviceHandler_->discardParsedDeviceSettings();

        String actionTitle;
        String actionMessage;
        int actionStatusCode = 400;
        bool restartRequired = false;
        const bool actionOk = deviceHandler_->handleDeviceAction(
            server_,
            actionTitle,
            actionMessage,
            actionStatusCode,
            restartRequired);

        if (restartRequired) {
            *restartPending_ = true;
            *restartRequestedAtMs_ = millis();
        }

        sendResultPage(
            actionTitle.length() > 0U
                ? actionTitle
                : (actionOk ? String(F("Aktion gespeichert")) : String(F("Aktion fehlgeschlagen"))),
            actionMessage.length() > 0U
                ? actionMessage
                : (actionOk ? String(F("Aktion ausgefuehrt.")) : String(F("Aktion ist fehlgeschlagen."))),
            !actionOk,
            actionStatusCode,
            !restartRequired);
        return;
    }

    const String masterMacText = server_.arg(MASTER_MAC_ARG_PRIMARY);
    const String statusIntervalText = server_.arg(statusIntervalArgName());
    const String sensorIntervalText = server_.arg(sensorIntervalArgName());

    uint8_t parsedMasterMac[6] = {0};
    if (!parseMacText(masterMacText.c_str(), parsedMasterMac)) {
        deviceHandler_->discardParsedDeviceSettings();
        sendResultPage(
            F("Eingabe ungueltig"),
            F("master_mac ist ungueltig. Erwartet wird AA:BB:CC:DD:EE:FF."),
            true,
            400,
            true);
        return;
    }

    uint32_t statusIntervalS = 0UL;
    if (!parseUnsignedLongText(
            statusIntervalText,
            config_.minSendIntervalS,
            config_.maxSendIntervalS,
            statusIntervalS)) {
        deviceHandler_->discardParsedDeviceSettings();
        sendResultPage(
            F("Eingabe ungueltig"),
            String(statusIntervalLabel()) + F(" ist ungueltig."),
            true,
            400,
            true);
        return;
    }

    uint32_t sensorIntervalS = 0UL;
    if (!parseUnsignedLongText(
            sensorIntervalText,
            effectiveMinSensorSendIntervalS(),
            effectiveMaxSensorSendIntervalS(),
            sensorIntervalS)) {
        deviceHandler_->discardParsedDeviceSettings();
        sendResultPage(
            F("Eingabe ungueltig"),
            String(sensorIntervalLabel()) + F(" ist ungueltig."),
            true,
            400,
            true);
        return;
    }

    String deviceError;
    if (!deviceHandler_->parseDeviceSave(server_, deviceError)) {
        deviceHandler_->discardParsedDeviceSettings();
        sendResultPage(
            F("Eingabe ungueltig"),
            deviceError,
            true,
            400,
            true);
        return;
    }

    NodeBasisSnapshot previousBasis = {};
    captureBasisSnapshot(previousBasis);
    deviceHandler_->captureDeviceSnapshot();

    setStoredMasterMac(parsedMasterMac);
    *statusSendIntervalS_ = sanitizeStatusSendInterval(statusIntervalS);
    *sensorSendIntervalS_ = sanitizeSensorSendInterval(sensorIntervalS);
    deviceHandler_->applyParsedDeviceSettings();

    if (!saveCurrentState()) {
        restoreBasisSnapshot(previousBasis);
        deviceHandler_->restoreDeviceSnapshot();
        deviceHandler_->discardParsedDeviceSettings();
        sendResultPage(
            F("Speichern fehlgeschlagen"),
            F("Speichern in NVS fehlgeschlagen. Vorzustand wurde wiederhergestellt."),
            true,
            500,
            true);
        return;
    }

    deviceHandler_->discardParsedDeviceSettings();

    *restartPending_ = true;
    *restartRequestedAtMs_ = millis();

    sendResultPage(
        F("Gespeichert"),
        F("Setup-Daten gespeichert. Neustart laeuft."),
        false,
        200,
        false);
}

// loadBasisFromStorage – Basis-Blob aus Preferences lesen und anwenden
bool NodeProvisioningController::loadBasisFromStorage(Preferences& prefs) {
    NodeBasisSettings settings = {};
    if (!readBasisBlob(prefs, settings)) {
        return false;
    }

    applyBasisSettings(settings);
    return true;
}

// readBasisBlob – Rohdaten der Basis aus Preferences lesen mit Groessen- und Validitaetspruefung
//   Rueckgabe: true bei Erfolg, outSettings ist dann gefuellt
bool NodeProvisioningController::readBasisBlob(Preferences& prefs, NodeBasisSettings& outSettings) const {
    const char* basisKey = config_.basisStorageKey ? config_.basisStorageKey : "node_basis_v1";
    if (prefs.getBytesLength(basisKey) != sizeof(NodeBasisSettings)) {
        return false;
    }

    NodeBasisSettings candidate = {};
    if (prefs.getBytes(basisKey, &candidate, sizeof(candidate)) != sizeof(candidate)) {
        return false;
    }

    if (!basisBlobValid(candidate)) {
        return false;
    }

    outSettings = candidate;
    return true;
}

// applyBasisSettings – Geladene Basis-Einstellungen auf aktive Runtime-Pointer anwenden
void NodeProvisioningController::applyBasisSettings(const NodeBasisSettings& settings) {
    if ((settings.flags & NODE_BASIS_FLAG_MASTER_MAC) != 0U) {
        setStoredMasterMac(settings.masterMac);
    } else {
        clearStoredMasterMac();
    }

    *statusSendIntervalS_ = sanitizeStatusSendInterval(settings.statusSendIntervalS);
    *sensorSendIntervalS_ = sanitizeSensorSendInterval(settings.sensorSendIntervalS);
}

// buildBasisSettings – Aktuelle Runtime-Werte in NodeBasisSettings-Struct ueberfuehren
NodeBasisSettings NodeProvisioningController::buildBasisSettings() const {
    NodeBasisSettings settings = {};
    settings.magic = NODE_BASIS_MAGIC;
    settings.version = NODE_BASIS_VERSION;
    settings.statusSendIntervalS = sanitizeStatusSendInterval(*statusSendIntervalS_);
    settings.sensorSendIntervalS = sanitizeSensorSendInterval(*sensorSendIntervalS_);

    if (*masterMacValid_) {
        settings.flags |= NODE_BASIS_FLAG_MASTER_MAC;
        memcpy(settings.masterMac, masterMac_, 6U);
    }

    return settings;
}

// writeBasisToStorage – Aktuelle oder uebergebene Basis in Preferences schreiben
bool NodeProvisioningController::writeBasisToStorage(Preferences& prefs) const {
    return writeBasisToStorage(prefs, buildBasisSettings());
}

bool NodeProvisioningController::writeBasisToStorage(
    Preferences& prefs,
    const NodeBasisSettings& settings) const {
    const char* basisKey = config_.basisStorageKey ? config_.basisStorageKey : "node_basis_v1";
    return prefs.putBytes(basisKey, &settings, sizeof(settings)) == sizeof(settings);
}

// removeBasisFromStorage – Basis-Blob aus Preferences entfernen
bool NodeProvisioningController::removeBasisFromStorage(Preferences& prefs) const {
    const char* basisKey = config_.basisStorageKey ? config_.basisStorageKey : "node_basis_v1";
    prefs.remove(basisKey);
    return true;
}

// saveCurrentState – Aktuelle Einstellungen persistent in NVS speichern
//   Vorgehen: 1) Preferences im Schreibmodus oeffnen
//             2) Basis-Blob schreiben (nur bei Aenderung)
//             3) Device-Settings speichern (durch deviceHandler)
//   Rollback: Wenn Device-Save fehlschlaegt, wird Basis auf Vorzustand zurueckgesetzt.
//             Wenn vorher keine Basis existierte, wird sie komplett entfernt.
//   Rueckgabe: true bei Erfolg, false bei NVS-Fehler
bool NodeProvisioningController::saveCurrentState() {
    if (!initialized_) return false;

    Preferences prefs;
    if (!prefs.begin(config_.storageNamespace, false)) {
        log("WARN", "Preferences konnten fuer Save nicht geoeffnet werden.");
        return false;
    }

    NodeBasisSettings previousBasis = {};
    const bool hadPreviousBasis = readBasisBlob(prefs, previousBasis);
    const NodeBasisSettings nextBasis = buildBasisSettings();
    const bool basisChanged = !hadPreviousBasis || !basisSettingsEqual(previousBasis, nextBasis);

    if (basisChanged && !writeBasisToStorage(prefs, nextBasis)) {
        prefs.end();
        log("WARN", "Gemeinsame Node-Basis konnte nicht gespeichert werden.");
        return false;
    }

    if (!deviceHandler_->saveDeviceSettings(prefs)) {
        if (basisChanged && hadPreviousBasis) {
            const char* basisKey = config_.basisStorageKey ? config_.basisStorageKey : "node_basis_v1";
            prefs.putBytes(basisKey, &previousBasis, sizeof(previousBasis));
        } else if (basisChanged) {
            removeBasisFromStorage(prefs);
        }

        prefs.end();
        log("WARN", "Geraetespezifische Setup-Daten konnten nicht gespeichert werden. Node-Basis wurde zurueckgerollt.");
        return false;
    }

    prefs.end();

    char masterMacText[MASTER_MAC_TEXT_LEN] = {0};
    formatMacText(masterMac_, *masterMacValid_, masterMacText, sizeof(masterMacText));
    log(
        "INFO",
        "Node-Basis %s: master_mac=%s status_s=%lu sensor_s=%lu",
        basisChanged ? "gespeichert" : "unveraendert",
        *masterMacValid_ ? masterMacText : "unset",
        (unsigned long)*statusSendIntervalS_,
        (unsigned long)*sensorSendIntervalS_);
    return true;
}

// clearStoredSettings – Alle persistierten Einstellungen loeschen (Basis + Device)
//   Vorgehen: 1) Preferences oeffnen 2) Basis entfernen 3) Device-Settings loeschen
//   Rollback: Wenn Device-Loeschung fehlschlaegt, wird Basis wiederhergestellt.
//   Rueckgabe: true bei Erfolg, false bei NVS-Fehler
bool NodeProvisioningController::clearStoredSettings() {
    if (!initialized_) return false;

    Preferences prefs;
    if (!prefs.begin(config_.storageNamespace, false)) {
        log("WARN", "Preferences konnten fuer Reset nicht geoeffnet werden.");
        return false;
    }

    NodeBasisSettings previousBasis = {};
    const bool hadPreviousBasis = readBasisBlob(prefs, previousBasis);

    removeBasisFromStorage(prefs);
    if (!deviceHandler_->clearDeviceSettings(prefs)) {
        if (hadPreviousBasis) {
            const char* basisKey = config_.basisStorageKey ? config_.basisStorageKey : "node_basis_v1";
            prefs.putBytes(basisKey, &previousBasis, sizeof(previousBasis));
        }
        prefs.end();
        log("WARN", "Geraetespezifische Setup-Daten konnten nicht geloescht werden.");
        return false;
    }

    prefs.end();
    log("INFO", "Gemeinsame Node-Basis und geraetespezifische Setup-Daten geloescht.");
    return true;
}

// update – Haupt-Loop des Provisioning-Controllers (muss regelmaessig gerufen werden)
//   Tasks: 1) Setup-Taster auswerten 2) Web-Client bedienen (wenn AP aktiv)
//          3) Setup-LED-Indikator aktualisieren 4) Verzoegerten Neustart ausloesen
//
void NodeProvisioningController::update() {
    if (!initialized_) return;

    updateSetupButton();

    if (*setupApActive_) {
        server_.handleClient();
    }

    updateSetupIndicator();

    if (*restartPending_ &&
        (millis() - *restartRequestedAtMs_) >= config_.restartDelayMs) {
        *restartPending_ = false;
        log("INFO", "Setup-Neustart wird ausgefuehrt.");
        exitSetupMode("save restart");
        delay(50);
        ESP.restart();
    }
}

// isSetupModeActive – Ist der Setup-Modus aktuell aktiv?
bool NodeProvisioningController::isSetupModeActive() const {
    return setupMode_ != nullptr && *setupMode_;
}

// setupButtonConfigured – Ist ein Setup-Taster in der Konfiguration definiert?
bool NodeProvisioningController::setupButtonConfigured() const {
    return config_.setupButtonPin >= 0;
}

// setupIndicatorConfigured – Ist eine Setup-LED in der Konfiguration definiert?
bool NodeProvisioningController::setupIndicatorConfigured() const {
    return config_.setupIndicatorLedPin >= 0;
}

// statusIntervalArgName – HTML-Formular-Feldname fuer Status-Intervall (aus Config, sonst Default)
const char* NodeProvisioningController::statusIntervalArgName() const {
    return fallbackText(config_.statusSendIntervalFieldName, STATUS_INTERVAL_ARG_DEFAULT);
}

// sensorIntervalArgName – HTML-Formular-Feldname fuer Sensor-Intervall (aus Config, sonst Default)
const char* NodeProvisioningController::sensorIntervalArgName() const {
    return fallbackText(config_.sensorSendIntervalFieldName, SENSOR_INTERVAL_ARG_DEFAULT);
}

// statusIntervalLabel – Anzeigelabel fuer Status-Intervall (aus Config, sonst Feldname)
const char* NodeProvisioningController::statusIntervalLabel() const {
    return fallbackText(config_.statusSendIntervalLabel, statusIntervalArgName());
}

// sensorIntervalLabel – Anzeigelabel fuer Sensor-Intervall (aus Config, sonst Feldname)
const char* NodeProvisioningController::sensorIntervalLabel() const {
    return fallbackText(config_.sensorSendIntervalLabel, sensorIntervalArgName());
}

// statusIntervalHint – Hilfetext fuer Status-Intervall (aus Config, sonst Default-Hinweis)
const char* NodeProvisioningController::statusIntervalHint() const {
    return fallbackText(config_.statusSendIntervalHint, STATUS_INTERVAL_HINT_DEFAULT);
}

// sensorIntervalHint – Hilfetext fuer Sensor-Intervall (aus Config, sonst Default-Hinweis)
const char* NodeProvisioningController::sensorIntervalHint() const {
    return fallbackText(config_.sensorSendIntervalHint, SENSOR_INTERVAL_HINT_DEFAULT);
}

// initializeSetupIo – GPIOs fuer Setup-Taster und -LED initialisieren (pinMode + Pullup-Konfig)
//   Der Taster-Pin wird als INPUT (ggf. INPUT_PULLUP) konfiguriert, die LED als OUTPUT.
void NodeProvisioningController::initializeSetupIo() {
    if (setupButtonConfigured()) {
        pinMode(config_.setupButtonPin, config_.setupButtonActiveLow ? INPUT_PULLUP : INPUT);
    }

    if (setupIndicatorConfigured()) {
        pinMode(config_.setupIndicatorLedPin, OUTPUT);
        writeSetupIndicator(false);
    }
}

// writeSetupIndicator – LED-Zustand setzen (beruecksichtigt activeHigh-Konfiguration)
void NodeProvisioningController::writeSetupIndicator(bool active) {
    if (!setupIndicatorConfigured()) return;

    digitalWrite(
        config_.setupIndicatorLedPin,
        active == config_.setupIndicatorLedActiveHigh ? HIGH : LOW);
    setupIndicatorState_ = active;
}

// updateSetupButton – Taster-Entprellung und Langdruck-Erkennung
//   Erkennt steigende/fallende Flanke und misst die Haltezeit.
//   Bei ueberschreiten von setupButtonHoldMs: enterSetupMode() ausloesen.
//   Entprellung erfolgt indirekt ueber die Haltezeit-Schwelle.
//   Nach einmaligem Ausloesen wird setupButtonHoldConsumed gesetzt bis zum Loslassen.
void NodeProvisioningController::updateSetupButton() {
    if (!setupButtonConfigured()) return;

    const bool active =
        config_.setupButtonActiveLow
            ? (digitalRead(config_.setupButtonPin) == LOW)
            : (digitalRead(config_.setupButtonPin) == HIGH);
    const unsigned long jetztMs = millis();

    if (active && !setupButtonLastActive_) {
        setupButtonPressedAtMs_ = jetztMs;
        setupButtonHoldConsumed_ = false;
    }

    if (active && !setupButtonHoldConsumed_ && !isSetupModeActive()) {
        const unsigned long holdMs =
            config_.setupButtonHoldMs > 0UL ? config_.setupButtonHoldMs : SETUP_BUTTON_HOLD_MS_DEFAULT;
        if ((jetztMs - setupButtonPressedAtMs_) >= holdMs) {
            setupButtonHoldConsumed_ = true;
            log("INFO", "Setup-Taster gehalten, starte Setup-Modus");
            enterSetupMode();
        }
    }

    if (!active && setupButtonLastActive_) {
        setupButtonPressedAtMs_ = 0UL;
        setupButtonHoldConsumed_ = false;
    }

    setupButtonLastActive_ = active;
}

// updateSetupIndicator – LED-Blinken im Setup-Modus (sonst aus)
//   Im Setup-Modus: LED im Intervall setupIndicatorBlinkMs togglen.
//   Blink-Startzeit wird bei jedem Eintritt in den Setup-Modus zurueckgesetzt.
//   Ausserhalb: LED ausschalten und Blink-Zaehler zuruecksetzen.
void NodeProvisioningController::updateSetupIndicator() {
    if (!setupIndicatorConfigured()) return;

    if (!isSetupModeActive()) {
        if (setupIndicatorState_) {
            writeSetupIndicator(false);
        }
        setupIndicatorLastBlinkMs_ = 0UL;
        return;
    }

    const unsigned long blinkMs =
        config_.setupIndicatorBlinkMs > 0UL ? config_.setupIndicatorBlinkMs : SETUP_INDICATOR_BLINK_MS_DEFAULT;
    const unsigned long jetztMs = millis();
    if (setupIndicatorLastBlinkMs_ == 0UL ||
        (jetztMs - setupIndicatorLastBlinkMs_) >= blinkMs) {
        writeSetupIndicator(!setupIndicatorState_);
        setupIndicatorLastBlinkMs_ = jetztMs;
    }
}

// log – Formatiertes Logging ueber Callback (nur bei gesetztem logFn_)
//   Baut einen 240-Byte-Puffer auf und ruft den Log-Callback mit Level und Message.
void NodeProvisioningController::log(const char* level, const char* format, ...) const {
    if (logFn_ == nullptr || format == nullptr) return;

    char message[240];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    logFn_(level, message);
}

}  // namespace ShNodeProvisioning
}  // namespace SmartHome
