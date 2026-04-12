#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>

namespace SmartHome {
namespace ShNodeProvisioning {

constexpr size_t MASTER_MAC_TEXT_LEN = 18U;

struct NodeBasisSettings {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint32_t flags;
    uint8_t masterMac[6];
    uint8_t reservedMac[2];
    uint32_t statusSendIntervalS;
    uint32_t sensorSendIntervalS;
};

struct NodeBasisSnapshot {
    bool masterMacValid;
    uint8_t masterMac[6];
    uint32_t statusSendIntervalS;
    uint32_t sensorSendIntervalS;
};

struct NodeProvisioningConfig {
    NodeProvisioningConfig() = default;

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
    unsigned long setupButtonHoldMs = 5000UL;
    int setupIndicatorLedPin = -1;
    bool setupIndicatorLedActiveHigh = true;
    unsigned long setupIndicatorBlinkMs = 500UL;
};

using SetupLogFn = void (*)(const char* level, const char* message);

class DeviceProvisioningHandler {
  public:
    virtual ~DeviceProvisioningHandler() = default;

    virtual const char* pageTitle() const = 0;
    virtual const char* pageIntro() const = 0;
    virtual const char* deviceSectionTitle() const = 0;
    virtual const char* deviceSectionIntro() const = 0;

    virtual void loadDeviceDefaults() = 0;
    virtual bool loadDeviceSettings(Preferences& prefs) = 0;
    virtual bool saveDeviceSettings(Preferences& prefs) = 0;
    virtual bool clearDeviceSettings(Preferences& prefs) = 0;

    virtual void captureDeviceSnapshot() = 0;
    virtual void restoreDeviceSnapshot() = 0;

    virtual bool parseDeviceSave(WebServer& server, String& errorText) = 0;
    virtual void applyParsedDeviceSettings() = 0;
    virtual void discardParsedDeviceSettings() = 0;
    virtual void appendDeviceFieldsHtml(String& page, WebServer* sourceServer) const = 0;
    virtual void appendDeviceActionsHtml(String& page) const { (void)page; }
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

class NodeProvisioningController {
  public:
    NodeProvisioningController();

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

    void update();

    void applyDefaultBasisValues();
    void captureBasisSnapshot(NodeBasisSnapshot& snapshot) const;
    void restoreBasisSnapshot(const NodeBasisSnapshot& snapshot);

    void enterSetupMode();
    void exitSetupMode(const char* reason);

    bool saveCurrentState();
    bool clearStoredSettings();

    bool isSetupModeActive() const;
    bool hasStoredMasterMac() const;
    String buildStoredMasterMacText() const;

    bool isSendIntervalValid(uint32_t valueS) const;
    uint32_t sanitizeStatusSendInterval(uint32_t valueS) const;
    uint32_t sanitizeSensorSendInterval(uint32_t valueS) const;

    static bool parseMacText(const char* text, uint8_t outMac[6]);
    static void formatMacText(
        const uint8_t* mac,
        bool isValid,
        char* buffer,
        size_t bufferSize);

  private:
    void configureRoutes();
    void handleRoot();
    void handleSave();
    void sendForm(
        const String& masterMacText,
        const String& statusIntervalText,
        const String& sensorIntervalText,
        const String& infoText,
        const String& errorText,
        int statusCode,
        WebServer* sourceServer);
    String buildPage(
        const String& masterMacText,
        const String& statusIntervalText,
        const String& sensorIntervalText,
        const String& infoText,
        const String& errorText,
        WebServer* sourceServer) const;
    void appendSharedStyles(String& page) const;
    void sendResultPage(
        const String& titleText,
        const String& messageText,
        bool isError,
        int statusCode,
        bool showBackButton);
    String buildResultPage(
        const String& titleText,
        const String& messageText,
        bool isError,
        bool showBackButton) const;

    bool loadBasisFromStorage(Preferences& prefs);
    bool writeBasisToStorage(Preferences& prefs) const;
    bool writeBasisToStorage(Preferences& prefs, const NodeBasisSettings& settings) const;
    bool removeBasisFromStorage(Preferences& prefs) const;
    bool readBasisBlob(Preferences& prefs, NodeBasisSettings& outSettings) const;
    void applyBasisSettings(const NodeBasisSettings& settings);
    NodeBasisSettings buildBasisSettings() const;

    bool readRequestedMasterMac(String& outValue, const char*& outSourceArg);
    bool parseUnsignedLongText(
        String value,
        uint32_t minValue,
        uint32_t maxValue,
        uint32_t& outValue) const;
    void clearStoredMasterMac();
    void setStoredMasterMac(const uint8_t masterMac[6]);
    void log(const char* level, const char* format, ...) const;
    void initializeSetupIo();
    void updateSetupButton();
    void updateSetupIndicator();
    void writeSetupIndicator(bool active);
    bool setupButtonConfigured() const;
    bool setupIndicatorConfigured() const;
    const char* statusIntervalArgName() const;
    const char* sensorIntervalArgName() const;
    const char* statusIntervalLabel() const;
    const char* sensorIntervalLabel() const;
    const char* statusIntervalHint() const;
    const char* sensorIntervalHint() const;
    uint32_t effectiveMinSensorSendIntervalS() const;
    uint32_t effectiveMaxSensorSendIntervalS() const;

    NodeProvisioningConfig config_;
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
    DeviceProvisioningHandler* deviceHandler_;
    SetupLogFn logFn_;
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
