# NET-ZRL — 2-Relais-Knoten / Rolladen (Hardware-Referenz)

> `device_class: net_zrl` | `SH_CLASS_NET_ZRL (0x02)` | KiCAD: `Relais_2fach`

## Übersicht

NET-ZRL ist die Geräteklasse für **netzbetriebene 2-Relais-Knoten** mit Rolladen-/Cover-Steuerung. Im Gegensatz zu NET-ERL verwendet NET-ZRL zwei Relais in Sicherheitsschaltung (Serien-Verdrahtung).

## Modularer Aufbau

NET-ZRL verwendet wie NET-ERL den **zweiteiligen modularen Platinenaufbau**, jedoch mit einem anderen Leistungskreis:

```
┌─────────────────────┐    JST-PH 5-pol    ┌──────────────────────┐
│   Leistungskreis     │◄──────────────────►│  Modularer Steuerkreis│
│   (230V + 2× Relais) │  5V/GND/GPIO10    │  (ESP32-C3 + I²C)    │
│                      │  /GPIO5/3V3        │                      │
│ HLK-5M05             │                    │ **IDENTISCH mit      │
│ HF46F Relais (2×)    │                    │  net_erl Steuerkreis**│
│ IRLZ34N N-MOSFET (2×)│                    │                      │
│ PC817 Optokoppler (2×)│                   │ ESP32-C3 SuperMini   │
│ 1N4007 Freilaufdiode  │                   │ I²C-Pullups (4K7)    │
└─────────────────────┘                    └──────────────────────┘
```

> **Wichtig:** Der Steuerkreis ist **derselbe** wie bei NET-ERL! Nur der Leistungskreis unterscheidet sich.

### Leistungskreis (230V-Seite)

- **KiCAD:** `Relais_2fach/Realistische_Schaltung/Leistungskreis/Leistungskreis_korrektur/`
- Siehe [leistungskreis_2relais.md](leistungskreis_2relais.md)
- **Unterschiede zum 1-Relais-Leistungskreis:**
  - 2× Relais statt 1×
  - N-Kanal MOSFETs (IRLZ34N) statt P-Kanal
  - 2× Optokoppler und Freilaufdioden
  - Serien-Schaltung der Relaiskontakte (Sicherheit!)

### Steuerkreis (Logik-Seite)

- **KiCAD:** `Relais_2fach/Realistische_Schaltung/Modularer_Steuerkreis/mod_Steuerkreis_ESP32/`
- **Identisch mit net_erl Steuerkreis!**
- Siehe [../shared/modularer_steuerkreis.md](../shared/modularer_steuerkreis.md)

## Geräte dieser Klasse

| Gerät | ID | Besonderheiten |
|-------|-----|----------------|
| [NET-ZRL-002](NET-ZRL-002_shutter_module.md) | Rolladensteuerung | 2 Relais, 3 Taster, Kalibrierung, Positionsschätzung |

## Pinbelegung

| GPIO | Funktion | Bemerkung |
|------|----------|-----------|
| GPIO0 | I²C SDA | Optional (theoretisch verfügbar) |
| GPIO1 | I²C SCL | Optional (theoretisch verfügbar) |
| GPIO5 | Trigger_PIN2 | Relais 2 (Runter) — via Optokoppler |
| GPIO10 | Trigger_PIN1 | Relais 1 (Hoch) — via Optokoppler |
| GPIO20 | RX | UART (Debug) |
| GPIO21 | TX | UART (Debug) |

## Sicherheitsschaltung

Die beiden Relais sind in **Serie** geschaltet — es ist physikalisch unmöglich, dass beide Relais gleichzeitig schließen. Dies verhindert einen Kurzschluss der Motorwicklungen (gleichzeitiges Hoch- und Runterfahren).

Zusätzlich sorgt die Firmware für eine konfigurierbare **Dead-Time** zwischen dem Umschalten der Relais.
