# ESPHome Zweitlinie

Dieser Pfad bereitet einen separaten, additiven ESPHome-Geraetepfad vor, der denselben MQTT-/JSON-Vertrag wie der bestehende kompatible Firmwarepfad spricht.

Wichtig:
- Der Kern des Technikerprojekts bleibt unveraendert.
- Der bestehende Masterpfad bleibt die Hauptlinie fuer die eigene Firmware.
- Das bestehende ESP-NOW-Protokoll bleibt unberuehrt.
- Der Serververtrag bleibt unveraendert.
- Dieser ESPHome-Pfad ist vorbereitet, aber nicht hardwarevalidiert.
- **Diese Geraete arbeiten nur mit dem neuen `master_compat` MQTT-Bridge-Pfad** (nicht mit dem Legacy ESP-NOW Master).

## Zweck

Die Dateien unter `esphome/` zeigen, wie vertragskompatible MQTT-Geraete als alternativer Geraetepfad aufgebaut werden koennen, ohne den bisherigen Firmware-, Master- oder Serverkern umzubauen.

Geeignet als:
- kompatibler Firmwarepfad fuer einfache MQTT-Geraete
- alternativer Geraetepfad fuer getrennte ESPHome-Linien
- Beispielbasis fuer vertragskompatible MQTT-Geraete

Nicht gemeint als:
- Ersatz fuer den bestehenden Master
- Eingriff in die bestehende ESP-NOW-Linie
- Behauptung produktiver Hardwarefreigabe
- Direkte Kommunikation mit dem Legacy ESP-NOW Master (benoetigt `master_compat`)

## Topic-Uebersicht

- `smarthome/master/<master_id>/status`
- `smarthome/device/<device_id>/meta`
- `smarthome/device/<device_id>/availability`
- `smarthome/device/<device_id>/state`
- `smarthome/device/<device_id>/command`
- `smarthome/device/<device_id>/ack`

## Vertragsregeln

- `meta`, `availability` und `state` werden retained publiziert.
- `ack` wird nicht retained publiziert.
- Commands muessen `request_id` enthalten.
- ACKs geben `request_id` wieder zurueck.
- Commands ohne `request_id` werden nicht als erfolgreicher Befehl behandelt.

## Beispielpayloads

### Meta

```json
{
  "device_id": "esp_net_erl_light_01",
  "device_name": "ESP NET ERL Light Example",
  "device_class": "net_erl",
  "power_type": "mains",
  "fw_version": "esphome-example",
  "caps": 1,
  "meta_schema_version": 1,
  "control_mode": "relay_light",
  "config_profile": "none",
  "reporting_mode": "hybrid",
  "sensor_mask": "XXXXXXXXXX",
  "input_mask": "XXXXX"
}
```

### Availability

```json
{
  "device_id": "esp_net_erl_light_01",
  "availability": "online",
  "online": true,
  "power_type": "mains"
}
```

### State Relay

```json
{
  "device_id": "esp_net_erl_light_01",
  "relay_1": true,
  "fault": false
}
```

### State Sensor

```json
{
  "device_id": "esp_net_sen_env_01",
  "temp_01c": 231,
  "hum_01pct": 487,
  "fault": false
}
```

### State Cover

```json
{
  "device_id": "esp_net_zrl_cover_01",
  "cover_state": "stopped",
  "cover_calibrated": false,
  "fault": false
}
```

Wenn `cover_calibrated = true` und eine belastbare Position vorliegt, kommt zusaetzlich `cover_position` als Ganzzahl `0..100` dazu.

### State Battery Sensor (Window Contact)

```json
{
  "device_id": "bat_sen_window_contact_01",
  "contact_open": false,
  "fault": false
}
```

### State Battery Sensor (Rain)

```json
{
  "device_id": "bat_sen_rain_01",
  "rain_detected": true,
  "fault": false
}
```

### Command

```json
{
  "device_id": "esp_net_erl_light_01",
  "request_id": "req-123",
  "command": "set_relay",
  "relay_1": true
}
```

### ACK

```json
{
  "device_id": "esp_net_erl_light_01",
  "request_id": "req-123",
  "channel": "command",
  "status": "ok",
  "status_code": 0,
  "ack_msg_type": 5,
  "ack_seq": 1,
  "source": "esphome_command"
}
```

## Retained-Hinweis

Der Serverpfad arbeitet mit Snapshot-Daten. Deshalb muessen `meta`, `availability` und `state` retained bleiben. `ack` bleibt absichtlich nicht retained, damit technische Rueckmeldungen nicht als aktueller Geraetezustand missverstanden werden.

## Cover-Kalibrierung (NEU)

