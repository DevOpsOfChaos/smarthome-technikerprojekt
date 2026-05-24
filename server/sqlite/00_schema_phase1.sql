-- =============================================================================
-- SmartHome Phase 1 – Datenbankschema (SQLite)
-- =============================================================================
-- Verwaltet Geräte-Stammdaten, aktuellen Gerätezustand und Master-Status.
-- Alle Tabellen nutzen IF NOT EXISTS – das Schema wird bei jedem Neustart
-- idempotent angelegt, Migrationen folgen im Docker-Entrypoint.
-- =============================================================================

PRAGMA foreign_keys = ON;

-- ---------------------------------------------------------------------------
-- Tabelle: devices – Geräte-Stammdaten
-- ---------------------------------------------------------------------------
-- Enthält die Metadaten jedes Geräts (Klasse, Firmware, Fähigkeiten).
-- device_id ist der Primärschlüssel – jede Device-ID existiert genau einmal.
-- dashboard_display_name wird über /api/phase1/dashboard/display-name gesetzt.
CREATE TABLE IF NOT EXISTS devices (
    device_id              TEXT PRIMARY KEY,
    device_name            TEXT,
    dashboard_display_name TEXT,
    device_class           TEXT,
    power_type             TEXT,
    fw_version             TEXT,
    caps                   TEXT,   -- JSON-Array der Fähigkeiten
    control_mode           TEXT,
    config_profile         TEXT,
    reporting_mode         TEXT,
    sensor_mask            TEXT,
    input_mask             TEXT,
    mac_address            TEXT,
    meta_schema_version    TEXT,
    created_at             TEXT NOT NULL,
    updated_at             TEXT NOT NULL
);

-- ---------------------------------------------------------------------------
-- Tabelle: device_state_latest – Aktueller Gerätezustand (Snapshot)
-- ---------------------------------------------------------------------------
-- Enthält den jeweils letzten bekannten Zustand pro Gerät (Sensordaten,
-- Aktorstatus, letztes Event, letztes Ack). Kein Historienverlauf.
-- FOREIGN KEY auf devices sorgt für Kaskaden-Löschung.
CREATE TABLE IF NOT EXISTS device_state_latest (
    device_id             TEXT PRIMARY KEY,
    availability          TEXT NOT NULL DEFAULT 'unknown',
    online                INTEGER NOT NULL DEFAULT 0,
    last_seen_at          TEXT,
    fault                 INTEGER NOT NULL DEFAULT 0,
    relay_1               INTEGER,
    relay_2               INTEGER,
    cover_mode            TEXT,
    cover_state           TEXT,
    cover_direction       TEXT,
    cover_position        REAL,
    cover_calibrated      INTEGER,
    is_calibrated         INTEGER,
    travel_time_ms        INTEGER,
    temp_01c              REAL,
    hum_01pct             REAL,
    lux                   REAL,
    pressure_pa           REAL,
    gas_ohm               REAL,
    aqi                   REAL,
    tvoc_ppb              REAL,
    eco2_ppm              REAL,
    motion                INTEGER,
    rain                  INTEGER,
    rain_raw              REAL,
    window_open           INTEGER,
    battery_pct           REAL,
    battery_mv            REAL,
    button_flags          INTEGER,
    button_last_action    TEXT,
    button_last_action_at TEXT,
    report_interval_s     INTEGER,
    lux_threshold_on      REAL,
    auto_off_delay_s      INTEGER,
    rain_threshold        REAL,
    auto_up_time          TEXT,
    auto_down_time        TEXT,
    auto_schedule_enabled INTEGER,
    last_event_type       TEXT,
    last_event_label      TEXT,
    last_event_trigger    TEXT,
    last_event_param1     TEXT,
    last_event_param2     TEXT,
    last_event_at         TEXT,
    last_ack_request_id   TEXT,
    last_ack_channel      TEXT,
    last_ack_status       TEXT,
    last_ack_status_code  TEXT,
    last_ack_msg_type     TEXT,
    last_ack_seq          TEXT,
    last_ack_source       TEXT,
    last_ack_at           TEXT,
    updated_at            TEXT NOT NULL,
    FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE
);

-- ---------------------------------------------------------------------------
-- Tabelle: master_status – Aktueller Gateway-Status (Snapshot)
-- ---------------------------------------------------------------------------
-- Enthält den Verbindungsstatus jedes Master-Geräts (online, WiFi, MQTT,
-- ESP-NOW). Kein Verlauf – nur der letzte bekannte Snapshot.
CREATE TABLE IF NOT EXISTS master_status (
    master_id    TEXT PRIMARY KEY,
    online       INTEGER NOT NULL DEFAULT 0,
    wifi         INTEGER NOT NULL DEFAULT 0,
    mqtt         INTEGER NOT NULL DEFAULT 0,
    espnow       INTEGER NOT NULL DEFAULT 0,
    fw           TEXT,
    last_seen_at TEXT,
    updated_at   TEXT NOT NULL
);

-- ---------------------------------------------------------------------------
-- Tabelle: automations – Automatisierungsregeln
-- ---------------------------------------------------------------------------
-- Enthält je eine Automatisierung mit Zielgerät, Aktion und globalem Status.
-- action_payload ist JSON – erzeugt ausschliesslich über commandMinimal.
-- last_result speichert das Ergebnis (ok/error) des letzten Ausführungsversuchs.
CREATE TABLE IF NOT EXISTS automations (
    automation_id  TEXT PRIMARY KEY,
    name           TEXT NOT NULL,
    enabled        INTEGER NOT NULL DEFAULT 1,
    target_device_id TEXT NOT NULL,
    action_kind    TEXT NOT NULL,
    action_payload TEXT NOT NULL,
    created_at     TEXT NOT NULL,
    updated_at     TEXT NOT NULL,
    last_run_at    TEXT,
    last_result    TEXT
);

-- ---------------------------------------------------------------------------
-- Tabelle: automation_conditions – Bedingungen pro Automatisierung
-- ---------------------------------------------------------------------------
-- Eine Automatisierung kann mehrere Bedingungen haben (AND-Verknüpfung).
-- condition_scope: 'global' (Zeit/Wochentag) oder 'local' (Gerätezustand).
-- condition_kind:  'weekdays', 'time_exact', 'time_window', 'device_state'.
-- Operatoren:      eq, neq, gt, gte, lt, lte (nur diese sechs erlaubt).
-- ON DELETE CASCADE löscht Bedingungen automatisch bei Löschung der Automatisierung.
CREATE TABLE IF NOT EXISTS automation_conditions (
    condition_id      TEXT PRIMARY KEY,
    automation_id     TEXT NOT NULL,
    condition_scope   TEXT NOT NULL,
    condition_kind    TEXT NOT NULL,
    source_device_id  TEXT,
    field_name        TEXT,
    operator          TEXT,
    expected_value    TEXT,
    weekdays          TEXT,
    time_start        TEXT,
    time_end          TEXT,
    created_at        TEXT NOT NULL,
    updated_at        TEXT NOT NULL,
    FOREIGN KEY (automation_id) REFERENCES automations(automation_id) ON DELETE CASCADE
);
