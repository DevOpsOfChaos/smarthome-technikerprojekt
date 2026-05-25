# MQTT Topics & Payloads

## Übersicht

| Topic | Richtung | Retain | Beschreibung |
|-------|----------|:------:|-------------|
| `smarthome/master/{master_id}/status` | Master → Broker | ✅ | Master-Status (WLAN, MQTT, Nodes) |
| `smarthome/master/{master_id}/event` | Master → Broker | ❌ | Master-Ereignisse (boot, mqtt_connected, …) |
| `smarthome/device/{device_id}/meta` | Gerät → Broker | ✅ | Metadaten (Caps, Profile, Version) |
| `smarthome/device/{device_id}/availability` | Gerät → Broker | ✅ | Online/Offline-Status |
| `smarthome/device/{device_id}/state` | Gerät → Broker | ✅ | Live-Zustand (Sensoren, Relais, Cover) |
| `smarthome/device/{device_id}/event` | Gerät → Broker | ❌ | Ereignisse (Button, Bewegung, Fenster, …) |
| `smarthome/device/{device_id}/ack` | Master → Broker | ❌ | Kommando-Bestätigung |
| `smarthome/device/{device_id}/command` | Broker → Gerät | — | Kommandos vom Server |

---

## Geräte-IDs – Zwei getrennte Linien

Das Projekt betreibt zwei alternative Firmware-Linien mit **unterschiedlichen Geräte-IDs**:

| Gerätetyp | ESPHome-Linie (MQTT direkt) | Firmware-Linie (ESP-NOW → Master) |
|-----------|----------------------------|-----------------------------------|
| Flurmodul | `NET-ERL-010` | `NET-ERL-001` |
| Flurmodul LED-Ring | `NET-ERL-020` | `NET-ERL-002` |
| Wetterstation | `NET-SEN-020` | `NET-SEN-002` |
| Rollladen | `NET-ZRL-020` | `NET-ZRL-002` |
| Fensterkontakt | `bat_sen_010` | `bat_sen_01` |
| Regensensor | `bat_sen_020` | `bat_sen_02` |

**Wichtig:** MQTT-Topics sind case-sensitiv!
Die beiden Linien unterscheiden sich im **Suffix** (Ziffernfolge), nicht in der Schreibweise:
- ESPHome verwendet durchgehend **dreistellige** Suffixe (`010`, `020`).
- Firmware verwendet **zweistellige** Suffixe bei `bat_sen` (`01`, `02`) und
  **dreistellige** bei NET-Geräten (`001`, `002`).
- `bat_sen`-Geräte sind in **beiden** Linien klein mit Underscore geschrieben.
- NET-Geräte sind in **beiden** Linien groß mit Bindestrich geschrieben.

Alle folgenden Payload-Beispiele verwenden die **ESPHome-IDs**,
da diese Doku den MQTT-Vertrag der ESPHome-Linie beschreibt.

---

## Meta-Payload

```json
{
  "device_id": "NET-ERL-010",
  "device_name": "NET-ERL Hall Module",
  "device_class": "net_erl",
  "power_type": "mains",
  "fw_version": 1,
  "caps": 93,
  "mac_address": "AA:BB:CC:DD:EE:FF",
  "meta_schema_version": 1,
  "control_mode": "relay_light",
  "config_profile": "hall_light",
  "reporting_mode": "hybrid",
  "sensor_mask": "THLMXXXXXX",
  "input_mask": "XXXXX"
}
```

| Feld | Typ | Beschreibung |
|------|-----|-------------|
| `device_id` | string | Eindeutige Geräte-ID |
| `device_name` | string | Anzeigename |
| `device_class` | string | Geräteklasse: `net_erl`, `net_sen`, `net_zrl`, `bat_sen` |
| `power_type` | string | Stromversorgung: `mains` oder `battery` |
| `fw_version` | int | Firmware-Version (integer) |
| `caps` | int | Fähigkeiten-Bitmaske |
| `mac_address` | string | MAC-Adresse (Format `AA:BB:CC:DD:EE:FF`, kann Leerstring sein) |
| `meta_schema_version` | int | Schema-Version des Meta-Payloads |
| `control_mode` | string | Steuermodus (z.B. `relay_light`, `cover`, `none`) |
| `config_profile` | string | Konfigurationsprofil (z.B. `hall_light`, `cover_basic`) |
| `reporting_mode` | string | Meldeverhalten (z.B. `hybrid`, `sleep_event`) |
| `sensor_mask` | string | 10-stellige Sensormaske |
| `input_mask` | string | 5-stellige Eingabemaske |

