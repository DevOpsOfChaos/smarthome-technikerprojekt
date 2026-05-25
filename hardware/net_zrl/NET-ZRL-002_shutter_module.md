# NET-ZRL-002 — Rolladensteuerung

> 2-Relais-Knoten mit Taster-Steuerung | `SH_PROFILE_COVER_BASIC`

## Übersicht

Das **NET-ZRL-002** ist ein netzbetriebener 2-Relais-Knoten für die Rolladen-/Jalousiensteuerung. Es unterstützt manuelle Bedienung über 3 Taster (Hoch/Stopp/Runter) und automatisierte Steuerung per MQTT-Befehl.

## Hardware

| Komponente | Detail |
|-----------|--------|
| **Leistungskreis** | [leistungskreis_2relais.md](leistungskreis_2relais.md) — 2× HF46F, HLK-5M05 |
| **Steuerkreis** | [../shared/modularer_steuerkreis.md](../shared/modularer_steuerkreis.md) — ESP32-C3 |
| **Relais** | 2× Hongfa HF46F/5-HS1 in Serien-Sicherheitsschaltung |
| **Taster** | 3× (Hoch, Stopp, Runter) |
| **Versorgung** | 230V AC → 5V DC (HLK-5M05) |

## GPIO-Belegung

| GPIO | Funktion | Bemerkung |
|------|----------|-----------|
| GPIO0 | I²C SDA | Optional (nicht belegt) |
| GPIO1 | I²C SCL | Optional (nicht belegt) |
| GPIO3 | Taster HOCH | Digital In mit Pullup |
| GPIO4 | Taster STOPP | Digital In mit Pullup |
| GPIO5 | Trigger_PIN2 | Relais RUNTER (via PC817) |
| GPIO9 | Taster RUNTER | Digital In mit Pullup |
| GPIO10 | Trigger_PIN1 | Relais HOCH (via PC817) |
| GPIO20 | UART RX | Debug |
| GPIO21 | UART TX | Debug |

## Fähigkeiten (Capabilities)

| Cap | Bit | Bedeutung |
|-----|-----|-----------|
| `SH_CAP_RELAY` | 0x0001 | Relais 1 (Hoch) |
| `SH_CAP_RELAY2` | 0x0002 | Relais 2 (Runter) |
| `SH_CAP_COVER` | 0x2000 | Rolladen/Cover-Funktionalität |
| `SH_CAP_MULTIBUTTON` | 0x0800 | 3 Taster (Hoch/Stopp/Runter) |

## Taster-Beschaltung

Drei Taster gegen GND mit internen Pullups:

| Taster | GPIO | Funktion |
|--------|------|----------|
| HOCH | GPIO3 | Motor Aufwärts |
| STOPP | GPIO4 | Motor Stopp |
| RUNTER | GPIO9 | Motor Abwärts |

## Rolladen-Logik

- **Kalibrierung:** Einlern-Fahrzeit für vollständige Öffnung/Schließung
- **Positionsschätzung:** Zeitbasierte Berechnung der aktuellen Position
- **Dead-Time:** Konfigurierbare Pause zwischen Relais-Umschaltung (Schutz vor Kurzschluss)
- **Auto-Stopp:** Automatisches Abschalten bei erreichter Endposition

## Betriebsmodi

- **Control Mode:** `SH_CONTROL_MODE_COVER`
- **Config Profile:** `SH_PROFILE_COVER_BASIC`
- **Reporting:** `SH_REPORTING_HYBRID`

## Firmware

- **Verzeichnis:** `firmware/src/devices/net_zrl_shutter_module/`
- **Baut mit:** `platformio run -e net_zrl_shutter_module`
- **Zustandspayload:** `ZrlStateReportPayload` (Position, Ziel, Status, Kalibrierung)
