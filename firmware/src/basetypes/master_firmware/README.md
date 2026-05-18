# Master Firmware

Basistyp fuer die zentrale ESP-NOW-zu-MQTT-Bruecke.

Basis:
- Dynamische Node-Registry fuer Netz- und Batterie-Nodes
- ESP-NOW-Empfang fuer HELLO, HEARTBEAT, STATE, EVENT und ACK
- ESP-NOW-Senden fuer HELLO_ACK, STATE_REQUEST, CMD und CFG
- MQTT-Ausgabe fuer Master-Status, Node-Meta, Availability, State, Event und ACK
- MQTT-Kommandos fuer `get_state`, `set_relay`, `set_config`, `open`, `close`, `stop` und `set_position`
- Pending-/Retry-Logik fuer bestaetigungspflichtige CMD- und CFG-Nachrichten

Sichtbare MQTT-Linie:
- `smarthome/master/{DEVICE_ID}/status`
- `smarthome/master/{DEVICE_ID}/event`
- `smarthome/device/{node_id}/meta`
- `smarthome/device/{node_id}/availability`
- `smarthome/device/{node_id}/state`
- `smarthome/device/{node_id}/ack`
- `smarthome/device/{node_id}/event`
- `smarthome/device/{node_id}/command`

Hinweis:
- Zugangsdaten gehoeren nicht in diese Firmware-Dateien, sondern in die lokale `Secrets.h`.
- Der Master speichert keine feste Node-Liste; Nodes melden sich dynamisch per HELLO.