### Caps-Bitmaske

| Bit | Konstante | Wert | Bedeutung |
|-----|-----------|-----:|-----------|
| 0 | SH_CAP_RELAY | 1 | Relais 1 |
| 1 | SH_CAP_RELAY2 | 2 | Relais 2 |
| 2 | SH_CAP_TEMP | 4 | Temperatursensor |
| 3 | SH_CAP_HUM | 8 | Feuchtesensor |
| 4 | SH_CAP_LUX | 16 | Helligkeitssensor (VEML7700) |
| 5 | SH_CAP_AQI | 32 | Luftqualität (ENS160) |
| 6 | SH_CAP_MOTION | 64 | Bewegungssensor (PIR/LD2410) |
| 7 | SH_CAP_WINDOW | 128 | Fensterkontakt |
| 8 | SH_CAP_RAIN | 256 | Regensensor |
| 9 | SH_CAP_BATTERY | 512 | Batteriebetrieb |
| 10 | SH_CAP_BUTTON | 1024 | Lokaler Taster |
| 11 | SH_CAP_MULTIBUTTON | 2048 | Mehrfachtaster |
| 12 | SH_CAP_LED_RING | 4096 | NeoPixel LED-Ring |
| 13 | SH_CAP_COVER | 8192 | Rollladensteuerung |
| 14 | SH_CAP_GAS | 16384 | Gassensor (BME680) |
| 15 | SH_CAP_PRESSURE | 32768 | Drucksensor (BME280/BME680) |

### Caps pro Gerät

| Gerät | Caps | Bitmaske |
|-------|-----:|----------|
| `NET-ERL-010` (Flur) | 93 | RELAY(1)+TEMP(4)+HUM(8)+LUX(16)+MOTION(64) |
| `NET-ERL-020` (LED Ring) | 54397 | RELAY(1)+TEMP(4)+HUM(8)+LUX(16)+AQI(32)+MOTION(64)+BUTTON(1024)+LED_RING(4096)+GAS(16384)+PRESSURE(32768) |
| `NET-SEN-020` (Wetter) | 33052 | TEMP(4)+HUM(8)+LUX(16)+RAIN(256)+PRESSURE(32768) |
| `NET-ZRL-020` (Rollladen) | 10243 | RELAY(1)+RELAY2(2)+MULTIBUTTON(2048)+COVER(8192) |
| `bat_sen_010` (Fenster) | 640 | BATTERY(512)+WINDOW(128) |
| `bat_sen_020` (Regen) | 768 | BATTERY(512)+RAIN(256) |

---

## Sensor-Maske

10-stellige Maske (eine Stelle pro Sensorposition). Mögliche Zeichen:
- **T** = Temperatur
- **H** = Feuchte
- **L** = Helligkeit (Lux)
- **P** = Luftdruck
- **G** = Gas
- **A** = Luftqualität (AQI)
- **M** = Bewegung
- **W** = Fenster
- **R** = Regen
- **X** = nicht belegt

---

## State-Payload (nach Geräteklasse)

### NET-ERL (Relais + Sensor)

```json
{
  "device_id": "NET-ERL-010",
  "relay_1": true,
  "temp_01c": 225,
  "hum_01pct": 480,
  "lux": 320,
  "pressure_pa": null,
  "gas_ohm": null,
  "aqi": null,
  "tvoc_ppb": null,
  "eco2_ppm": null,
  "motion": false,
  "fault": false,
  "auto_flags": 0,
  "report_interval_s": 60,
  "auto_on_lux_threshold": null
}
```

