PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS devices (
    device_id TEXT PRIMARY KEY,
    device_name TEXT,
    dashboard_display_name TEXT,
    device_class TEXT,
    power_type TEXT,
    fw_version TEXT,
    caps TEXT,
    control_mode TEXT,
    config_profile TEXT,
    reporting_mode TEXT,
    sensor_mask TEXT,
    input_mask TEXT,
    mac_address TEXT,
    meta_schema_version TEXT,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS device_state_latest (
    device_id TEXT PRIMARY KEY,
    availability TEXT NOT NULL DEFAULT 'unknown',
    online INTEGER NOT NULL DEFAULT 0,
    last_seen_at TEXT,
    fault INTEGER NOT NULL DEFAULT 0,
    relay_1 INTEGER,
    relay_2 INTEGER,
    cover_mode TEXT,
    cover_state TEXT,
    cover_direction TEXT,
    cover_position REAL,
    cover_calibrated INTEGER,
    is_calibrated INTEGER,
    travel_time_ms INTEGER,
    temp_01c REAL,
    hum_01pct REAL,
    lux REAL,
    pressure_pa REAL,
    gas_ohm REAL,
    aqi REAL,
    tvoc_ppb REAL,
    eco2_ppm REAL,
    motion INTEGER,
    rain INTEGER,
    rain_raw REAL,
    window_open INTEGER,
    battery_pct REAL,
    battery_mv REAL,
    button_flags INTEGER,
    button_last_action TEXT,
    button_last_action_at TEXT,
    report_interval_s INTEGER,
    lux_threshold_on REAL,
    auto_off_delay_s INTEGER,
    rain_threshold REAL,
    auto_up_time TEXT,
    auto_down_time TEXT,
    auto_schedule_enabled INTEGER,
    last_event_type TEXT,
    last_event_label TEXT,
    last_event_trigger TEXT,
    last_event_param1 TEXT,
    last_event_param2 TEXT,
    last_event_at TEXT,
    last_ack_request_id TEXT,
    last_ack_channel TEXT,
    last_ack_status TEXT,
    last_ack_status_code TEXT,
    last_ack_msg_type TEXT,
    last_ack_seq TEXT,
    last_ack_source TEXT,
    last_ack_at TEXT,
    updated_at TEXT NOT NULL,
    FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS device_event_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL,
    event_type TEXT,
    event_label TEXT,
    event_trigger TEXT,
    param1 TEXT,
    param2 TEXT,
    occurred_at TEXT NOT NULL,
    logged_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS device_ack_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL,
    request_id TEXT,
    channel TEXT,
    status TEXT,
    status_code TEXT,
    ack_msg_type TEXT,
    ack_seq TEXT,
    source TEXT,
    occurred_at TEXT NOT NULL,
    FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS master_status (
    master_id TEXT PRIMARY KEY,
    online INTEGER NOT NULL DEFAULT 0,
    wifi INTEGER NOT NULL DEFAULT 0,
    mqtt INTEGER NOT NULL DEFAULT 0,
    espnow INTEGER NOT NULL DEFAULT 0,
    fw TEXT,
    last_seen_at TEXT,
    updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS master_event_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    master_id TEXT NOT NULL,
    event TEXT,
    message TEXT,
    fw TEXT,
    occurred_at TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_device_event_log_device_time
    ON device_event_log (device_id, occurred_at DESC);

CREATE INDEX IF NOT EXISTS idx_device_ack_log_device_time
    ON device_ack_log (device_id, occurred_at DESC);

CREATE INDEX IF NOT EXISTS idx_master_event_log_master_time
    ON master_event_log (master_id, occurred_at DESC);
