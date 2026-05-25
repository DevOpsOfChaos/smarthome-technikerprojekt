# NET-ERL-010 – Hall Module (ESPHome)

## 1. Übersicht

**ESPHome-ID:** `NET-ERL-010`
**Typ:** Netzbetrieb (mains)
**Hardware:** ESP32-C3 (esp32-c3-devkitm-1)
**Device-Class:** `net_erl`
**Sensoren:** BME280 (Temp, Feuchte), VEML7700 (Lux), PIR (Bewegung)
**Aktor:** 1 Relais (Schaltausgang)
**Kommunikation:** MQTT direkt (kein ESP-NOW-Master)

## 2. Hardware

| Komponente | Typ | GPIO | Adresse |
|-----------|-----|------|---------|
| BME280 | I2C | SDA=GPIO0, SCL=GPIO1 | 0x76 |
| VEML7700 | I2C | SDA=GPIO0, SCL=GPIO1 | 0x10 |
| PIR (Bewegung) | GPIO | GPIO7 | — |
| Relais | GPIO | GPIO10 | — |
| Setup-Taster | GPIO | GPIO2 | — |

## 3. Konfiguration (YAML-Substitutions)

```yaml
node_name: net-erl-010
device_id: "NET-ERL-010"
device_name: "NET-ERL Hall Module"
device_class: net_erl
power_type: mains
fw_version: "1"
caps: "93"                      # RELAY(1)|TEMP(4)|HUM(8)|LUX(16)|MOTION(64)
control_mode: relay_light
config_profile: hall_light
reporting_mode: hybrid
sensor_mask: "THLMXXXXXX"       # Temp, Hum, Lux, Motion
input_mask: "XXXXX"
setup_status_default: "10"      # Status-Intervall: 10 s
setup_sensor_default: "10"
setup_ap_password: net-erl-setup
relay_pin: GPIO10
pir_pin: GPIO7
setup_button_pin: GPIO2
i2c_sda_pin: GPIO0
i2c_scl_pin: GPIO1
bme280_address: "0x76"
veml7700_address: "0x10"
temp_offset_01c: "0"
hum_offset_01pct: "0"
```

## 4. Sensoren

### 4.1 BME280 (Temperatur, Feuchte)
- **I2C-Adresse**: 0x76
- **Poll-Intervall**: 60 s
- **Oversampling**: 16× (Temperatur + Feuchte)
- **Kein Druck-Reporting**: `pressure_pa` im State-Payload ist immer `null`

### 4.2 VEML7700 (Helligkeit)
- **I2C-Adresse**: 0x10
- **Poll-Intervall**: 60 s

### 4.3 PIR (Bewegung)
- **GPIO**: GPIO7
- **Kein Pull-Up/Down**
- **Entprellzeit**: 35 ms (delayed_on / delayed_off)
- **Event**: `motion_detected` bei Zustandswechsel (event_type=2)

## 5. Relais
- **GPIO**: GPIO10
- **Restore-Mode**: RESTORE_DEFAULT_OFF
- **Steuerung**: per MQTT-Kommando `set_relay`
- **Event**: `relay_changed` bei Schaltvorgang (event_type=6, trigger=2)
- **Kein Auto-Light**: Relais wird manuell oder per Server-Kommando geschaltet; `auto_flags` immer `0`

## 6. MQTT-Topics

| Topic | Retain | Beschreibung |
|-------|:------:|-------------|
| `smarthome/device/NET-ERL-010/meta` | ✅ | Metadaten (Caps, Version, MAC) |
| `smarthome/device/NET-ERL-010/availability` | ✅ | Online/Offline |
| `smarthome/device/NET-ERL-010/state` | ✅ | Sensordaten + Relais-Zustand |
| `smarthome/device/NET-ERL-010/event` | ❌ | Events (motion, relay) |
| `smarthome/device/NET-ERL-010/ack` | ❌ | Kommando-Bestätigung |
| `smarthome/device/NET-ERL-010/command` | — | Kommandos (get_state, set_relay, set_config) |

## 7. State-Payload

```json
{
  "device_id": "NET-ERL-010",
  "relay_1": false,
  "temp_01c": 225,
  "hum_01pct": 480,
  "lux": 320,
  "pressure_pa": null,
  "gas_ohm": null,
  "aqi": null,
  "tvoc_ppb": null,
  "eco2_ppm": null,
  "motion": false,
  "auto_flags": 0,
  "report_interval_s": 10,
  "auto_on_lux_threshold": null,
  "fault": false
}
```

**Null-Werte**: `pressure_pa`, `gas_ohm`, `aqi`, `tvoc_ppb`, `eco2_ppm`, `auto_on_lux_threshold` sind immer `null` (Sensoren nicht vorhanden).

## 8. Events

| Event | event_type | trigger | Bedingung |
|-------|-----------|---------|-----------|
| `motion_detected` | 2 | auto (3) | PIR wechselt Zustand |
| `relay_changed` | 6 | master_cmd (2) | Relais per MQTT-Kommando geschaltet |
| `node_boot` | — | — | Gerät startet (on_boot) |

## 9. Setup-Taster

- **GPIO**: GPIO2, inverted, Pull-Up
- **Entprellzeit**: 35 ms
- **5 Sekunden halten**: Aktiviert Setup-Portal (WiFi-AP, SSID = `net-erl-010-setup`)
- **Setup-Passwort**: `net-erl-setup`

## 10. Sensor-Offset

- `temp_offset_01c`: Temperatur-Korrektur in Zehntelgrad (negativ = nach unten)
- `hum_offset_01pct`: Feuchte-Korrektur in Zehntelprozent
- Offset wird NACH Sensor-Konvertierung, VOR State-Publish angewendet

## 11. Fault-Erkennung

- **hardware_fault**: `true` wenn `!env_has_temp || !env_has_hum || !env_has_lux` (BME280 oder VEML7700 nie erreicht)
- **contract_fault**: `true` bei Kommando-Verarbeitungsfehlern
- **State-JSON `fault`**: `hardware_fault || contract_fault`
- **Design-Limit**: `env_has_*`-Flags sind Einweg-Latches; Sensorausfall NACH Erstkontakt wird nicht erkannt

## 12. Intervalle

- **State-Publish**: alle `${setup_status_default}`s (10 s)
- **Periodischer Refresh**: alle 5 min (Meta, Availability, State)
- **Sensor-Poll**: 60 s (BME280, VEML7700)
