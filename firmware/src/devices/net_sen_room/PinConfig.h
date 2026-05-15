// =============================================================================
// PinConfig.h – GPIO-Pin-Mapping fuer NET-SEN Room (Legacy)
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_sen_room/PinConfig.h
// Hardware:   ESP32-C3
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN: Alte Raum-Sensor-Plaetze, z.B. Wohnzimmer/Buero]
// === EINSATZZWECK ===
//
// Pin-Belegung:
//   I2C SDA: GPIO0 (Standard aus HardwarePinStandard)
//   I2C SCL: GPIO1 (Standard)
//   Status-LED: -1 (nicht bestueckt)
//
// I2C-Sensoren (konfigurierbar via #if-Options):
//   BME280:  Adresse 0x76 (NET_SEN_ROOM_USE_BME280)
//   VEML7700: Adresse 0x10 (NET_SEN_ROOM_USE_VEML7700)
//
// Status: LEGACY – nicht Teil der aktiven net_sen-Linie.
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
// =============================================================================

#pragma once

#define NET_SEN_PIN_SENSOR_SDA 0
#define NET_SEN_PIN_SENSOR_SCL 1
#define NET_SEN_PIN_STATUS_LED -1
