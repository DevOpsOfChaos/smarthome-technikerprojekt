# Aktive V1-Flows

- `00_boot.json`: gemeinsame Konfiguration und Initialisierung des Laufzeitzustands
- `05_dashboard_runtime.json`: FlowFuse-Dashboard-Basis mit Uebersichts- und Detailseite
- `10_mqtt_ingest.json`: MQTT-Abos, Topic-Erkennung, Routing auf Device- oder Masterpfad
- `20_device_store.json`: gemeinsames Geraeteobjekt und SQLite-Upserts fuer `devices` und `device_state_latest`
- `60_dashboard_overview.json`: zentrale Geraeteuebersicht
- `63_dashboard_device_detail.json`: Detailseite pro Geraet
- `90_master_diag.json`: separater Masterpfad fuer `status` und `event`

Die Dateien bleiben bewusst klein und rollengetrennt.
