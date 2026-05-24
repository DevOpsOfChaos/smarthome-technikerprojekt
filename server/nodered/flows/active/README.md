# Aktive Server-Flows

- `00_boot.json`: gemeinsame Konfiguration und Initialisierung des Laufzeitzustands
- `05_dashboard_runtime.json`: FlowFuse-Dashboard-Basis mit Übersichts-, Detail-, Master- und Automatisierungsseite
- `10_mqtt_ingest.json`: MQTT-Abos, Topic-Erkennung, Routing auf Device- oder Masterpfad
- `20_device_store.json`: gemeinsames Geräteobjekt und SQLite-Upserts für `devices` und `device_state_latest`
- `30_sqlite_persist.json`: SQLite-Schreibknoten, nimmt `msg.sqlite_batch` entgegen
- `40_command_minimal.json`: Aktorsteuerung (Relais, Rolladen)
- `41_cover_automation_detail.json`: Rolladen-Detailsteuerung abhängig vom Kalibrierstatus
- `42_dashboard_device_name.json`: Anzeigenamen persistent setzen und lesen
- `60_dashboard_overview.json`: Geräteübersicht
- `63_dashboard_device_detail.json`: Detailseite pro Gerät
- `64_dashboard_device_delete.json`: Gerät aus Übersicht und SQLite entfernen
- `65_dashboard_master.json`: Master-Status-Seite (liest aus `master_status`, keine Steuerkommandos)
- `70_dashboard_automations.json`: Automatisierungen-CRUD-Seite (erstellen, anzeigen, bearbeiten, löschen, aktivieren/deaktivieren)
- `71_automation_runner.json`: Automation-Runner (minütlicher Tick, Bedingungsprüfung, MQTT-Befehl via commandMinimal)
- `90_master_diag.json`: Master-Gateway-Status (Snapshot, kein Verlauf)

Die Dateien bleiben bewusst klein und rollengetrennt.
