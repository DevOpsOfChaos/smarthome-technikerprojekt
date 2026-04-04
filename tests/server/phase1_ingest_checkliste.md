# Phase 1 Ingest Checkliste

## Ziel

Diese Checkliste prueft nur den Phase-1-Kern:

- MQTT-Ingest
- gemeinsames Geraeteobjekt
- separater Masterpfad
- zentrale SQLite-Writes
- partielle Updates ohne Feldzerstoerung
- eine einzige fachliche Wahrheitsbasis in `server/nodered/lib/`

## Vorbereitung

1. `cd server`
2. `docker compose up -d`
3. Node-RED unter `http://localhost:1880` oeffnen
4. Debug-Ansicht beobachten
5. warten, bis der SQLite-Node ohne Install-/Build-Fehler gestartet ist

## Entdopplung vorab pruefen

Vor den MQTT-Tests kurz kontrollieren:

```bash
rg -n "global.get\\(\"topicRouter\"\\)|global.get\\(\"topicHandlers\"\\)|global.get\\(\"deviceStore\"\\)|global.get\\(\"sqliteWrites\"\\)" server/nodered/flows/active
```

Erwartung:

- `10_mqtt_ingest.json` nutzt `topicRouter`
- `00_boot.json` nutzt `deviceStore`
- `20_device_store.json` und `90_master_diag.json` nutzen `topicHandlers`
- `30_sqlite_persist.json` enthaelt nur den SQLite-Ausfuehrungspfad

Zusatzcheck:

```bash
rg -n "aliasMap|stateFields|configFields|coerceBoolean\\(|createDevice\\(|INSERT INTO devices|INSERT INTO device_state_latest" server/nodered/flows/active
```

Erwartung:

- keine Treffer in den aktiven Flow-Dateien
- Fachlogik liegt nur noch unter `server/nodered/lib/`

## SQLite-Datei pruefen

Nach dem Start muss die Datenbankdatei vorhanden sein:

```bash
test -f server/sqlite/smarthome_phase1.db && echo ok
```

## Testgerät anlegen

### Meta senden

```bash
mosquitto_pub -h localhost -t smarthome/device/net_erl_hall_light/meta -r -m "{\"device_id\":\"net_erl_hall_light\",\"device_name\":\"Flurlicht\",\"device_class\":\"NET-ERL\",\"power_type\":\"mains\",\"caps\":[\"switchable\",\"motion\",\"lux\"],\"fw_version\":\"0.1.0\"}"
```

Erwartung:

- Geraet wird auto-angelegt
- `identity.device_id` und `identity.device_name` sind gesetzt
- `meta.caps` enthaelt mindestens `switchable`, `motion`, `lux`, `online_state`, `fault_state`, `ack_tracking`
- `devices` enthaelt oder aktualisiert eine Zeile fuer `net_erl_hall_light`

## Availability prüfen

```bash
mosquitto_pub -h localhost -t smarthome/device/net_erl_hall_light/availability -r -m "{\"device_id\":\"net_erl_hall_light\",\"availability\":\"online\",\"online\":true}"
```

Erwartung:

- `availability.online = true`
- `availability.availability = online`
- `availability.last_seen_at` wird aktualisiert
- `device_state_latest.online = 1`

## State prüfen

### Erster State

```bash
mosquitto_pub -h localhost -t smarthome/device/net_erl_hall_light/state -r -m "{\"device_id\":\"net_erl_hall_light\",\"relay_1\":true,\"motion\":true,\"lux\":120,\"fault\":false,\"report_interval_s\":60}"
```

Erwartung:

- `state.relay_1 = true`
- `state.motion = true`
- `state.lux = 120`
- `config.report_interval_s = 60`
- `availability.online` bleibt oder wird `true`
- `device_state_latest` wird fuer dasselbe Geraet aktualisiert

### Partieller State

```bash
mosquitto_pub -h localhost -t smarthome/device/net_erl_hall_light/state -r -m "{\"device_id\":\"net_erl_hall_light\",\"motion\":false}"
```

Erwartung:

- `state.motion` wird auf `false` gesetzt
- `state.relay_1` bleibt `true`
- `state.lux` bleibt `120`
- fehlende Felder loeschen nichts
- SQLite loescht die vorherigen `device_state_latest`-Werte ebenfalls nicht

## Event prüfen

```bash
mosquitto_pub -h localhost -t smarthome/device/net_erl_hall_light/event -m "{\"device_id\":\"net_erl_hall_light\",\"event\":\"motion_clear\",\"event_type\":\"motion\",\"trigger\":\"pir\",\"param1\":\"0\",\"param2\":\"0\"}"
```

Erwartung:

- `last_event.event_type = motion`
- `last_event.event_label = motion_clear`
- normaler `state` bleibt unveraendert
- `device_event_log` bekommt einen neuen Eintrag
- `device_state_latest.last_event_*` wird mitgezogen

