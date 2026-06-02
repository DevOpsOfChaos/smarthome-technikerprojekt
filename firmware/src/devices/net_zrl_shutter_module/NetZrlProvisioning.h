// =============================================================================
// NetZrlProvisioning.h – Provisioning-Config-Factory fuer NET-ZRL
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_zrl_shutter_module/NetZrlProvisioning.h
//
// Datei-Funktion:
//   Factory-Konfiguration fuer das NodeProvisioning-Framework.
//   Definiert Setup-AP-Parameter (SSID-Passwort, Kanal), Storage-Namespaces
//   und die makeConfig()-Factory-Funktion fuer NET-ZRL-Geraete.
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
//
// Aenderungshistorie:
//   [2026-05-14] DevOpsOfChaos – Kommentierung (Deutsch)
// =============================================================================

#pragma once

// Arduino.h wird fuer Arduino-Typen genutzt, ShNodeProvisioning.h definiert die
// NodeProvisioningConfig-Struktur und das Web-/NVS-Provisioning-Framework.
#include <Arduino.h>
#include <ShNodeProvisioning.h>

namespace SmartHome {
namespace NetZrlProvisioning {

// =============================================================================
// KONSTANTEN – Setup-AP und Storage
// =============================================================================

// Passwort fuer den Setup-AccessPoint. Es ist kein Produktgeheimnis, sondern
// ein Bring-up-/Service-Passwort fuer lokale Inbetriebnahme.
constexpr const char* SETUP_AP_PASSWORD = "net-zrl-setup";

// Storage-Namespace fuer Preferences (NVS-Partition). Muss zu main.cpp passen,
// sonst werden Master-MAC und Intervalle aus einem anderen Namespace gelesen.
constexpr const char* STORAGE_NAMESPACE = "net_zrl";

// Storage-Key fuer den NodeBasis-Datensatz (Version v1). Version im Key macht
// inkompatible Layoutwechsel explizit.
constexpr const char* STORAGE_KEY_NODE_BASIS = "node_basis_v1";

// WLAN-Kanal des Setup-AccessPoints
constexpr int SETUP_AP_CHANNEL = 1;

// Verzoegerung zwischen Restart-Anforderung und tatsaechlichem Neustart (ms)
constexpr unsigned long RESTART_DELAY_MS = 1500UL;

// =============================================================================
// FACTORY-FUNKTION – NodeProvisioningConfig erzeugen
// =============================================================================

// Aufgabe: Erstellt eine vollstaendige NodeProvisioning-Konfiguration.
// Eingabewerte:
// - deviceId: Geraete-ID, wird fuer Setup-SSID/Anzeige verwendet.
// - defaultStatusSendIntervalS: Default-STATE-Intervall in Sekunden.
// - defaultSensorSendIntervalS: Default-Sensor-Intervall in Sekunden.
// - minSendIntervalS/maxSendIntervalS: erlaubter Bereich fuer Setup-Werte.
// Ausgabewert: NodeProvisioningConfig fuer ShNodeProvisioning.
//
// Diese Factory haelt Storage-Keys und AP-Parameter an einer Stelle. main.cpp
// muss dadurch nicht wissen, wie die generische Provisioning-Struktur aufgebaut ist.
inline ShNodeProvisioning::NodeProvisioningConfig makeConfig(
    const char* deviceId,
    uint32_t defaultStatusSendIntervalS,
    uint32_t defaultSensorSendIntervalS,
    uint32_t minSendIntervalS,
    uint32_t maxSendIntervalS) {
    return {
        deviceId,
        SETUP_AP_PASSWORD,
        STORAGE_NAMESPACE,
        STORAGE_KEY_NODE_BASIS,
        defaultStatusSendIntervalS,
        defaultSensorSendIntervalS,
        minSendIntervalS,
        maxSendIntervalS,
        RESTART_DELAY_MS,
        SETUP_AP_CHANNEL,
    };
}

}  // namespace NetZrlProvisioning
}  // namespace SmartHome
