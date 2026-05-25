# NET-ERL-020 – Hall Module LED Ring (ESPHome)

## 1. Übersicht

**ESPHome-ID:** `NET-ERL-020`
**Typ:** Netzbetrieb (mains)
**Hardware:** ESP32-C3 (esp32-c3-devkitm-1)
**Device-Class:** `net_erl`
**Sensoren:** BME680 (Temp, Feuchte, Druck, Gas), VEML7700 (Lux), ENS160 (AQI, TVOC, eCO2), PIR (Bewegung)
**Aktor:** 1 Relais + 17 NeoPixel LED-Ring
**Kommunikation:** MQTT direkt (kein ESP-NOW-Master)

## 2. Hardware

| Komponente | Typ | GPIO | Adresse |
|-----------|-----|------|---------|
| BME680 | I2C | SDA=GPIO0, SCL=GPIO1 | 0x76 |
| VEML7700 | I2C | SDA=GPIO0, SCL=GPIO1 | 0x10 |
| ENS160 | I2C | SDA=GPIO0, SCL=GPIO1 | 0x52 |
| PIR (Präsenz) | GPIO | GPIO7 | — |
| Relais | GPIO | GPIO10 | — |
| NeoPixel LED-Ring | GPIO | GPIO4 | 17 LEDs (WS2812, GRB) |
| Lokaler Taster | GPIO | GPIO6 | — |

## 3. Konfiguration (YAML-Substitutions)

```yaml
node_name: net-erl-020
device_id: "NET-ERL-020"
device_name: "NET-ERL Hall Module LED Ring"
device_class: net_erl
power_type: mains
fw_version: "1"
caps: "54397"                        # RELAY(1)|TEMP(4)|HUM(8)|LUX(16)|AQI(32)|MOTION(64)|BUTTON(1024)|LED_RING(4096)|GAS(16384)|PRESSURE(32768)
control_mode: relay_light
config_profile: hall_module_led_ring
reporting_mode: hybrid
sensor_mask: "THLPGAMXXX"            # Temp, Hum, Lux, Pressure, Gas, AQI, Motion
input_mask: "BXXXX"
setup_status_default: "10"
setup_sensor_default: "10"
setup_ap_password: net-erl-setup
report_interval_s: "10"
auto_on_lux_threshold: "250"
auto_off_delay_s: "15"
relay_pin: GPIO10
button_pin: GPIO6
presence_pin: GPIO7
led_ring_pin: GPIO4
led_ring_count: "17"
led_ring_default_brightness_pct: "18"
led_ring_air_quality_phase_s: "15"
led_ring_temperature_phase_s: "15"
led_ring_humidity_phase_s: "15"
i2c_sda_pin: GPIO0
i2c_scl_pin: GPIO1
bme680_address: "0x76"
veml7700_address: "0x10"
ens160_address: "0x52"
temp_offset_01c: "0"
hum_offset_01pct: "0"
```

## 4. Sensoren

### 4.1 BME680 (Temperatur, Feuchte, Druck, Gas)
- **I2C-Adresse**: 0x76
- **Poll-Intervall**: 60 s
- **Oversampling**: 8× (Temperatur), 2× (Feuchte), 4× (Druck)
- **IIR-Filter**: 3×
- **Gas-Warmup**: 3 Minuten (180.000 ms) vor erstem gültigen Wert
- **Gas-Messungen**: Mindestens 5 gültige Messungen (`bme680_gas_read_count >= 5`) vor Report
- **Sentinel-Werte**: `temp_01c = -32768`, `hum_01pct = 65535`, `pressure_pa = 4294967295`, `gas_ohm = 4294967295` bei fehlendem Wert

### 4.2 VEML7700 (Helligkeit)
- **I2C-Adresse**: 0x10
- **Poll-Intervall**: 60 s
- **Gain**: 1×, **Integration Time**: 400 ms

### 4.3 ENS160 (Luftqualität)
- **I2C-Adresse**: 0x52
- **Poll-Intervall**: 60 s
- **Warmup**: 3 Minuten (180.000 ms)
- **Werte**: AQI (1-5 Rohwert, gemappt auf 100-500), TVOC (ppb), eCO2 (ppm)
- **AQI-Mapping**: `raw_aqi * 100` (1→100, 2→200, 3→300, 4→400, 5→500)

### 4.4 PIR (Präsenz/Bewegung)
- **GPIO**: GPIO7 (als `presence_input`)
- **Kein Pull-Up/Down**

## 5. LED-Ring

- **17 NeoPixel** (WS2812, GRB) an GPIO4
- **Helligkeit**: 18 % Standard (konfigurierbar 1–60 %)
- **Phasen (45 s Zyklus)**: AQI (15 s) → Temperatur (15 s) → Feuchte (15 s)
- **Nur die AQI-Phase ist Teil der Technikerarbeit**; Temperatur- und Feuchtephasen sind optionale ESPHome-Komfortfunktionen
- **Offline-Anzeige**: Blinken blau (500 ms)
- **Hardware-Fault**: Rot dauerhaft
- **Alert-Code 1 (Lux-Blockade)**: Gelb/Orange Sweep (120 ms)
- **Alert-Code 2 (ACK-OK)**: Grüner Sinus-Puls (120 ms)
- **Relais-Toggle**: Kurzer Tastendruck (< 5 s) toggelt Relais

