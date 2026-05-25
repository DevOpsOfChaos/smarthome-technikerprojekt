# NET-ZRL-020 – Shutter Module (ESPHome)

## 1. Übersicht

**ESPHome-ID:** `NET-ZRL-020`
**Typ:** Netzbetrieb (mains)
**Hardware:** ESP32-C3 (esp32-c3-devkitm-1)
**Device-Class:** `net_zrl`
**Aktor:** 2 Relais (Auf/Ab), 2 Status-LEDs
**Eingaben:** 3 Taster (Auf, Ab, Stop)
**Steuerung:** `control_mode: cover`, `config_profile: cover_basic`
**Kommunikation:** MQTT direkt

## 2. Hardware

| Komponente | GPIO | Funktion |
|-----------|------|----------|
| Relais Auf | GPIO10 | Rollladen hoch (`relay_up_pin`) |
| Relais Ab | GPIO5 | Rollladen runter (`relay_down_pin`) |
| Taster AUF | GPIO20 | Lokale Auf-Steuerung (`button_up_pin`) |
| Taster AB | GPIO4 | Lokale Ab-Steuerung + Setup-Hold (`button_down_pin`) |
| Taster STOP | GPIO3 | Stop (`button_stop_pin`) |
| LED Auf | GPIO7 | Statusanzeige Auf-Relais (`led_up_pin`) |
| LED Ab | GPIO6 | Statusanzeige Ab-Relais (`led_down_pin`) |

## 3. Konfiguration (YAML-Substitutions)

```yaml
node_name: net-zrl-020
device_id: "NET-ZRL-020"
device_name: "NET-ZRL Shutter"
device_class: net_zrl
power_type: mains
fw_version: "1"
caps: "10243"                            # RELAY(1)|RELAY2(2)|MULTIBUTTON(2048)|COVER(8192)
control_mode: cover
config_profile: cover_basic
reporting_mode: hybrid
sensor_mask: "XXXXXXXXXX"
input_mask: "XXXXX"
setup_ap_password: net-zrl-setup
setup_status_default: "60"
setup_sensor_default: "60"
setup_status_min: "10"
setup_status_max: "65535"
setup_sensor_min: "10"
setup_sensor_max: "65535"
relay_up_pin: GPIO10
relay_down_pin: GPIO5
default_cover_travel_ms: "100000"         # 100 s Standard-Fahrzeit
button_up_pin: GPIO20
button_down_pin: GPIO4
button_stop_pin: GPIO3
led_up_pin: GPIO7
led_down_pin: GPIO6
```

## 4. Cover-Steuerung

### 4.1 Plattform
- **Typ**: eigene Relais-/Fahrzeitsteuerung, analog zur Original-Firmware
- **Restore-Mode**: `ALWAYS_OFF` (beide Relais starten immer aus)

### 4.2 Modi
- **Manuell**: Lokale Taster (Auf/Ab/Stop)
- **MQTT-Kommando**: `open`, `close`, `stop`, `set_position`
- **Setup**: AB-Taster 5 s halten

### 4.3 Kalibrierung
- **Starten**: STOP-Taster 5 s halten oder `calibrate`-Kommando
- **Phasen**: Idle(0) -> MovingToTop(1) -> WaitForDownStart(2) -> MeasuringDown(3) -> WaitForUpStart(4) -> MeasuringUp(5)
- **Ergebnis**: `cover_calibrated = true`, Fahrzeiten gespeichert (`travel_time_up_ms`, `travel_time_down_ms`), Position = 100
- **Löschen**: `clear_calibration`-Kommando
- **Ungelernt**: keine gespeicherten Fahrzeiten (`0`), `cover_position = null`; Endlagenfahrten nutzen `default_estimated_travel_time_ms` (100.000 ms)

### 4.4 Teil-Positionierung (`set_position`)
- Nur nach Kalibrierung möglich
- Berechnet Fahrzeit: `partial_ms = (travel_ms * |target - current|) / 100`
- Endlagen 0 und 100 sind auch unkalibriert als volle Auf-/Ab-Fahrt erlaubt

### 4.5 cover_state
| Wert | Bedeutung |
|------|-----------|
| `opening` | Fährt hoch |
| `closing` | Fährt runter |
| `stopped` | Steht |

### 4.6 cover_position
- 0–100 % (0 = geschlossen, 100 = offen)
- `null` wenn nicht kalibriert

### 4.7 Relais-Mapping
- Über `setup_relay_up_mapping` konfigurierbar (relay_a / relay_b)
- Vertauscht Auf/Ab-Relais-Zuordnung

## 5. MQTT-Topics

| Topic | Retain | Beschreibung |
|-------|:------:|-------------|
| `smarthome/device/NET-ZRL-020/meta` | ✅ | Metadaten |
| `smarthome/device/NET-ZRL-020/availability` | ✅ | Online/Offline |
| `smarthome/device/NET-ZRL-020/state` | ✅ | Cover-Zustand (Position, Modus) |
| `smarthome/device/NET-ZRL-020/event` | ❌ | Events (cover_up/down/stop) |
| `smarthome/device/NET-ZRL-020/ack` | ❌ | Kommando-Bestätigung |
| `smarthome/device/NET-ZRL-020/command` | — | open/close/stop/set_position/calibrate/clear_calibration |

## 6. State-Payload (Cover-Contract)

```json
{
  "device_id": "NET-ZRL-020",
  "relay_1": false,
  "relay_2": false,
  "cover_mode": true,
  "cover_state": "stopped",
  "cover_position": 75,
  "cover_calibrated": true,
  "fault": false
}
```

## 7. Events

| Event | event_type | trigger | Bedingung |
|-------|-----------|---------|-----------|
| `cover_up` | 9 | manual_button (1) | AUF-Taster gedrückt |
| `cover_down` | 10 | manual_button (1) | AB-Taster gedrückt (kurz, < 5 s) |
| `cover_stop` | 11 | manual_button (1) | STOP-Taster gedrückt |
| `node_boot` | — | — | Gerät startet (on_boot) |

## 8. Fault-Erkennung

- **hardware_fault**: immer `false` (keine diagnostischen Sensoren)
- **contract_fault**: `true` bei Kommando-Fehlern (z.B. unkalibriert)
- **State-JSON `fault`**: `hardware_fault || contract_fault`

## 9. Setup-Taster

- **AB-Taster (GPIO4)**: 5 s halten → Setup-Portal (WiFi-AP, SSID = `net-zrl-020-setup`)
- **Setup-Passwort**: `net-zrl-setup`
- Master-MAC und Fahrzeiten konfigurierbar

## 10. Intervalle

- **Periodischer Refresh**: alle 5 min (Meta, Availability, State)
- **State-Publish**: `setup_status_default` = 60 s
