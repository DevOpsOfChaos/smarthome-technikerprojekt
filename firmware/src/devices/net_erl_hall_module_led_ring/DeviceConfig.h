/*
===============================================================================
 Datei: DeviceConfig.h
 Code-Name: NET-ERL Hall Module LED Ring Config
 Projekt: SmartHome Technikerprojekt
 Bereich: Firmware / Device-Konfiguration / Netzbetriebener Relais-Komfortaktor
 Ersteller: DevOpsOfChaos
 Datum: 2026-05-14
 Letzte Bearbeitung: 2026-05-18

 Zweck: Geraetekonfiguration fuer das NET-ERL Hall Module LED Ring
 Beschreibung: Definiert Identitaet, Faehigkeiten, Funk-/Meldeintervalle,
 Auto-Light-Parameter und Sensoradressen fuer die LED-Ring-Variante mit BME680,
 VEML7700, ENS160, LD2410, Button und NeoPixel-Ring.

 Genutzte Bibliotheken:
 - DeviceTypes.h: eigene Protokollbibliothek mit Capability-Flags, Profilen und Modi.

 Wichtige Werte:
 - 50 Millisekunden Sensor-Poll-Intervall fuer LD2410.
 - 60000 Millisekunden Umweltsensor-Intervall entsprechen 60 Sekunden.
 - 30000 Millisekunden Recovery/Snapshot entsprechen 30 Sekunden.
 - 180000 Millisekunden Gas-Warmup entsprechen 180 Sekunden oder 3 Minuten.
 - 1200000 Millisekunden ENS160-Warmup entsprechen 1200 Sekunden oder 20 Minuten.
 - 120000 Millisekunden ENS160-Stale-Timeout entsprechen 120 Sekunden oder 2 Minuten.
 - 40 Millisekunden Button-Entprellzeit.

 Aenderungsverlauf:
 - 2026-05-14: Konfiguration fuer NET-ERL Hall Module LED Ring angelegt.
 - 2026-05-18: Dateiheader vereinheitlicht und Platzhalter entfernt.
===============================================================================
*/

#pragma once

#include "../../../lib/sh_protocol/src/DeviceTypes.h"

#define NET_ERL_DEVICE_ID "NET-ERL-002"
#define NET_ERL_DEVICE_NAME "NET-ERL Hall Module LED Ring"
#define NET_ERL_FW_VARIANT "net_erl_hall_module_led_ring"
// Bitmaske: RELAY(0x0001)|TEMP(0x0004)|HUM(0x0008)|LUX(0x0010)|AQI(0x0020)|MOTION(0x0040)|BUTTON(0x0400)|LED_RING(0x1000)|GAS(0x4000)|PRESSURE(0x8000)
#define NET_ERL_DEVICE_CAPS (SH_CAP_RELAY | SH_CAP_TEMP | SH_CAP_HUM | SH_CAP_LUX | SH_CAP_MOTION | SH_CAP_AQI | SH_CAP_PRESSURE | SH_CAP_BUTTON | SH_CAP_LED_RING | SH_CAP_GAS)

#define NET_ERL_DEVICE_CONTROL_MODE SH_CONTROL_MODE_RELAY_LIGHT
#define NET_ERL_DEVICE_CONFIG_PROFILE SH_PROFILE_HALL_MODULE_LED_RING
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
#define NET_ERL_DEFAULT_AUTO_ON_LUX_THRESHOLD 250U
#define NET_ERL_DEFAULT_AUTO_OFF_DELAY_S 15U

// Teil der Technikerarbeit:
// Grundparameter fuer die LED-Ring-Luftqualitaetsanzeige des Hall-Moduls.
// Der Ring visualisiert den gemessenen ENS160-AQI lokal am Geraet.
#define NET_ERL_LED_RING_BRIGHTNESS 24U
#define NET_ERL_LED_RING_MAX_CFG_BRIGHTNESS 96U
#define NET_ERL_LED_RING_AQI_PHASE_MS 15000UL

// Nicht Bestandteil der Technikerarbeit:
// Zusaetzliche Komfortphasen und Hinweisanimationen. Sie aendern weder
// Protokoll, Serververtrag noch die bewertete Firmware-Architekturlinie.
#define NET_ERL_LED_RING_TEMP_PHASE_MS 15000UL
#define NET_ERL_LED_RING_HUM_PHASE_MS 15000UL
#define NET_ERL_LED_RING_FRAME_INTERVAL_MS 120UL
#define NET_ERL_LED_RING_LUX_BLOCKED_ALERT_MS 3000UL

#define NET_ERL_SENSOR_POLL_INTERVAL_MS 50UL        // 50 Millisekunden LD2410-Poll.
#define NET_ERL_ENV_SAMPLE_INTERVAL_MS 60000UL      // 60000 Millisekunden = 60 Sekunden.
#define NET_ERL_SENSOR_RECOVERY_RETRY_INTERVAL_MS 30000UL // 30000 Millisekunden = 30 Sekunden.
#define NET_ERL_SNAPSHOT_LOG_INTERVAL_MS 30000UL    // 30000 Millisekunden = 30 Sekunden.

#define NET_ERL_I2C_CLOCK_HZ 5000UL  // 5 kHz I2C-Takt fuer langen/stoeranfaelligen Sensorbus.

// BME680
#define NET_ERL_BME680_PRIMARY_ADDRESS 0x76
#define NET_ERL_BME680_FALLBACK_ADDRESS 0x77
#define NET_ERL_BME680_GAS_WARMUP_MS 180000UL      // 180000 Millisekunden = 3 Minuten.
#define NET_ERL_BME680_GAS_WARMUP_MIN_READS 5U

// ENS160
#define NET_ERL_ENS160_PRIMARY_ADDRESS 0x52
#define NET_ERL_ENS160_FALLBACK_ADDRESS 0x53
#define NET_ERL_ENS160_WARMUP_MS 1200000UL         // 1200000 Millisekunden = 20 Minuten.
#define NET_ERL_ENS160_STALE_TIMEOUT_MS 120000UL   // 120000 Millisekunden = 2 Minuten.

// Button
#define NET_ERL_BUTTON_DEBOUNCE_MS 40UL // 40 ms Entprellzeit (lokaler Taster).

// Sensor-Offset-Kompensation (in Geraete-Nativeinheiten).
// Positiver Offset = Korrektur nach oben, negativer = nach unten.
// Beispiel: BME680 nahe Netzteil misst 30 °C bei 22 °C Raumtemperatur → Offset -80 (-8,0 °C in Zehntelgrad).
#define NET_ERL_TEMP_OFFSET_01C      0    // Zehntelgrad (0 = kein Offset)
#define NET_ERL_HUM_OFFSET_01PCT     0    // Zehntelprozent (0 = kein Offset)
