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
const fs = require("fs");
const libRoot = path.join(__dirname, "lib");

const envPath = [
    path.join(__dirname, ".env"),
    "/config/.env",
    "/data/.env"
].find((candidate) => fs.existsSync(candidate));

if (envPath) {
    const envText = fs.readFileSync(envPath, "utf8");
    for (const line of envText.split(/\r?\n/)) {
        const trimmed = line.trim();
        if (!trimmed || trimmed.startsWith("#")) {
            continue;
        }

        const separatorIndex = trimmed.indexOf("=");
        if (separatorIndex <= 0) {
            continue;
        }

        const key = trimmed.slice(0, separatorIndex).trim();
        const value = trimmed.slice(separatorIndex + 1).trim().replace(/^["']|["']$/g, "");
        if (key && process.env[key] === undefined) {
            process.env[key] = value;
        }
    }
}

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
        topicRouter:       require(path.join(libRoot, "topic_router")),
        automationStore:   require(path.join(libRoot, "automation_store")),
        automationEngine:  require(path.join(libRoot, "automation_engine"))
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
