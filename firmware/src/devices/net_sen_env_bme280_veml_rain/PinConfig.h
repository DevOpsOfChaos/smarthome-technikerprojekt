// =============================================================================
// PinConfig.h – GPIO-Pin-Mapping fuer NET-SEN Env BME280+VEML+Rain
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_sen_env_bme280_veml_rain/PinConfig.h
// Hardware:   ESP32-C3
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN]
// === EINSATZZWECK ===
//
// Pin-Belegung:
//   I2C SDA:         GPIO0 – BME280 (Adr. 0x76/0x77) + VEML7700 (0x10)
//   I2C SCL:         GPIO1
//   Regen-Digital:   GPIO3 – digitaler Regensensor (active-LOW + Pullup)
//   Status-LED:      -1 (nicht bestueckt)
//   Setup-Button:    GPIO2 (active-LOW, 5s Hold)
//   Setup-LED:       GPIO7 (active-HIGH, 500ms blink)
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
// =============================================================================

#pragma once

#define NET_SEN_PIN_SENSOR_SDA 0
#define NET_SEN_PIN_SENSOR_SCL 1

// Digitaler Regen-Signalpin (active-LOW, Pullup aktiv)
#define NET_SEN_ENV_BME280_VEML_RAIN_SIGNAL_PIN 3
#define NET_SEN_ENV_BME280_VEML_RAIN_ACTIVE_LOW 1
#define NET_SEN_ENV_BME280_VEML_RAIN_USE_PULLUP 1

#define NET_SEN_PIN_STATUS_LED -1

#define SETUP_BUTTON_PIN 2
#define SETUP_BUTTON_ACTIVE_LOW 1
#define SETUP_BUTTON_HOLD_MS 5000UL
#define SETUP_INDICATOR_LED_PIN 7
#define SETUP_INDICATOR_LED_ACTIVE_HIGH 1
#define SETUP_INDICATOR_BLINK_MS 500UL