## ACK prüfen

```bash
mosquitto_pub -h localhost -t smarthome/device/net_erl_hall_light/ack -m "{\"device_id\":\"net_erl_hall_light\",\"request_id\":\"req-1\",\"channel\":\"relay_1\",\"status\":\"ok\",\"status_code\":\"200\",\"ack_msg_type\":\"set_relay\",\"ack_seq\":\"17\"}"
```

Erwartung:

- `last_ack.request_id = req-1`
- `last_ack.channel = relay_1`
- `last_ack.status = ok`
- normaler `state` bleibt unveraendert
- `device_ack_log` bekommt einen neuen Eintrag
- `device_state_latest.last_ack_*` wird mitgezogen

## Minimalen Command-/ACK-Pfad pruefen

In einem zweiten Terminal vor dem HTTP-Aufruf den offiziellen Command-Capture starten:

```bash
mosquitto_sub -h localhost -t smarthome/device/net_erl_hall_light/command -C 1 -v
```

Dann den engen Server-Einstieg aufrufen:

```bash
curl -s -X POST http://localhost:1880/api/phase1/net-erl/relay-1 -H "Content-Type: application/json" -d "{\"device_id\":\"net_erl_hall_light\",\"relay_1\":true}"
```

Erwartung:

- HTTP antwortet mit `202`
- die Antwort enthaelt `device_id`, `request_id`, `command = set_relay`, `relay_1 = true`
- MQTT zeigt genau einen Publish auf `smarthome/device/net_erl_hall_light/command`
- der publizierte Payload enthaelt dieselbe `request_id`

Mit derselben `request_id` den ACK-Rueckweg pruefen:

```bash
mosquitto_pub -h localhost -t smarthome/device/net_erl_hall_light/ack -m "{\"device_id\":\"net_erl_hall_light\",\"request_id\":\"<request_id_aus_http>\",\"channel\":\"command\",\"status\":\"ok\",\"status_code\":\"0\",\"ack_msg_type\":\"5\",\"ack_seq\":\"1\"}"
```

Optional den sichtbaren Zielzustand nachziehen:

```bash
mosquitto_pub -h localhost -t smarthome/device/net_erl_hall_light/state -r -m "{\"device_id\":\"net_erl_hall_light\",\"relay_1\":true}"
```

Erwartung:

- `device_ack_log` bekommt einen neuen Eintrag mit derselben `request_id`
- `device_state_latest.last_ack_request_id` entspricht der HTTP-Antwort
- `device_state_latest.last_ack_channel = command`
- `device_state_latest.relay_1 = true`, wenn der optionale State-Nachtrag gefahren wurde

## Master separat prüfen

### Masterstatus

```bash
mosquitto_pub -h localhost -t smarthome/master/master_1/status -r -m "{\"master_id\":\"master_1\",\"online\":true,\"wifi\":true,\"mqtt\":true,\"espnow\":true,\"fw\":\"0.1.0\"}"
```

### Masterevent

```bash
mosquitto_pub -h localhost -t smarthome/master/master_1/event -m "{\"master_id\":\"master_1\",\"event\":\"mqtt_connected\",\"message\":\"broker online\",\"fw\":\"0.1.0\"}"
```

Erwartung:

- Masterdaten landen nicht im normalen `devices`-Objekt
- `master_1` fuehrt einen separaten Statusblock
- `master_1` fuehrt einen separaten letzten Eventblock
- `master_status` enthaelt den letzten Status
- `master_event_log` bekommt einen neuen Eintrag

## SQLite-Inhalt stichprobenartig pruefen

Ein kurzer Spot-Check gegen die lokale DB-Datei reicht:

```bash
python - <<'PY'
import sqlite3
db = sqlite3.connect("server/sqlite/smarthome_phase1.db")
for sql in [
    "select device_id, device_name from devices order by device_id",
    "select device_id, online, relay_1, motion, lux from device_state_latest order by device_id",
    "select device_id, event_type, event_label from device_event_log order by id desc limit 3",
    "select device_id, request_id, status from device_ack_log order by id desc limit 3",
    "select master_id, online, wifi, mqtt, espnow from master_status order by master_id",
    "select master_id, event, message from master_event_log order by id desc limit 3",
]:
    print("\\nSQL>", sql)
    for row in db.execute(sql):
        print(row)
db.close()
PY
```

## Abschluss

Erfolgreich ist Phase 1 nur dann, wenn alle Punkte gleichzeitig stimmen:

- Geraete werden robust auto-angelegt
- `meta`, `availability`, `state`, `event` und `ack` landen in den richtigen Bloecken
- partielle State-Updates zerstoeren keine vorhandenen Werte
- der Master bleibt komplett getrennt
- die SQLite-Writes kommen aus derselben zentralen Handlerkette
- die aktiven Flows enthalten keine zweite fachliche Logikbasis mehr
