# NET-ERL — 1-Relais-Knoten (Hardware-Referenz)

> `device_class: net_erl` | `SH_CLASS_NET_ERL (0x01)` | KiCAD: `Relais_1fach`

## Übersicht

NET-ERL ist die Geräteklasse für **netzbetriebene 1-Relais-Knoten**. Einsatz: Lampen, Steckdosen, Flurlicht-Steuerung mit optionalen Sensoren.

## Modularer Aufbau

NET-ERL verwendet den **zweiteiligen modularen Platinenaufbau**:

```
┌─────────────────────┐    JST-PH 5-pol    ┌──────────────────────┐
│   Leistungskreis     │◄──────────────────►│  Modularer Steuerkreis│
│   (230V + Relais)    │   5V/GND/GPIO10    │  (ESP32-C3 + I²C)    │
│                      │   /GPIO5/3V3       │                      │
│ HLK-5M05             │                    │ ESP32-C3 SuperMini   │
│ HF46F Relais (1×)    │                    │ I²C-Pullups (4K7)    │
│ FQP27P06 P-MOSFET    │                    │ GPIO-Pinheader       │
│ PC817 Optokoppler    │                    │ 5 Testpunkte         │
│ 1N4007 Freilaufdiode │                    │ 2× M3 Montage        │
└─────────────────────┘                    └──────────────────────┘
```

### Leistungskreis (230V-Seite)

- **KiCAD:** `Relais_1fach/Realistische_Schaltung/Leistungskreis/`
- **Projekt:** `Leistungskreis_1Relai_korrektur.kicad_pro`
- Siehe [leistungskreis_1relais.md](leistungskreis_1relais.md)

### Steuerkreis (Logik-Seite)

- **KiCAD:** `Relais_1fach/Realistische_Schaltung/Modularer_Steuerkreis/mod_Steuerkreis_ESP32/`
- **Identisch mit net_zrl Steuerkreis!**
- Siehe [../shared/modularer_steuerkreis.md](../shared/modularer_steuerkreis.md)

## Geräte dieser Klasse

| Gerät | ID | Sensoren/Besonderheiten |
|-------|-----|------------------------|
| [NET-ERL-001](NET-ERL-001_hall_module.md) | Flurmodul Basis | BME280, VEML7700, HC-SR501 PIR |
| [NET-ERL-002](NET-ERL-002_hall_module_led_ring.md) | Flurmodul LED-Ring | BME680, VEML7700, ENS160, LD2410, NeoPixel-Ring |

## Pinbelegung

| GPIO | Funktion | Bemerkung |
|------|----------|-----------|
| GPIO0 | I²C SDA | Sensoren (theoretisch verfügbar) |
| GPIO1 | I²C SCL | Sensoren (theoretisch verfügbar) |
| GPIO5 | Trigger_PIN2 | NC bei 1-Relais (reserviert für 2-Relais) |
| GPIO10 | Trigger_PIN1 | Relais-Steuerung (via Optokoppler PC817) |
| GPIO20 | RX | UART (Debug) |
| GPIO21 | TX | UART (Debug) |