**`auto_flags` Bitmaske:**
| Bit | Flag |
|-----|------|
| 0x01 | AUTO_REQUEST_ON - Auto-Light angefordert |
| 0x02 | AUTO_RELAY_OWNED - Relais von Automation gesteuert |
| 0x04 | BLOCKED_BY_SERVER - Server hat Auto-Light deaktiviert |
| 0x08 | BLOCKED_BY_LUX - Zu hell für Auto-Light |
| 0x10 | BLOCKED_BY_MISSING_LUX - Lux-Wert fehlt für Auto-Entscheidung |
| 0x20 | PRESENCE_SOURCE_AVAILABLE - Bewegungssensor vorhanden |
| 0x40 | LIGHT_VALUE_AVAILABLE - Luxsensor vorhanden |
| 0x80 | LIGHT_GUARD_ENABLED - Auto-Light aktiv |

### NET-ZRL (Rollladen)

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

**`cover_state` Werte:**
- `"open"` - ganz oben (position=100)
- `"closed"` - ganz unten (position=0)
- `"opening"` - fährt hoch
- `"closing"` - fährt runter
- `"stopped"` - steht

### NET-SEN (Sensor)

```json
{
  "device_id": "NET-SEN-020",
  "temp_01c": 195,
  "hum_01pct": 620,
  "lux": 15000,
  "pressure_pa": null,
  "gas_ohm": null,
  "aqi": null,
  "tvoc_ppb": null,
  "eco2_ppm": null,
  "motion": false,
  "rain_raw": 0,
  "fault": false
}
```

### bat_sen (Batterie)

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

**`window_open`:** `0` = geschlossen, `1` = offen, `null` = nicht zutreffend (z.B. Regensensor).

**Null-Werte:** Felder die für das jeweilige Gerät nicht zutreffen sind `null` (z.B. `rain_raw` beim Fensterkontakt, `window_open` beim Regensensor).

---

## Event-Payload

```json
{
  "device_id": "NET-ERL-010",
  "event": "motion_detected",
  "event_type": 2,
  "trigger": 3,
  "param1": 1,
  "param2": 0
}
```

### Event-Typen

| event_type | event | Beschreibung |
|----------:|-------|-------------|
| 1 | `button_press` | Taster gedrückt |
| 2 | `motion_detected` | Bewegung erkannt |
| 3 | `window_opened` | Fenster geöffnet |
| 4 | `window_closed` | Fenster geschlossen |
| 5 | `rain_detected` | Regen erkannt |
| 6 | `relay_changed` | Relais geschaltet |
| 7 | `light_auto_on` | Auto-Light ein |
| 8 | `light_auto_off` | Auto-Light aus |
| 9 | `cover_up` | Rollladen hoch |
| 10 | `cover_down` | Rollladen runter |
| 11 | `cover_stop` | Rollladen Stop |
| 12 | `cover_calib_start` | Rollladen-Kalibrierung gestartet |
| 13 | `cover_calib_done` | Rollladen-Kalibrierung abgeschlossen |
| 14 | `node_boot` | Gerät gestartet |
| 15 | `sensor_fault` | Sensorfehler |
| 16 | `comm_fault` | Kommunikationsfehler |
| 17 | `button_release` | Taster losgelassen |
| 18 | `button_long_press` | Taster lang gedrückt (Setup) |

### Trigger

| trigger | Bedeutung |
|--------:|-----------|
| 1 | `manual_button` - lokaler Taster |
| 2 | `master_cmd` - Kommando vom Master/Server |
| 3 | `auto` - automatisch (Bewegung, Fenster, Regen) |
| 4 | `auto_off_timer` - Timeout (Auto-Light aus) |

---

## Command-Payload (Broker → Gerät)

Topic: `smarthome/device/{device_id}/command`

### get_state
```json
{
  "command": "get_state",
  "request_id": "req-123"
}
```
→ Löst State-Report aus. ACK: `{"status":"sent","status_code":0}`

### set_relay
```json
{
  "command": "set_relay",
  "request_id": "req-124",
  "relay_1": true
}
```
→ Schaltet Relais. ACK: `{"status":"ok","status_code":0}`

### Cover-Kommandos
```json
{"command": "open", "request_id": "req-125"}
{"command": "close", "request_id": "req-126"}
{"command": "stop", "request_id": "req-127"}
{"command": "set_position", "request_id": "req-128", "position": 75}
```

