/**
 * =============================================================================
 * @modul     settings (Node-RED)
 * @beschreibung  Node-RED-Serverkonfiguration für den SmartHome-Server.
 *                Definiert Ports, globale Kontexte für Function-Nodes und
 *                die Einbindung aller Lib-Module.
 *
 * @umgebung  Wird beim Containerstart von docker-compose.yml als Basis
 *            verwendet und um commandMinimal, coverAutomation ergänzt.
 * @export    Node-RED-kompatibles Settings-Objekt
 * =============================================================================
 */

const path = require("path");
const libRoot = path.join(__dirname, "lib");

module.exports = {
    // -----------------------------------------------------------------------
    // Dateisystem
    // -----------------------------------------------------------------------
    flowFile: "flows.json",
    flowFilePretty: true,

    // -----------------------------------------------------------------------
    // Server
    // -----------------------------------------------------------------------
    uiPort: process.env.PORT || 1880,
    uiHost: "0.0.0.0",

    // -----------------------------------------------------------------------
    // Verbindungen
    // -----------------------------------------------------------------------
    mqttReconnectTime: 15000,
    serialReconnectTime: 15000,
    sqliteReconnectTime: 20000,

    // -----------------------------------------------------------------------
    // Debugging
    // -----------------------------------------------------------------------
    debugMaxLength: 1000,

    // -----------------------------------------------------------------------
    // Function-Node-Sicherheit
    // -----------------------------------------------------------------------
    functionExternalModules: false,

    // -----------------------------------------------------------------------
    // Globale Kontexte für Function-Nodes
    // -----------------------------------------------------------------------
    // Jeder Eintrag steht in jedem Function-Node als global.get("<key>")
    // zur Verfügung. Die Module werden einmalig beim Start geladen.
    functionGlobalContext: {
        capabilityHelpers: require(path.join(libRoot, "capability_helpers")),
        commandMinimal:    require(path.join(libRoot, "command_minimal")),
        coverAutomation:   require(path.join(libRoot, "cover_automation")),
        dashboardV1:       require(path.join(libRoot, "dashboard_v1")),
        deviceStore:       require(path.join(libRoot, "device_store")),
        timeHelpers:       require(path.join(libRoot, "time_helpers")),
        topicHandlers:     require(path.join(libRoot, "topic_handlers")),
        topicRouter:       require(path.join(libRoot, "topic_router"))
    },
    exportGlobalContextKeys: false,

    // -----------------------------------------------------------------------
    // Sicherheit
    // -----------------------------------------------------------------------
    // CredentialSecret aus Umgebungsvariable – nie hartcodiert.
    credentialSecret: process.env.NODERED_CREDENTIAL_SECRET || false,

    // -----------------------------------------------------------------------
    // Editor
    // -----------------------------------------------------------------------
    editorTheme: {
        projects: {
            enabled: false
        }
    }
};
