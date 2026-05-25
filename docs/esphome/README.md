# ESPHome-Geräte – Dokumentation

## Übersicht

Die ESPHome-Linie ist eine alternative Firmware-Implementierung für Smarthome-Geräte.
Im Gegensatz zur Firmware-Linie (ESP-NOW → Master → MQTT) kommunizieren ESPHome-Geräte
**direkt per MQTT** mit dem Server – ohne ESP-NOW-Master als Zwischeninstanz.

## Architektur

```
┌──────────────┐   MQTT direkt   ┌──────────┐
│ ESPHome-Gerät│◄──────────────►│  Server  │
│  (ESP32-C3)  │   JSON         │(Node-RED)│
└──────────────┘                 └──────────┘
```

**Kein Master, kein ESP-NOW.** Jedes Gerät ist ein eigenständiger MQTT-Client
mit vollständiger Topic-Verwaltung (Birth/Last-Will, Meta, State, Events, Commands).

## Geräte-Übersicht

| Gerät | ESPHome-ID | Typ | Sensoren | Besonderheiten |
|-------|-----------|-----|----------|----------------|
| [NET-ERL-010](devices/NET-ERL-010_hall_module.md) | `NET-ERL-010` | Netz | BME280, VEML7700, PIR | 1 Relais, Motion-Event |
| [NET-ERL-020](devices/NET-ERL-020_hall_module_led_ring.md) | `NET-ERL-020` | Netz | BME680, VEML7700, ENS160, LD2410 | 1 Relais, LED-Ring, Auto-Light |
| [NET-SEN-020](devices/NET-SEN-020_weather_station.md) | `NET-SEN-020` | Netz | BME280, VEML7700 | Regensensor (GPIO) |
| [NET-ZRL-020](devices/NET-ZRL-020_shutter_module.md) | `NET-ZRL-020` | Netz | — | 2 Relais, Cover-Kalibrierung |
| [bat_sen_010](devices/bat_sen_010_window_contact.md) | `bat_sen_010` | Batterie | Reed-Kontakt | Deep-Sleep (15 min), Fenster-Event |
| [bat_sen_020](devices/bat_sen_020_rain_sensor.md) | `bat_sen_020` | Batterie | Regensensor (ADC) | Deep-Sleep (15 min), Regen-Event |

## Gemeinsame Konzepte

### MQTT-Vertrag
Alle ESPHome-Geräte implementieren denselben MQTT-Vertrag wie die Firmware-Linie.
Siehe [Firmware MQTT-Contract](../firmware/03_mqtt_contract_referenz.md) für Details.

Die ESPHome-Linie verwendet eigene Geräte-IDs (3-stelliger Suffix, z.B. `NET-ERL-020`).
MQTT-Topics sind case-sensitiv – ESPHome-IDs nutzen Kleinbuchstaben mit Underscore
für Batteriegeräte (`bat_sen_010`).

### Events
Alle Geräte publizieren Ereignisse auf `smarthome/device/{id}/event`:
- Bewegung, Fenster, Regen, Taster, Relais-Schaltung
- 18 Event-Typen mit Trigger-Quelle und Parametern

### Sensor-Offset
Temperatur und Feuchte können über YAML-Substitutions (`temp_offset_01c`,
`hum_offset_01pct`) kalibriert werden. Offsets in Zehntelgrad/-prozent.

### Setup-Modus
Jedes Gerät kann durch langes Drücken des Setup-Tasters (5 s) in den
Setup-Modus versetzt werden. Der Setup-Modus startet einen WiFi-Access-Point
(SSID = Geräte-ID) mit Webinterface zur Konfiguration von Master-MAC und
Geräteparametern.

### Code-Struktur
Jedes ESPHome-Gerät besteht aus:
- **Device-YAML** (`esphome/devices/{name}.yaml`): Gerätespezifische Konfiguration
- **Shared Packages** (`esphome/packages/`): Wiederverwendbare Bausteine
  - `smarthome_contract_base.yaml`: MQTT-Meta, Availability, Provisioning-UI
  - `smarthome_command_ack.yaml`: Kommando-ACK-System
  - `smarthome_device_event.yaml`: Event-Publishing
  - `smarthome_cover_contract.yaml`: Rollladen-Logik (nur NET-ZRL)
  - `smarthome_command_dispatch.h`: C++ Shared-Funktionen (MAC-Validierung)

## Unterschiede zur Firmware-Linie

| Aspekt | ESPHome | Firmware |
|--------|---------|----------|
| Kommunikation | MQTT direkt | ESP-NOW → Master → MQTT |
| Konfiguration | YAML-Substitutions | C++ DeviceConfig.h |
| Sensor-Filter | 16× Oversampling + IIR | EMA-Filter (α=0,2) |
| Events | Alle 18 Typen | 15 Typen (nur ESP-NOW) |
| Deep-Sleep | ESPHome `deep_sleep` | Manuelle `esp_deep_sleep_start()` |
| Cover | `smarthome_cover_contract.yaml` | Inline C++ State-Machine |
