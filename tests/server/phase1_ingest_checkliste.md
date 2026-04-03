# Phase 1 Ingest Checkliste

## Ziel

Diese Checkliste prueft nur den Phase-1-Kern:

- MQTT-Ingest
- gemeinsames Geraeteobjekt
- separater Masterpfad
- partielle Updates ohne Feldzerstoerung
- eine einzige fachliche Wahrheitsbasis in `server/nodered/lib/`

## Vorbereitung

1. `cd server`
2. `docker compose up -d`
3. Node-RED unter `http://localhost:1880` oeffnen
4. Debug-Ansicht beobachten

## Entdopplung vorab pruefen

Vor den MQTT-Tests kurz kontrollieren:

```bash
rg -n "global.get\\(\"topicRouter\"\\)|global.get\\(\"topicHandlers\"\\)|global.get\\(\"deviceStore\"\\)" server/nodered/flows/active
```

Erwartung:

- `10_mqtt_ingest.json` nutzt `topicRouter`
- `20_device_store.json` und `90_master_diag.json` nutzen `topicHandlers`
- `00_boot.json` nutzt `deviceStore`

Zusatzcheck:

```bash
rg -n "aliasMap|stateFields|configFields|coerceBoolean\\(|createDevice\\(" server/nodered/flows/active
```

Erwartung:

- keine Treffer in den aktiven Flow-Dateien
- Fachlogik liegt nur noch unter `server/nodered/lib/`

## Testgerät anlegen

### Meta senden

```bash
mosquitto_pub -h localhost -t smarthome/device/net_erl_hall_light/meta -r -m "{\"device_id\":\"net_erl_hall_light\",\"device_name\":\"Flurlicht\",\"device_class\":\"NET-ERL\",\"power_type\":\"mains\",\"caps\":[\"switchable\",\"motion\",\"lux\"],\"fw_version\":\"0.1.0\"}"
```

Erwartung:

- Geraet wird auto-angelegt
- `identity.device_id` und `identity.device_name` sind gesetzt
- `meta.caps` enthaelt mindestens `switchable`, `motion`, `lux`, `online_state`, `fault_state`, `ack_tracking`

## Availability prüfen

```bash
mosquitto_pub -h localhost -t smarthome/device/net_erl_hall_light/availability -r -m "{\"device_id\":\"net_erl_hall_light\",\"availability\":\"online\",\"online\":true}"
```

Erwartung:

- `availability.online = true`
- `availability.availability = online`
- `availability.last_seen_at` wird aktualisiert

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

### Partieller State

```bash
mosquitto_pub -h localhost -t smarthome/device/net_erl_hall_light/state -r -m "{\"device_id\":\"net_erl_hall_light\",\"motion\":false}"
```

Erwartung:

- `state.motion` wird auf `false` gesetzt
- `state.relay_1` bleibt `true`
- `state.lux` bleibt `120`
- fehlende Felder loeschen nichts

## Event prüfen

```bash
mosquitto_pub -h localhost -t smarthome/device/net_erl_hall_light/event -m "{\"device_id\":\"net_erl_hall_light\",\"event\":\"motion_clear\",\"event_type\":\"motion\",\"trigger\":\"pir\",\"param1\":\"0\",\"param2\":\"0\"}"
```

Erwartung:

- `last_event.event_type = motion`
- `last_event.event_label = motion_clear`
- normaler `state` bleibt unveraendert

## ACK prüfen

```bash
mosquitto_pub -h localhost -t smarthome/device/net_erl_hall_light/ack -m "{\"device_id\":\"net_erl_hall_light\",\"request_id\":\"req-1\",\"channel\":\"relay_1\",\"status\":\"ok\",\"status_code\":\"200\",\"ack_msg_type\":\"set_relay\",\"ack_seq\":\"17\"}"
```

Erwartung:

- `last_ack.request_id = req-1`
- `last_ack.channel = relay_1`
- `last_ack.status = ok`
- normaler `state` bleibt unveraendert

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

## Abschluss

Erfolgreich ist Phase 1 nur dann, wenn alle Punkte gleichzeitig stimmen:

- Geraete werden robust auto-angelegt
- `meta`, `availability`, `state`, `event` und `ack` landen in den richtigen Bloecken
- partielle State-Updates zerstoeren keine vorhandenen Werte
- der Master bleibt komplett getrennt
- die aktiven Flows enthalten keine zweite fachliche Logikbasis mehr
