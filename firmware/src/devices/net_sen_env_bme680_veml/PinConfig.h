// =============================================================================
// PinConfig.h – GPIO-Pin-Mapping fuer NET-SEN Env BME680+VEML+ENS160
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_sen_env_bme680_veml/PinConfig.h
// Hardware:   ESP32-C3
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN]
// === EINSATZZWECK ===
//
// Pin-Belegung:
//   I2C SDA: GPIO0
//   I2C SCL: GPIO1
//   Status-LED: -1 (nicht bestueckt)
//
// I2C-Adressen:
//   BME680:      0x76 (Fallback 0x77)
//   VEML7700:    0x10
//   ENS160:      0x52 (Fallback 0x53)
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
// =============================================================================

#pragma once

#define NET_SEN_PIN_SENSOR_SDA 0
#define NET_SEN_PIN_SENSOR_SCL 1
#define NET_SEN_PIN_STATUS_LED -1
