# ESP32-C3 SuperMini Plus V2.0 — Plattform-Referenz

> Alle Geräte dieses Projekts verwenden das TENSTAR ESP32-C3 SuperMini Plus V2.0

## Technische Daten

| Eigenschaft | Wert |
|-------------|------|
| Mikrocontroller | Espressif ESP32-C3 (RISC-V, 32-Bit) |
| CPU-Takt | bis 160 MHz |
| Flash | 4 MB |
| SRAM | 400 KB |
| WLAN | 802.11 b/g/n (2.4 GHz) |
| Bluetooth | BLE 5.0 |
| USB | Native USB-CDC + JTAG |
| ADC | 12 Bit, bis 6 Kanäle nutzbar |
| PWM | auf nahezu allen GPIOs |
| Betriebsspannung | 5 V (USB/VIN) |
| Logikspannung | 3.3 V |
| Onboard-LDO | ME6211 / XC6206 / AMS1117-Clone |
| Max. Ausgangsstrom (3.3V) | ~500 mA |

## Stromaufnahme

| Modus | Strom |
|-------|-------|
| WiFi TX Peak | ~300 mA |
| Idle (WiFi connected) | 70–80 mA |
| Light Sleep | ~5 mA |
| Deep Sleep (Chip only) | ~40 µA |
| Deep Sleep (Board gesamt) | ~0.5–0.8 mA |

## Onboard-Peripherie

| Komponente | Pin/Detail |
|-----------|------------|
| Power-LED | Rot (fest verdrahtet) |
| User-LED | Blau (GPIO8) |
| RGB-LED | WS2812 (GPIO8) |
| Reset-Taster | EN |
| Boot-Taster | GPIO9 |
| Antenne | PCB-Antenne + U.FL-Anschluss |

> **Hinweis:** Blaue LED und RGB-LED teilen sich GPIO8. Beide können nicht gleichzeitig genutzt werden.

## THT-Adapter (`ESP32-C3_SUPERMINI_TH`)

Für den Einsatz auf den Projekt-Platinen wird das SuperMini-Board auf eine **Through-Hole-Adapterplatine** gesteckt:

- KiCAD-Symbol: `ESP32-C3_SUPERMINI_TH.kicad_sym`
- Footprint: `MODULE_ESP32-C3_SUPERMINI_TH.kicad_mod`
- 3D-Modell: `ESP32-C3_SUPERMINI_TH.step`
- Ermöglicht einfaches Einstecken/Austauschen auf Stiftleisten (2× 10-Pin)

## Verfügbare GPIOs auf dem Adapter

| Pin | Funktion | Analog | Besonderheit |
|-----|----------|--------|-------------|
| GPIO0 | I²C SDA / frei | ✓ | ADC1_CH0 |
| GPIO1 | I²C SCL / frei | ✓ | ADC1_CH1 |
| GPIO2 | frei | ✓ | ADC1_CH2 |
| GPIO3 | frei | ✓ | ADC1_CH3 |
| GPIO4 | frei | ✓ | ADC1_CH4 |
| GPIO5 | frei | ✓ | ADC2_CH0 |
| GPIO6 | frei | ✗ | — |
| GPIO7 | frei | ✗ | — |
| GPIO8 | frei (LED) | ✗ | Onboard WS2812 |
| GPIO9 | BOOT-Taster | ✗ | Nicht als I/O nutzbar |
| GPIO10 | frei | ✗ | — |
| GPIO20 | RX / frei | ✗ | UART |
| GPIO21 | TX / frei | ✗ | UART |

> **Strapping-Pins:** GPIO2, GPIO8, GPIO9 haben Boot-Verhalten. GPIO9 ist im Normalbetrieb nicht nutzbar.

## Schnittstellen

- **UART:** 2× (RX/TX auf GPIO20/21)
- **I²C:** 1× (SDA/SCL auf GPIO0/1)
- **SPI:** Verfügbar auf GPIO4-7
- **ADC:** 12-Bit, bis 6 Kanäle (GPIO0-5)
- **PWM:** Auf nahezu allen GPIOs
- **USB-CDC:** Native USB-Serial für Programmierung & Debug
- **USB-JTAG:** On-Chip-Debugger