### set_config
```json
{
  "command": "set_config",
  "request_id": "req-129",
  "values": {"report_interval_s": 30}
}
```

**Hinweis:** ESPHome-Geräte akzeptieren in `set_config` derzeit nur `master_mac`.
Weitere Konfigurationswerte (`report_interval_s`, `auto_on_lux_threshold` etc.)
werden nur über den Firmware-Master-Pfad (ESP-NOW) unterstützt.

## ACK-Payload (Master → Broker)

```json
{
  "device_id": "NET-ERL-010",
  "request_id": "req-123",
  "channel": "command",
  "status": "ok",
  "status_code": 0,
  "source": "node_ack"
}
```

**`status` Werte:**

| status | Bedeutung | Master Firmware | ESPHome | master_compat |
|--------|-----------|:-:|:-:|:-:|
| `ok` | Erfolg | 0 | 0 | 0 |
| `sent` | Gesendet (get_state) | 0 | 0 | 0 |
| `busy` | Gerät hat noch offenen Auftrag | -2 | -2 | -2 |
| `timeout` | Keine ACK-Antwort erhalten | 3 | — | 3 |
| `unsupported` | Kommando nicht unterstützt | **-3** | **-2** | **-2** |
| `invalid_payload` | Ungültiger Payload (JSON-Fehler, fehlende Felder) | -21 | -21 | -21 |
| `send_failed` | ESP-NOW-Versand fehlgeschlagen | -4 | — | -4 |
| `not_calibrated` | Rollladen nicht kalibriert | -5 | -5 | -5 |
| `unknown_device` | Gerät nicht registriert | -6 | -6 | -6 |
| `registry_full` | Registry voll (max. 16 Nodes) | -7 | — | — |
| `no_route` | MAC-Adresse unbekannt | -8 | — | — |
| `meta_required` | HELLO-Metadaten fehlen | -9 | — | — |
| `missing_request_id` | request_id fehlt im Payload | — | -20 | -20 |

> **Hinweis zu abweichenden Codes:** Die drei Linien verwenden teils
> unterschiedliche Codes für dieselbe Bedeutung.
> - `unsupported`: Master Firmware `-3`, ESPHome/master_compat `-2`
> - `invalid_payload`: alle Linien verwenden `-21`
> - `—` = Code wird von dieser Linie nicht verwendet.

## Availability-Payload

```json
{
  "device_id": "NET-ERL-010",
  "online": true
}
```
- `online: true` → Gerät ist per MQTT erreichbar
- `online: false` → Gerät per Last-Will als offline gemeldet

---

## Sensor-Offsets

Jedes Gerät unterstützt Temperatur- und Feuchte-Offsets zur Kompensation von Einbaufehlern:

| Gerät | temp_offset_01c | hum_offset_01pct |
|-------|----------------:|-----------------:|
| `NET-ERL-010` (Flur) | 0 | 0 |
| `NET-ERL-020` (LED Ring) | 0 | 0 |
| `NET-SEN-020` (Wetter) | 0 | 0 |

Offsets werden in Geräte-Nativeinheiten angegeben (Zehntelgrad, Zehntelprozent).
Negativer Wert = Korrektur nach unten. Positiver Wert = Korrektur nach oben.

Beispiel: BME280 nahe Netzteil misst 30 °C bei 22 °C Raumtemperatur → `temp_offset_01c = -80` (-8,0 °C).

---

## Topic-Struktur (Zusammenfassung)

```
smarthome/
├── master/{master_id}/
│   ├── status          (retained, Master-Status)
│   └── event           (transient, Master-Ereignisse)
└── device/{device_id}/
    ├── meta            (retained, Metadaten)
    ├── availability    (retained, Online/Offline)
    ├── state           (retained, Live-Zustand)
    ├── event           (transient, Ereignisse)
    ├── ack             (transient, Kommando-Bestätigung)
    └── command         (Eingang, Kommandos vom Server)
```

---

*Stand: 2026-05-24 – nach Angleichung Firmware/ESPHome*
