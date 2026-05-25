# bat_sen_010 – Window Contact (ESPHome)

## 1. Übersicht

**ESPHome-ID:** `bat_sen_010`
**Typ:** Batteriebetrieb (battery)
**Hardware:** ESP32-C3 (esp32-c3-devkitm-1)
**Device-Class:** `bat_sen`
**Sensor:** Reed-Kontakt (Fenster offen/geschlossen), Batterie-ADC
**Kommunikation:** MQTT direkt, Deep-Sleep

## 2. Hardware

| Komponente | GPIO | Funktion |
|-----------|------|----------|
| Reed-Kontakt | GPIO3 | Fenster offen/geschlossen (Pull-Up) |
| Setup-Taster | GPIO2 | Setup-Modus (5 s halten) |
| Batterie-ADC | GPIO4 | Spannungsmessung (12 dB Dämpfung) |

## 3. Konfiguration (YAML-Substitutions)

```yaml
node_name: bat-sen-010
device_id: "bat_sen_010"
device_name: "BAT-SEN Window"
device_class: bat_sen
power_type: battery
fw_version: "1"
caps: "640"                          # BATTERY(512)|WINDOW(128)
control_mode: none
config_profile: none
reporting_mode: sleep_event
sensor_mask: "WXXXXXXXXX"
input_mask: "XXXXX"
setup_ap_password: bat-sen-setup
setup_status_default: "900"          # 15 min Wake-Intervall
setup_sensor_default: "5000"         # 5 s RX-Fenster
setup_status_min: "30"
setup_status_max: "65535"
setup_sensor_min: "500"
setup_sensor_max: "60000"
contact_pin: GPIO3
battery_pin: GPIO4
setup_button_pin: GPIO2
rx_window_ms: "5000"
```

## 4. Deep-Sleep-Zyklus

```
Wake (Timer oder GPIO-Interrupt)
  ├── Sensoren lesen (Reed + Battery-ADC)
  ├── WiFi verbinden (fast_connect, power_save_mode: LIGHT)
  ├── MQTT verbinden (Birth/Will)
  ├── on_connect: Meta + Availability + State publizieren
  ├── State + Event publizieren (wenn Fensterzustand geändert)
  ├── RX-Fenster (5 s): auf Kommandos warten
  └── Deep-Sleep (900 s)
```

**Wake-Quellen:**
- Timer: 900 s Intervall
- Reed-Kontakt GPIO3 (INVERT_WAKEUP): Wake bei Zustandswechsel
- Setup-Taster GPIO2 (manuell)

**Deep-Sleep-Parameter:**
- `run_duration`: `${rx_window_ms}ms` (5.000 ms)
- `sleep_duration`: 900 s (15 min)
- `wakeup_pin`: GPIO3 (contact_pin), `INVERT_WAKEUP`

## 5. MQTT-Topics

| Topic | Retain | Beschreibung |
|-------|:------:|-------------|
| `smarthome/device/bat_sen_010/meta` | ✅ | Metadaten |
| `smarthome/device/bat_sen_010/availability` | ✅ | Online/Offline |
| `smarthome/device/bat_sen_010/state` | ✅ | Batterie + Fensterzustand |
| `smarthome/device/bat_sen_010/event` | ❌ | window_opened / window_closed |
| `smarthome/device/bat_sen_010/command` | — | get_state, set_config |
| `smarthome/device/bat_sen_010/ack` | ❌ | Kommando-Bestätigung |

**Hinweis:** `on_connect` publiziert Meta, Availability und State erneut, da `on_boot` vor der MQTT-Verbindung feuert.

## 6. State-Payload

```json
{
  "device_id": "bat_sen_010",
  "battery_pct": 85,
  "battery_mv": 2900,
  "window_open": 0,
  "rain_raw": null,
  "button_flags": 0,
  "fault": false
}
```

**window_open:** Integer 0/1 (nicht bool), wie Firmware-Protokoll.

## 7. Batterie

- **ADC**: GPIO4, 12 dB Dämpfung (Messbereich 0–3,3 V)
- **ADC-Poll**: 1 s (während Run-Duration)
- **Spannungsteiler**: 1:1 (Faktor 2), Kalibrierfaktor 1,078
- **CR2032-Profil**: 2200 mV (leer) – 3000 mV (voll)
- **Berechnung**: `pct = (mv - 2200) * 100 / (3000 - 2200)`, clamped auf 0–100

## 8. Events

| Event | event_type | trigger | Bedingung |
|-------|-----------|---------|-----------|
| `window_opened` | 3 | auto (3) | Reed-Kontakt schließt |
| `window_closed` | 4 | auto (3) | Reed-Kontakt öffnet |
| `node_boot` | — | — | Wake aus Deep-Sleep (on_boot) |

## 9. Fault-Erkennung

- **hardware_fault**: immer `false` (keine diagnostischen Sensoren)
- **contract_fault**: `true` bei Kommando-Fehlern
- `battery_pct`/`battery_mv`: `null` wenn `battery_has_value == false`

## 10. Setup-Taster

- **GPIO**: GPIO2, kein Pull-Up/Down
- **Entprellzeit**: 35 ms
- **5 Sekunden halten**: Aktiviert Setup-Portal (WiFi-AP, SSID = `bat-sen-010-setup`)
- **Setup-Passwort**: `bat-sen-setup`
- **Deep-Sleep-Prevent**: Solange Taster gehalten, kein Sleep