Der Cover-Vertrag unterstuetzt jetzt eine manuelle Kalibrierung der Fahrzeiten:

1. **Kalibrierung starten**: Command `calibrate` senden
2. **Auffahrt messen**: Cover faehrt hoch (max. 180s). Sobald die obere Endlage erreicht ist, `stop` senden
3. **Abfahrt messen**: Erneut `calibrate` senden, Cover faehrt runter. Bei Erreichen der unteren Endlage erneut `stop` senden
4. **Speichern**: Erneut `calibrate` senden, die gemessenen Fahrzeiten werden persistent gespeichert und `cover_calibrated = true` gesetzt
5. **Kalibrierung loeschen**: Command `clear_calibration` setzt den Cover zurueck

Nach erfolgreicher Kalibrierung:
- `cover_position` (0-100) wird im State-Payload mitgeliefert
- `set_position` mit Zielposition 0-100 funktioniert (zeitbasierte Positionsschaetzung)
- Endlagen (0/100) funktionieren auch unkalibriert

**WARNUNG**: Waehrend der Kalibrierung muss der Benutzer die Cover-Bewegung ueberwachen und manuell stoppen. Es gibt keinen automatischen Endlagenschutz - mechanische Endschalter sind fuer den Produktiveinsatz notwendig.

## Dateien

### Packages (gemeinsame Vertragsbausteine)
- `packages/smarthome_contract_base.yaml`: gemeinsame Vertragsbasis fuer Meta, Availability und Statusfehler
- `packages/smarthome_command_ack.yaml`: ACK-Helfer fuer den Command-Pfad
- `packages/smarthome_relay_contract.yaml`: Relais-Beispielvertrag mit `set_relay` und `get_state`
- `packages/smarthome_cover_contract.yaml`: Cover-Vertrag mit `open`, `close`, `stop`, `set_position`, Kalibrierung und Positionsregel

### Devices - Mains (netzbetrieben)
- `devices/esp_net_erl_light_example.yaml`: einfacher Relais-Beispielpfad
- `devices/esp_net_sen_env_example.yaml`: einfacher Temperatur-/Feuchte-Beispielpfad (DHT22)
- `devices/esp_net_sen_env_bme280.yaml`: BME280 (Temperatur, Feuchte, Luftdruck)
- `devices/esp_net_sen_env_bme680.yaml`: BME680 (Temperatur, Feuchte, Luftdruck, Gaswiderstand)
- `devices/esp_net_sen_env_bme280_veml.yaml`: BME280 + VEML7700 (Temperatur, Feuchte, Luftdruck, Helligkeit)
- `devices/esp_net_zrl_cover_example.yaml`: Cover mit Kalibrierung und Positionssteuerung (zeitbasiert)

### Devices - Battery (batteriebetrieben)
- `devices/bat_sen_window_contact_example.yaml`: Fensterkontakt, GPIO-Wake bei Kontaktwechsel, Deep Sleep
- `devices/bat_sen_rain_example.yaml`: Regensensor, periodischer Wake (900s), ADC-basierte Nasserkennung

## Abgrenzung

Diese Zweitlinie ist absichtlich getrennt gehalten:
- kein Eingriff in `firmware/`
- kein Eingriff in den bestehenden Masterpfad
- kein Eingriff in ESP-NOW
- kein Eingriff in die Serverarchitektur
- **Kommunikation ausschliesslich ueber MQTT**
- **Erfordert `master_compat` MQTT-Bridge fuer die Integration mit dem bestehenden System**

Wer diesen Pfad produktiv nutzen will, muss die jeweiligen Pins, Sensoren, Fahrzeiten, Endlagen und Fehlersituationen am Zielgeraet sauber validieren.

## Status

| Geraet | Vertrag | Hardware-Logik | Kalibrierung | Validierung |
|--------|---------|----------------|--------------|-------------|
| net_erl_light | ✅ | ✅ | N/A | ❌ |
| net_sen_env (DHT22) | ✅ | ✅ (nur Temp/Hum) | N/A | ❌ |
| net_sen_env (BME280) | ✅ | ✅ (Temp/Hum/Druck) | N/A | ❌ |
| net_sen_env (BME680) | ✅ | ✅ (Temp/Hum/Druck/Gas) | N/A | ❌ |
| net_sen_env (BME280+VEML) | ✅ | ✅ (Temp/Hum/Druck/Lux) | N/A | ❌ |
| net_zrl_cover | ✅ | ✅ (zeitbasiert) | ✅ (manuell) | ❌ |
| bat_sen_window_contact | ✅ | ✅ (GPIO-Wake) | N/A | ❌ |
| bat_sen_rain | ✅ | ✅ (ADC, periodisch) | N/A | ❌ |
