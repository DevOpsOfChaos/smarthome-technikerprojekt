# bat_sen_020 – Rain Sensor (ESPHome)

## 1. Übersicht

**ESPHome-ID:** `bat_sen_020`
**Typ:** Batteriebetrieb (battery)
**Hardware:** ESP32-C3 (esp32-c3-devkitm-1)
**Device-Class:** `bat_sen`
**Sensor:** Regensensor (analog, ADC), Batterie-ADC
**Kommunikation:** MQTT direkt, Deep-Sleep

## 2. Hardware

| Komponente | GPIO | Funktion |
|-----------|------|----------|
| Regen-ADC | GPIO3 | Analoger Feuchtewert (0–4095, 12-Bit) |
| Setup-Taster | GPIO2 | Setup-Modus (5 s halten) |
| Batterie-ADC | GPIO4 | Spannungsmessung (12 dB Dämpfung) |

## 3. Konfiguration (YAML-Substitutions)

```yaml
node_name: bat-sen-020
device_id: "bat_sen_020"
device_name: "BAT-SEN Rain"
device_class: bat_sen
power_type: battery
fw_version: "1"
caps: "768"                              # BATTERY(512)|RAIN(256)
control_mode: none
config_profile: none
reporting_mode: sleep_event
sensor_mask: "RXXXXXXXXX"
input_mask: "XXXXX"
setup_ap_password: bat-sen-setup
setup_status_default: "900"              # 15 min Wake-Intervall
setup_sensor_default: "5000"             # 5 s RX-Fenster
setup_status_min: "30"
setup_status_max: "65535"
setup_sensor_min: "500"
setup_sensor_max: "60000"
rain_pin: GPIO3
battery_pin: GPIO4
setup_button_pin: GPIO2
rain_wet_threshold_raw: "2200"           # ADC-Rohwert: ab hier nass
rain_clear_threshold_raw: "2050"         # ADC-Rohwert: ab hier trocken (Hysterese 150)
wake_interval_s: "900"
rx_window_ms: "5000"
```

## 4. Deep-Sleep-Zyklus

```
Wake (Timer)
  ├── Sensoren lesen (Regen-ADC + Battery-ADC)
  ├── WiFi verbinden (fast_connect, power_save_mode: LIGHT)
  ├── MQTT verbinden (Birth/Will)
  ├── on_connect: Meta + Availability + State publizieren
  ├── State + Event publizieren (wenn Regenzustand geändert)
  ├── RX-Fenster (5 s): auf Kommandos warten
  └── Deep-Sleep (900 s)
```

**Wake-Quellen:**
- Timer: `${wake_interval_s}` (900 s = 15 min)
- Setup-Taster GPIO2 (manuell)

**Deep-Sleep-Parameter:**
- `run_duration`: `${rx_window_ms}ms` (5.000 ms)
- `sleep_duration`: `${wake_interval_s}s` (900 s)

## 5. Regensensor

- **ADC**: GPIO3, 12 dB Dämpfung (Messbereich 0–3,3 V)
- **ADC-Poll**: 1 s (während Run-Duration)
- **Auflösung**: 12 Bit (0–4095)
- **Nass-Schwelle**: 2200 (ADC-Rohwert)
- **Trocken-Schwelle**: 2050 (150 Punkte Hysterese)
- **Hysterese**: Verhindert Flattern bei Grenzwerten
- **Event**: `rain_detected` nur bei tatsächlichem Zustandswechsel (event_type=5, trigger=3)

## 6. Batterie

- **ADC**: GPIO4, 12 dB Dämpfung (Messbereich 0–3,3 V)
- **ADC-Poll**: 1 s
- **Spannungsteiler**: 1:1 (Faktor 2), Kalibrierfaktor 1,078
- **2× AA-Profil**: 2000 mV (leer) – 3200 mV (voll)
- **Berechnung**: `pct = (mv - 2000) * 100 / (3200 - 2000)`, clamped auf 0–100

## 7. MQTT-Topics

| Topic | Retain | Beschreibung |
|-------|:------:|-------------|
| `smarthome/device/bat_sen_020/meta` | ✅ | Metadaten |
| `smarthome/device/bat_sen_020/availability` | ✅ | Online/Offline |
| `smarthome/device/bat_sen_020/state` | ✅ | Batterie + Regen-Rohwert |
| `smarthome/device/bat_sen_020/event` | ❌ | rain_detected |
| `smarthome/device/bat_sen_020/command` | — | get_state, set_config |
| `smarthome/device/bat_sen_020/ack` | ❌ | Kommando-Bestätigung |

## 8. State-Payload

```json
{
  "device_id": "bat_sen_020",
  "battery_pct": 72,
  "battery_mv": 2600,
  "window_open": null,
  "rain_raw": 1845,
  "button_flags": 0,
  "fault": false
}
```

## 9. Events

| Event | event_type | trigger | Bedingung |
|-------|-----------|---------|-----------|
| `rain_detected` | 5 | auto (3) | ADC überschreitet/unterschreitet Schwelle (mit Hysterese) |
| `node_boot` | — | — | Wake aus Deep-Sleep (on_boot) |

## 10. Fault-Erkennung

- **hardware_fault**: immer `false` (keine diagnostischen Sensoren)
- **contract_fault**: `true` bei Kommando-Fehlern
- Regen-ADC: Rohwert (`rain_raw`) wird direkt gesendet; Server interpretiert mit Schwellwerten
- `battery_pct`/`battery_mv`: `null` wenn `battery_has_value == false`

## 11. Setup-Taster

- **GPIO**: GPIO2, kein Pull-Up/Down
- **Entprellzeit**: 35 ms
- **5 Sekunden halten**: Aktiviert Setup-Portal (WiFi-AP, SSID = `bat-sen-020-setup`)
- **Setup-Passwort**: `bat-sen-setup`
- **Deep-Sleep-Prevent**: Solange Taster gehalten, kein Sleep
