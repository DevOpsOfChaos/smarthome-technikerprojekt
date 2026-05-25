# SmartHome Technikerprojekt — Firmware-Dokumentation

Dieses Verzeichnis enthält die vollständige technische Dokumentation aller 6 ESP32-C3-basierten SmartHome-Geräte, des ESP-NOW Binärprotokolls, der MQTT-Verträge und der Device-Typen/Capabilities.

## Dokumentationsstruktur

| Datei | Beschreibung | Größe |
|---|---|---|
| `01_espnow_protokoll_referenz.md` | ESP-NOW Binary Protocol: Header, Payloads, CRC, Event-System | 28 KB |
| `02_devicetypes_capabilities.md` | Device-Klassen, Power-Typen, Control-Modes, Profile, Capability-Bitmask | 7.5 KB |
| `03_mqtt_contract_referenz.md` | MQTT Topics, JSON-Schemata, ACK-Mechanismus, Cover-Contract | 14.5 KB |
| `devices/NET-ERL-001_hall_module.md` | Flurmodul (PIR+BME280+VEML7700+Relais) | 16.5 KB |
| `devices/NET-ERL-002_hall_module_led_ring.md` | Erweit. Flurmodul (Radar+BME680+ENS160+NeoPixel+Relais) | 19.4 KB |
| `devices/NET-ZRL-002_shutter_module.md` | Rollladenmodul (2 Relais, 3 Taster, Kalibrierung, Positionsschätzung) | ~600 Zeilen |
| `devices/NET-SEN-002_weather_station.md` | Wettersensor (BME280+VEML7700+digitaler Regen) | 12 KB |
| `devices/bat_sen_01_window_contact.md` | Batterie-Fensterkontakt (Reed, CR2032, GPIO-Wake) | 9.7 KB |
| `devices/bat_sen_02_rain_sensor.md` | Batterie-Regensensor (ADC, Hysterese, Timer-Wake) | 12.7 KB |

## Device Comparison Matrix

| Merkmal | NET-ERL-001 | NET-ERL-002 | NET-ZRL-002 | NET-SEN-002 | bat_sen_01 | bat_sen_02 |
|---|---|---|---|---|---|---|
| Klasse | NET_ERL (0x01) | NET_ERL (0x01) | NET_ZRL (0x02) | NET_SEN (0x03) | BAT_SEN (0x04) | BAT_SEN (0x04) |
| Strom | Mains | Mains | Mains | Mains | CR2032 | 2x AA |
| Leistung | relais_light | relais_light | cover | none | none | none |
| Profil | hall_light | hall_module_led_ring | cover_basic | none | none | none |
| Reporting | hybrid | hybrid | hybrid | hybrid | sleep_event | sleep_event |
| Sensoren | BME280, VEML7700, PIR | BME680, VEML7700, ENS160, LD2410 | — | BME280, VEML7700, Rain | Reed (GPIO) | ADC Rain |
| Aktoren | 1 Relais | 1 Relais + NeoPixel | 2 Relais (Cover) | — | — | — |
| Taster | — | 1 (GPIO6, akt-low) | 3 (GPIO20/4/3) | — | — | — |
| Status-LEDs | — | — | 2 (Auf=GPIO7, Ab=GPIO6, akt-high) | — | — | — |
| ESP-NOW Payload | 31B (Config) | 45B (Gas+Config) | 25B (Config) | 24–36B (ext) | 24B | 24B |
| Wake | n/a | n/a | n/a | n/a | GPIO+Timer | Timer (900s) |
| Setup-Portal | Ja | Ja | Ja | Ja | Ja | Ja |

## Design Principles

- **Base type architecture** — `NetErlRuntime`, `NetSenRuntime`, `BatSenRuntime`
- **Device-specific hooks pattern** — Hooks for `on_config`, `on_sensor`, `on_control`
- **ESP-NOW retry mechanism** — 2 retries, 50 ms interval
- **Sensor recovery** — damped error logging, automatic re-init
- **Late-Lux principle** — auto-light waits for stable lux after PIR trigger
- **Kalibrierung mit Rollback** — persistent config with rollback on write failure

## Quick Links

- ESP-NOW Protokoll: [01_espnow_protokoll_referenz.md](01_espnow_protokoll_referenz.md)
- Device-Typen: [02_devicetypes_capabilities.md](02_devicetypes_capabilities.md)
- MQTT Vertrag: [03_mqtt_contract_referenz.md](03_mqtt_contract_referenz.md)
- Alle Devices: [devices/](devices/)
