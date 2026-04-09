const path = require("path");
const libRoot = path.join(__dirname, "lib");

module.exports = {
    flowFile: "flows.json",
    flowFilePretty: true,
    uiPort: process.env.PORT || 1880,
    uiHost: "0.0.0.0",
    mqttReconnectTime: 15000,
    serialReconnectTime: 15000,
    debugMaxLength: 1000,
    functionExternalModules: false,
    functionGlobalContext: {
        capabilityHelpers: require(path.join(libRoot, "capability_helpers")),
        dashboardV1: require(path.join(libRoot, "dashboard_v1")),
        deviceStore: require(path.join(libRoot, "device_store")),
        timeHelpers: require(path.join(libRoot, "time_helpers")),
        topicHandlers: require(path.join(libRoot, "topic_handlers")),
        topicRouter: require(path.join(libRoot, "topic_router"))
    },
    exportGlobalContextKeys: false,
    sqliteReconnectTime: 20000,
    credentialSecret: process.env.NODERED_CREDENTIAL_SECRET || false,
    editorTheme: {
        projects: {
            enabled: false
        }
    }
};
