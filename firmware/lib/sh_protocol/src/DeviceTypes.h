#pragma once

/**
 * @file DeviceTypes.h
 * @brief Geraeteklassen, Faehigkeits-Bitmasks, Power-Typen und Meta-Enums
 *
 * @details Definiert den oberen Geraetevertrag fuer das SmartHome-Protokoll.
 *          Geraeteklassen beschreiben die Basisarchitektur eines Nodes.
 *          Sondergeraete (z.B. Rolladensteuerung) sind NET-ZRL-Geraete
 *          mit entsprechenden Meta-/Profilfeldern – keine neue Klasse.
 *
 * @version 0.3.0
 * @date    2026-03-25
 */

// ============================================================
// Geräteklassen (device_class im HELLO-Payload)
// ============================================================

// Netzbetriebener Node mit einem Relais.
// Typisch: einfache Lampe, Schaltaktor.
#define SH_CLASS_NET_ERL    0x01U

// Netzbetriebener Node mit zwei Relais.
// Typisch: Rolladen, Außenlicht, Doppelschalter.
#define SH_CLASS_NET_ZRL    0x02U

// Netzbetriebener Sensor-Node, kein Relais.
// Typisch: Klimasensor, Luftqualität, Präsenz.
#define SH_CLASS_NET_SEN    0x03U

// Batteriebetriebener Sensor- und Event-Node.
// Typisch: Fensterkontakt, Wandschalter, Regensensor.
#define SH_CLASS_BAT_SEN    0x04U

// Master-Gerät (Bridge ESP-NOW <-> MQTT).
#define SH_CLASS_MASTER     0xFEU

// Unbekannte oder noch nicht registrierte Klasse.
#define SH_CLASS_UNKNOWN    0xFFU

// ============================================================
// Power-Typen (power_type im HELLO-Payload)
// ============================================================

// Netzbetrieben (dauerhaft verfügbar).
#define SH_POWER_MAINS      0x00U

// Batteriebetrieben (schläft zwischen Ereignissen oder periodisch).
#define SH_POWER_BATTERY    0x01U

// ============================================================
// Meta-Schema-Version
// ============================================================

// Erste Version des oberen Geräte-Metadatenschemas.
#define SH_META_SCHEMA_VERSION_1        0x01U
#define SH_META_SCHEMA_VERSION_CURRENT  SH_META_SCHEMA_VERSION_1

// ============================================================
// Control-Modes (control_mode im HELLO-Payload)
// ============================================================

// Kein Aktorpfad vorhanden.
#define SH_CONTROL_MODE_NONE             0x00U

// Ein Relais, generisch.
#define SH_CONTROL_MODE_RELAY            0x01U

// Ein Relais, fachlich primär Licht.
#define SH_CONTROL_MODE_RELAY_LIGHT      0x02U

// Zwei Relais, generisch.
#define SH_CONTROL_MODE_DUAL_RELAY       0x03U

// Zwei Relais, fachlich primär Licht.
#define SH_CONTROL_MODE_DUAL_RELAY_LIGHT 0x04U

// Rolladen-/Cover-Steuerung.
#define SH_CONTROL_MODE_COVER            0x05U

// ============================================================
// Config-Profile (config_profile im HELLO-Payload)
// ============================================================

// Kein Zusatzprofil.
#define SH_PROFILE_NONE           0x00U

// Flurlicht / Bewegungslicht mit einem Relais.
#define SH_PROFILE_HALL_LIGHT     0x01U

// Basis-Rolladenprofil.
#define SH_PROFILE_COVER_BASIC    0x03U

// Flurmodul mit LED-Ring, Luftqualitaets- und Presence-Erweiterung.
#define SH_PROFILE_HALL_MODULE_LED_RING 0x04U

// ============================================================
// Reporting-Modes (reporting_mode im HELLO-Payload)
// ============================================================

// Nur periodische Zustandsmeldungen.
#define SH_REPORTING_PERIODIC        0x01U

// Nur ereignisgetrieben.
#define SH_REPORTING_EVENT_DRIVEN    0x02U

// Periodisch + ereignisgetrieben.
#define SH_REPORTING_HYBRID          0x03U

// Schlafender Node mit periodischem Aufwachen.
#define SH_REPORTING_SLEEP_PERIODIC  0x04U

// Schlafender Node mit Event-/Wake-getriebenem Pfad.
#define SH_REPORTING_SLEEP_EVENT     0x05U

// ============================================================
// Faehigkeits-Bitmasks (caps_hi / caps_lo im HELLO-Payload)
// ============================================================
//
// Beide Bytes zusammen ergeben eine 16-Bit-Maske.
// caps_hi enthaelt Bits 15..8, caps_lo enthaelt Bits 7..0.
//
// Kombination zu 16-Bit-Wert: uint16_t caps = ((uint16_t)caps_hi << 8) | caps_lo;
//
// Pruefung auf Faehigkeit:    bool hatRelay = (caps & SH_CAP_RELAY) != 0;
//
// Bits 7..0 (caps_lo):

// Mindestens ein Relais vorhanden.
#define SH_CAP_RELAY        0x0001U

// Zwei Relais vorhanden (Rolladen, Doppelschalter).
#define SH_CAP_RELAY2       0x0002U

// Temperatursensor vorhanden.
#define SH_CAP_TEMP         0x0004U

// Feuchtigkeitssensor vorhanden.
#define SH_CAP_HUM          0x0008U

// Lichtsensor vorhanden.
#define SH_CAP_LUX          0x0010U

// Luftqualitätssensor vorhanden.
#define SH_CAP_AQI          0x0020U

// Bewegungs- oder Präsenzsensor vorhanden (PIR oder Radar).
#define SH_CAP_MOTION       0x0040U

// Fensterkontakt vorhanden (Reed-Schalter).
#define SH_CAP_WINDOW       0x0080U

// Bits 15..8 (caps_hi):

// Regensensor vorhanden.
#define SH_CAP_RAIN         0x0100U

// Batteriemessung vorhanden.
#define SH_CAP_BATTERY      0x0200U

// Lokaler Taster vorhanden (1-fach).
#define SH_CAP_BUTTON       0x0400U

// Mehrfachtaster vorhanden (2- bis 4-fach).
#define SH_CAP_MULTIBUTTON  0x0800U

// WS2812-LED-Ring vorhanden.
#define SH_CAP_LED_RING     0x1000U

// Rolladenfähigkeit vorhanden (Fahrzeit, Kalibrierung, Verriegelung).
#define SH_CAP_COVER        0x2000U

// Setup-Portal aktiv (nur für Diagnosezwecke im HELLO mitgeteilt).
#define SH_CAP_SETUP_PORTAL 0x4000U

// Luftdrucksensor vorhanden.
#define SH_CAP_PRESSURE     0x8000U
