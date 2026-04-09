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
        capabilityHelpers: require("/opt/smarthome/lib/capability_helpers"),
        dashboardV1: require("/opt/smarthome/lib/dashboard_v1"),
        deviceStore: require("/opt/smarthome/lib/device_store"),
        timeHelpers: require("/opt/smarthome/lib/time_helpers"),
        topicHandlers: require("/opt/smarthome/lib/topic_handlers"),
        topicRouter: require("/opt/smarthome/lib/topic_router")
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
