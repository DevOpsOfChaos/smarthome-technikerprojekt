# NET-SEN-020 – Env BME280+VEML+Rain (ESPHome)

## 1. Übersicht

**ESPHome-ID:** `NET-SEN-020`
**Typ:** Netzbetrieb (mains)
**Hardware:** ESP32-C3 (esp32-c3-devkitm-1)
**Device-Class:** `net_sen`
**Sensoren:** BME280 (Temp, Feuchte, Druck), VEML7700 (Lux), Regen (GPIO)
**Aktor:** Keiner (reiner Sensor, `control_mode: none`)
**Kommunikation:** MQTT direkt

## 2. Hardware

| Komponente | Typ | GPIO | Adresse |
|-----------|-----|------|---------|
| BME280 | I2C | SDA=GPIO0, SCL=GPIO1 | 0x76 |
| VEML7700 | I2C | SDA=GPIO0, SCL=GPIO1 | 0x10 |
| Regen-Sensor | GPIO | GPIO3 | — |
| Setup-Taster | GPIO | GPIO2 | — |

## 3. Konfiguration (YAML-Substitutions)

```yaml
node_name: net-sen-020
device_id: "NET-SEN-020"
device_name: "NET-SEN Env BME280+VEML+Rain"
device_class: net_sen
power_type: mains
fw_version: "1"
caps: "33052"                        # TEMP(4)|HUM(8)|LUX(16)|RAIN(256)|PRESSURE(32768)
control_mode: none
config_profile: none
reporting_mode: hybrid
sensor_mask: "THPLRXXXXX"            # Temp, Hum, Pressure, Lux, Rain
input_mask: "XXXXX"
setup_status_default: "10"
setup_sensor_default: "10"
setup_ap_password: net-sen-setup
i2c_sda_pin: GPIO0
i2c_scl_pin: GPIO1
rain_pin: GPIO3
setup_button_pin: GPIO2
bme280_address: "0x76"
veml7700_address: "0x10"
temp_offset_01c: "0"
hum_offset_01pct: "0"
```

## 4. Sensoren

### 4.1 BME280 (Temperatur, Feuchte, Druck)
- **I2C-Adresse**: 0x76
- **Poll-Intervall**: 60 s
- **Oversampling**: 16× (Temperatur, Feuchte, Druck)

### 4.2 VEML7700 (Helligkeit)
- **I2C-Adresse**: 0x10
- **Poll-Intervall**: 60 s

### 4.3 Regen-Sensor (GPIO)
- **GPIO**: GPIO3, Pull-Up, invertiert
- **Entprellzeit**: 35 ms
- **Event**: `rain_detected` bei Zustandswechsel (event_type=5, trigger=3)

## 5. MQTT-Topics

| Topic | Retain | Beschreibung |
|-------|:------:|-------------|
| `smarthome/device/NET-SEN-020/meta` | ✅ | Metadaten |
| `smarthome/device/NET-SEN-020/availability` | ✅ | Online/Offline |
| `smarthome/device/NET-SEN-020/state` | ✅ | Temp, Feuchte, Druck, Lux, Regen |
| `smarthome/device/NET-SEN-020/event` | ❌ | Events (rain_detected) |
| `smarthome/device/NET-SEN-020/ack` | ❌ | Kommando-Bestätigung |
| `smarthome/device/NET-SEN-020/command` | — | Kommandos (get_state, set_config) |

## 6. State-Payload

```json
{
  "device_id": "NET-SEN-020",
  "temp_01c": 195,
  "hum_01pct": 620,
  "lux": 15000,
  "pressure_pa": 101325,
  "gas_ohm": null,
  "aqi": null,
  "tvoc_ppb": null,
  "eco2_ppm": null,
  "motion": false,
  "rain_raw": 1,
  "fault": false
}
```

**Null-Werte**: `gas_ohm`, `aqi`, `tvoc_ppb`, `eco2_ppm` sind immer `null` (Sensoren nicht vorhanden). `motion` ist immer `false`.

## 7. Events

| Event | event_type | trigger | Bedingung |
|-------|-----------|---------|-----------|
| `rain_detected` | 5 | auto (3) | Regen-Pin wechselt Zustand |
| `node_boot` | — | — | Gerät startet (on_boot) |

## 8. Fault-Erkennung

- **hardware_fault**: `true` wenn `!env_has_temp || !env_has_hum || !env_has_pressure || !env_has_lux` (BME280 oder VEML7700 nie erreicht)
- Regen-Sensor (GPIO) hat keine Fehlererkennung
- **Design-Limit**: `env_has_*`-Flags sind Einweg-Latches

## 9. Setup-Taster

- **GPIO**: GPIO2, inverted, Pull-Up
- **Entprellzeit**: 35 ms
- **5 Sekunden halten**: Aktiviert Setup-Portal (WiFi-AP, SSID = `net-sen-020-setup`)
- **Setup-Passwort**: `net-sen-setup`

## 10. Intervalle

- **State-Publish**: alle `${setup_status_default}`s (10 s)
- **Periodischer Refresh**: alle 5 min (Meta, Availability, State)
- **Sensor-Poll**: 60 s (BME280, VEML7700)