## 6. Auto-Light

Integrierte Auto-Light-Logik mit manuellem Override:

1. **Bewegung** (PIR GPIO7) → `handle_motion_on()`
2. **Lux-Prüfung**: `evaluate_auto_on_from_lux()`
   - Lux ≤ 250 (Schwelle) → Relais ein (`light_auto_on` Event, event_type=7)
   - Lux > 250 → `blocked_by_lux`, LED-Ring warnt
3. **Timeout**: 15 s nach letzter Bewegung → Relais aus (`light_auto_off` Event, event_type=8)
4. **Manuelles Override**: Lokaler Taster deaktiviert Auto-Light
   - **Manuell EIN**: Follow-Motion für 30 min (Timeout 1.800.000 ms); danach 15 s Probe-Phase
   - **Manuell AUS**: Explicit-Off-Hold solange Präsenz HIGH ist

### auto_flags (im State-Payload)

| Bit | Wert | Flag | Beschreibung |
|-----|------|------|-------------|
| 0 | 0x01 | AUTO_REQUEST_ON | Auto-Light-Einschaltvorgang läuft (`pending_auto_on_decision`) |
| 1 | 0x02 | AUTO_RELAY_OWNED | Relais von Automation gesteuert (`relay_auto_owned`) |
| 3 | 0x08 | BLOCKED_BY_LUX | Helligkeit zu hoch für Auto-Light |
| 5 | 0x20 | PRESENCE_SOURCE_AVAILABLE | Bewegungssensor vorhanden (immer gesetzt) |
| 6 | 0x40 | LIGHT_VALUE_AVAILABLE | Luxsensor liefert Werte (`env_has_lux`) |
| 7 | 0x80 | LIGHT_GUARD_ENABLED | Auto-Light-Funktion aktiviert (immer gesetzt) |

## 7. MQTT-Topics

| Topic | Retain | Beschreibung |
|-------|:------:|-------------|
| `smarthome/device/NET-ERL-020/meta` | ✅ | Metadaten |
| `smarthome/device/NET-ERL-020/availability` | ✅ | Online/Offline |
| `smarthome/device/NET-ERL-020/state` | ✅ | Sensordaten + Relais + auto_flags |
| `smarthome/device/NET-ERL-020/event` | ❌ | Events (motion, relay, light_auto_on/off) |
| `smarthome/device/NET-ERL-020/ack` | ❌ | Kommando-Bestätigung |
| `smarthome/device/NET-ERL-020/command` | — | Kommandos (get_state, set_relay, set_config) |

## 8. State-Payload

```json
{
  "device_id": "NET-ERL-020",
  "relay_1": false,
  "temp_01c": 225,
  "hum_01pct": 480,
  "lux": 320,
  "pressure_pa": 101325,
  "gas_ohm": 50000,
  "aqi": 200,
  "tvoc_ppb": 150,
  "eco2_ppm": 600,
  "motion": false,
  "auto_flags": 224,
  "report_interval_s": 10,
  "auto_on_lux_threshold": 250,
  "fault": false
}
```

**Sentinel-Werte** (`-32768`, `65535`, `4294967295`) werden als JSON-`null` ausgegeben.

## 9. Events

| Event | event_type | trigger | Bedingung |
|-------|-----------|---------|-----------|
| `motion_detected` | 2 | auto (3) | PIR erkennt Bewegung |
| `relay_changed` | 6 | manual_button (1) / master_cmd (2) | Relais geschaltet |
| `light_auto_on` | 7 | auto (3) | Auto-Light schaltet Relais ein |
| `light_auto_off` | 8 | auto_off_timer (4) | Auto-Light Timeout |
| `node_boot` | — | — | Gerät startet (on_boot) |

## 10. Fault-Erkennung

- **hardware_fault**: `true` wenn `!env_has_temp || !env_has_hum || !env_has_lux || (millis() >= 180000 && !env_has_air)`
- **contract_fault**: `true` bei Kommando-Fehlern
- **ENS160-Warmup**: 3 min; `env_has_air` wird erst danach gesetzt

## 11. Taster

- **GPIO**: GPIO6 (inverted, Pull-Up)
- **Entprellzeit**: 40 ms
- **Kurzer Druck (< 5 s)**: Relais toggeln (manuelles Override)
- **5 Sekunden halten**: Setup-Portal (WiFi-AP, SSID = `net-erl-020-setup`)

## 12. Intervalle

- **State-Publish**: alle 10 s (`report_interval_s`)
- **Periodischer Refresh**: alle 5 min (Meta, Availability, State)
- **Sensor-Poll**: 60 s (BME680, VEML7700, ENS160)
- **Auto-Light-Tick**: alle 1 s (Präsenz-Prüfung, Motion-Timeout)
