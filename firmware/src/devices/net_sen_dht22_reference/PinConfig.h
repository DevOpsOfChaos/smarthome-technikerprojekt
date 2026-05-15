// =============================================================================
// PinConfig.h – GPIO-Pin-Mapping fuer NET-SEN DHT22 Reference
// =============================================================================
// Projekt:    Smarthome Technikerprojekt
// Pfad:       firmware/src/devices/net_sen_dht22_reference/PinConfig.h
// Hardware:   ESP32-C3
//
// === EINSATZZWECK ===
// [HIER EINTRAGEN]
// === EINSATZZWECK ===
//
// Pin-Belegung:
//   DHT22-Daten:    GPIO6 – OneWire-Datenleitung (mit Pullup)
//   Status-LED:     -1    – nicht bestueckt
//
// I2C:  deaktiviert (NET_SEN_ENABLE_I2C_BASE=0)
//       Keine I2C-Sensoren an SDA/SCL angeschlossen.
//
// Autor:           DevOpsOfChaos
// Erstelldatum:    2026-05-14
// Letzte Aenderung: 2026-05-14
// =============================================================================

#pragma once

// DHT22-Datenleitung (fester Pin fuer den Referenzpfad)
#define NET_SEN_DHT22_REF_PIN_DATA 6

// Optionale Status-LED nicht bestueckt
#define NET_SEN_PIN_STATUS_LED -1
