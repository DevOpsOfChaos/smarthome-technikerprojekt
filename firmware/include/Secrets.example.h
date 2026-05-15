/**
 * @file Secrets.example.h
 * @brief Vorlage fuer Zugangsdaten (WLAN, MQTT)
 *
 * @details Echte Zugangsdaten duerfen NIEMALS committet werden!
 *          Diese Datei ist in .gitignore eingetragen.
 *
 * Verwendung:
 *   1. Diese Datei nach "Secrets.h" kopieren
 *   2. Werte mit echten Zugangsdaten fuellen
 *   3. Secrets.h wird beim Build automatisch eingebunden
 *
 * @note Fuer OTA-Updates und erweiterte MQTT-Authentifizierung
 *       koennen zusaetzliche Defines ergaenzt werden (MQTT_USER, MQTT_PASSWORD, OTA_PASSWORD).
 *
 * @author DevOpsOfChaos
 */

#pragma once

#define WIFI_SSID         "DEIN_WLAN_NAME"
#define WIFI_PASSWORD     "DEIN_WLAN_PASSWORT"
#define MQTT_HOST         "192.168.1.100"
#define MQTT_PORT         1883
