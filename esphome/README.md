# ESPHome Zweitlinie

Dieser Pfad bereitet einen separaten, additiven ESPHome-Geraetepfad vor, der denselben MQTT-/JSON-Vertrag wie der bestehende kompatible Firmwarepfad spricht.

Wichtig:
- Der Kern des Technikerprojekts bleibt unveraendert.
- Der bestehende Masterpfad bleibt die Hauptlinie fuer die eigene Firmware.
- Das bestehende ESP-NOW-Protokoll bleibt unberuehrt.
- Der Serververtrag bleibt unveraendert.
- Dieser ESPHome-Pfad ist vorbereitet, aber nicht hardwarevalidiert.

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

## Dateien

- `packages/smarthome_contract_base.yaml`: gemeinsame Vertragsbasis fuer Meta, Availability und Statusfehler
- `packages/smarthome_command_ack.yaml`: ACK-Helfer fuer den Command-Pfad
- `packages/smarthome_relay_contract.yaml`: Relais-Beispielvertrag mit `set_relay` und `get_state`
- `packages/smarthome_cover_contract.yaml`: Cover-Beispielvertrag mit `open`, `close`, `stop`, `set_position` und Kalibrierungsregel
- `devices/esp_net_erl_light_example.yaml`: einfacher Relais-Beispielpfad
- `devices/esp_net_sen_env_example.yaml`: einfacher Temperatur-/Feuchte-Beispielpfad
- `devices/esp_net_zrl_cover_example.yaml`: vorbereiteter Cover-Beispielpfad

## Abgrenzung

Diese Zweitlinie ist absichtlich getrennt gehalten:
- kein Eingriff in `firmware/`
- kein Eingriff in den bestehenden Masterpfad
- kein Eingriff in ESP-NOW
- kein Eingriff in die Serverarchitektur

Wer diesen Pfad produktiv nutzen will, muss die jeweiligen Pins, Sensoren, Fahrzeiten, Endlagen und Fehlersituationen am Zielgeraet sauber validieren.
