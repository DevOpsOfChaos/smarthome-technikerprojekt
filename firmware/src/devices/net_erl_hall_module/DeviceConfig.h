/*
===============================================================================
 Datei: DeviceConfig.h
 Code-Name: NET-ERL Hall Module Config
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / Device-Konfiguration / Netzbetriebener Relais-Komfortaktor
 Ersteller: DevOpsOfChaos
 Datum: 2026-05-14
 Letzte Bearbeitung: 2026-05-18

 Zweck: Geraetekonfiguration fuer das NET-ERL Hall Module
 Beschreibung: Definiert Identitaet, Faehigkeiten, Funk-/Meldeintervalle,
 Auto-Light-Parameter und Sensoradressen fuer net_erl_hall_module. Der BME280
 wird hier nur fuer Temperatur und Feuchte genutzt; Druck bleibt bewusst nicht
 Teil des externen Capability-Vertrags.

 Genutzte Bibliotheken:
 - DeviceTypes.h: eigene Protokollbibliothek mit Capability-Flags, Profilen und Modi.

 Wichtige Werte:
 - 5000 Millisekunden HELLO-Retry entsprechen 5 Sekunden.
 - 20000 Millisekunden Heartbeat entsprechen 20 Sekunden.
 - 20 Millisekunden Loop-Intervall begrenzen die Hauptschleife.
 - 250 Millisekunden Sensor-Poll-Intervall fuer den PIR.
 - 60000 Millisekunden Umweltsensor-Intervall entsprechen 60 Sekunden.
 - 30000 Millisekunden Recovery/Snapshot entsprechen 30 Sekunden.
 - 15 Sekunden Auto-Off-Delay sind die Nachlaufzeit nach Bewegung.

 Aenderungsverlauf:
 - 2026-05-14: Konfiguration fuer NET-ERL Hall Module angelegt.
 - 2026-05-18: Dateiheader vereinheitlicht und Platzhalter entfernt.
===============================================================================
*/

#pragma once

#include "../../../lib/sh_protocol/src/DeviceTypes.h"

#define NET_ERL_DEVICE_ID "NET-ERL-001"
#define NET_ERL_DEVICE_NAME "NET-ERL Hall Module"
#define NET_ERL_FW_VARIANT "net_erl_hall_module"

// Druck bleibt trotz BME280 bewusst aussen vor (Aussenvertrag ohne PRESSURE)
#define NET_ERL_DEVICE_CAPS (SH_CAP_RELAY | SH_CAP_TEMP | SH_CAP_HUM | SH_CAP_LUX | SH_CAP_MOTION)

#define NET_ERL_DEVICE_CONTROL_MODE SH_CONTROL_MODE_RELAY_LIGHT
#define NET_ERL_DEVICE_CONFIG_PROFILE SH_PROFILE_HALL_LIGHT
#define NET_ERL_DEVICE_REPORTING_MODE SH_REPORTING_HYBRID

#define NET_ERL_DEBUG_ENABLED 1
#define NET_ERL_WLAN_CHANNEL 6

#define NET_ERL_HELLO_RETRY_INTERVAL_MS 5000UL  // 5000 Millisekunden = 5 Sekunden.
#define NET_ERL_HEARTBEAT_INTERVAL_MS 20000UL   // 20000 Millisekunden = 20 Sekunden.
#define NET_ERL_LOOP_INTERVAL_MS 20UL           // 20 Millisekunden Loop-Pause.

#define NET_ERL_MIN_REPORT_INTERVAL_S 5U
#define NET_ERL_MAX_REPORT_INTERVAL_S 600U
#define NET_ERL_BOOT_COUNTER 1U

#define NET_ERL_DEFAULT_REPORT_INTERVAL_S 10U
#define NET_ERL_DEFAULT_AUTO_ON_LUX_THRESHOLD 250U  // Lux unter diesem Wert = einschalten
#define NET_ERL_DEFAULT_AUTO_OFF_DELAY_S 15U        // 15 Sekunden Nachlaufzeit.

#define NET_ERL_SENSOR_POLL_INTERVAL_MS 250UL       // 250 Millisekunden PIR-Poll.
#define NET_ERL_ENV_SAMPLE_INTERVAL_MS 60000UL      // 60000 ms = 60 s Sensor-Poll-Intervall (BME280-spezifikationskonform, verhindert Eigenerwaermung).
#define NET_ERL_SENSOR_RECOVERY_RETRY_INTERVAL_MS 30000UL // 30000 Millisekunden = 30 Sekunden.
#define NET_ERL_SNAPSHOT_LOG_INTERVAL_MS 30000UL    // 30000 Millisekunden = 30 Sekunden.

#define NET_ERL_BME280_ADDRESS 0x76
