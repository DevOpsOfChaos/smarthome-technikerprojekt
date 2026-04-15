"use strict";

function parseJson(value) {
    if (value === null || value === undefined || value === "") {
        return null;
    }
    if (typeof value === "object") {
        return value;
    }
    try {
        return JSON.parse(value);
    } catch (error) {
        return null;
    }
}

function normalizeString(value) {
    return typeof value === "string" ? value.trim() : "";
}

function sqlStringLiteral(value) {
    return "'" + String(value).replace(/'/g, "''") + "'";
}

function buildDeviceDeleteQuery(deviceId) {
    const normalizedId = normalizeString(deviceId);
    if (!normalizedId) {
        return null;
    }
    const idSql = sqlStringLiteral(normalizedId);
    return [
        "BEGIN IMMEDIATE;",
        "DELETE FROM device_state_latest WHERE device_id = " + idSql + ";",
        "DELETE FROM device_event_log WHERE device_id = " + idSql + ";",
        "DELETE FROM device_ack_log WHERE device_id = " + idSql + ";",
        "DELETE FROM devices WHERE device_id = " + idSql + ";",
        "COMMIT;"
    ].join(" ");
}

function normalizeBaseType(value) {
    const text = normalizeString(value).toLowerCase().replace(/-/g, "_");
    if (!text) {
        return "";
    }
    if (text.startsWith("master")) return "master";
    if (text.startsWith("net_erl")) return "net_erl";
    if (text.startsWith("net_zrl")) return "net_zrl";
    if (text.startsWith("net_sen")) return "net_sen";
    if (text.startsWith("bat_sen")) return "bat_sen";
    return text;
}

function isBatteryDevice(device) {
    return normalizeBaseType(device.base_type || device.device_class || device.device_id) === "bat_sen";
}

function isCoverDevice(device, state, meta) {
    const baseType = normalizeBaseType(device.base_type || device.device_class || device.device_id);
    const controlMode = normalizeString(meta && meta.control_mode).toLowerCase();
    if (baseType !== "net_zrl") {
        return false;
    }
    return controlMode.includes("cover")
        || controlMode.includes("shutter")
        || controlMode.includes("blind")
        || Object.prototype.hasOwnProperty.call(state, "cover_position")
        || Object.prototype.hasOwnProperty.call(state, "cover_direction")
        || Object.prototype.hasOwnProperty.call(state, "cover_moving");
}

function isRelayDevice(device, state, meta) {
    if (isCoverDevice(device, state, meta)) {
        return false;
    }
    return Object.prototype.hasOwnProperty.call(state, "relay_1")
        || Object.prototype.hasOwnProperty.call(state, "relay_2")
        || ["net_erl", "net_zrl"].includes(normalizeBaseType(device.base_type || device.device_class || device.device_id));
}

function isNetErlDevice(device) {
    return normalizeBaseType(device.base_type || device.device_class || device.device_id) === "net_erl";
}

function hasCapability(meta, capability) {
    const caps = meta && Array.isArray(meta.caps) ? meta.caps : [];
    return caps.includes(capability);
}

function availabilityInfo(availability) {
    const state = normalizeString(availability && availability.availability).toLowerCase();
    if (!state) {
        return { label: "unbekannt", className: "sh-chip-unknown" };
    }
    if (state === "online") {
        return { label: "online", className: "sh-chip-online" };
    }
    if (["late", "asleep", "sleeping_expected"].includes(state)) {
        return { label: state, className: "sh-chip-late" };
    }
    if (state === "offline") {
        return { label: "offline", className: "sh-chip-offline" };
    }
    return { label: state, className: "sh-chip-unknown" };
}

function formatNumber(rawValue, divisor, suffix) {
    if (typeof rawValue !== "number" || !Number.isFinite(rawValue)) {
        return "-";
    }
    const scaled = divisor ? rawValue / divisor : rawValue;
    const text = Number.isInteger(scaled) ? String(scaled) : scaled.toFixed(1).replace(/\.0$/, "");
    return suffix ? text + " " + suffix : text;
}

function normalizeCoverPosition(value, isCalibrated) {
    if (typeof value !== "number" || !Number.isFinite(value) || value < 0 || value > 100) {
        return null;
    }
    if (!isCalibrated && value !== 0 && value !== 100) {
        return null;
    }
    return value;
}

function coverCalibrationHint(positionCalibrated, positionValue) {
    if (!positionCalibrated) {
        return "Nicht kalibriert";
    }
    if (positionValue === null) {
        return "Kalibriert, Position aktuell unbekannt";
    }
    return "";
}

function formatStateValue(key, value) {
    if (value === null || value === undefined) return "-";
    if (["relay_1", "relay_2"].includes(key)) return value ? "An" : "Aus";
    if (["motion", "presence"].includes(key)) return value ? "Erkannt" : "Nein";
    if (key === "rain") return value ? "Nass" : "Trocken";
    if (key === "contact_open") return value ? "Offen" : "Geschlossen";
    if (key === "window_open") return value ? "Offen" : "Geschlossen";
    if (key === "cover_moving") return value ? "Ja" : "Nein";
    if (["cover_position", "cover_target", "battery_pct"].includes(key)) return formatNumber(value, 0, "%");
    if (key === "battery_mv") return formatNumber(value, 0, "mV");
    if (key === "button_flags") return formatNumber(value, 0, "");
    if (key === "temp_01c") return formatNumber(value, 10, "°C");
    if (key === "hum_01pct") return formatNumber(value, 10, "%");
    if (key === "pressure_hpa") return formatNumber(value, 0, "hPa");
    if (key === "pressure_pa") return formatNumber(value, 100, "hPa");
    if (key === "lux") return formatNumber(value, 0, "lx");
    if (key === "lux_01lx") return formatNumber(value, 10, "lx");
    if (key === "gas_ohm") return formatNumber(value, 0, "Ohm");
    if (typeof value === "number" && Number.isFinite(value)) return Number.isInteger(value) ? String(value) : value.toFixed(1).replace(/\.0$/, "");
    if (typeof value === "boolean") return value ? "Ja" : "Nein";
    if (typeof value === "string") return value;
    return JSON.stringify(value);
}

function labelForStateKey(key) {
    const labels = {
        battery_mv: "Batterie",
        battery_pct: "Batteriestand",
        button_flags: "Tasterflags",
        contact_open: "Kontakt",
        cover_direction: "Richtung",
        cover_moving: "Bewegung",
        cover_position: "Position",
        cover_target: "Ziel",
        gas_ohm: "Gas",
        hum_01pct: "Luftfeuchte",
        lux: "Helligkeit",
        lux_01lx: "Helligkeit",
        motion: "Bewegung",
        presence: "Präsenz",
        pressure_pa: "Druck",
        pressure_hpa: "Druck",
        relay_1: "Relais 1",
        relay_2: "Relais 2",
        rain: "Regen",
        rain_raw: "Regen-Rohwert",
        temp_01c: "Temperatur",
        window_open: "Kontakt"
    };
    return labels[key] || key.replace(/_/g, " ");
}

function toBooleanOrNull(value) {
    if (value === null || value === undefined) {
        return null;
    }
    return Boolean(value);
}

function assignIfPresent(target, key, value) {
    if (value !== null && value !== undefined && value !== "") {
        target[key] = value;
    }
}

function buildMetaFromRow(row) {
    const meta = {};
    assignIfPresent(meta, "device_name", row.device_name);
    assignIfPresent(meta, "device_class", row.device_class);
    assignIfPresent(meta, "power_type", row.power_type);
    assignIfPresent(meta, "fw_version", row.fw_version);
    assignIfPresent(meta, "control_mode", row.control_mode);
    assignIfPresent(meta, "config_profile", row.config_profile);
    assignIfPresent(meta, "reporting_mode", row.reporting_mode);
    assignIfPresent(meta, "sensor_mask", row.sensor_mask);
    assignIfPresent(meta, "input_mask", row.input_mask);
    assignIfPresent(meta, "mac_address", row.mac_address);
    assignIfPresent(meta, "meta_schema_version", row.meta_schema_version);
    const caps = parseJson(row.caps);
    if (Array.isArray(caps)) {
        meta.caps = caps;
    }
    return meta;
}

function buildAvailabilityFromRow(row) {
    return {
        availability: normalizeString(row.availability).toLowerCase() || "unknown",
        online: toBooleanOrNull(row.online),
        last_seen_at: row.last_seen_at || null
    };
}

function buildStateFromRow(row) {
    const state = {};
    [
        "cover_mode",
        "cover_state",
        "cover_direction",
        "cover_position",
        "travel_time_ms",
        "temp_01c",
        "hum_01pct",
        "lux",
        "pressure_pa",
        "gas_ohm",
        "aqi",
        "tvoc_ppb",
        "eco2_ppm",
        "rain_raw",
        "battery_pct",
        "battery_mv",
        "button_flags",
        "button_last_action",
        "button_last_action_at"
    ].forEach((key) => assignIfPresent(state, key, row[key]));

    [
        "fault",
        "relay_1",
        "relay_2",
        "motion",
        "rain",
        "window_open",
        "cover_calibrated",
        "is_calibrated"
    ].forEach((key) => {
        const value = toBooleanOrNull(row[key]);
        if (value !== null) {
            state[key] = value;
        }
    });

    if (state.window_open !== undefined) {
        state.contact_open = state.window_open;
    }

    const coverState = normalizeString(row.cover_state).toLowerCase();
    if (coverState) {
        state.cover_moving = ["opening", "closing", "moving"].includes(coverState);
    }

    const coverCalibrated = state.cover_calibrated === true || state.is_calibrated === true;
    if (Object.prototype.hasOwnProperty.call(state, "cover_position")) {
        const normalizedCoverPosition = normalizeCoverPosition(state.cover_position, coverCalibrated);
        if (normalizedCoverPosition === null) {
            delete state.cover_position;
        } else {
            state.cover_position = normalizedCoverPosition;
        }
    }

    return state;
}

function buildConfigFromRow(row) {
    const config = {};
    [
        "report_interval_s",
        "lux_threshold_on",
        "auto_off_delay_s",
        "rain_threshold",
        "auto_up_time",
        "auto_down_time"
    ].forEach((key) => assignIfPresent(config, key, row[key]));
    const autoScheduleEnabled = toBooleanOrNull(row.auto_schedule_enabled);
    if (autoScheduleEnabled !== null) {
        config.auto_schedule_enabled = autoScheduleEnabled;
    }
    return config;
}

function buildLastEventFromRow(row) {
    if (!row.last_event_at) {
        return null;
    }
    return {
        event_type: row.last_event_type || null,
        event_label: row.last_event_label || null,
        event_trigger: row.last_event_trigger || null,
        param1: row.last_event_param1 || null,
        param2: row.last_event_param2 || null,
        event_at: row.last_event_at
    };
}

function buildLastAckFromRow(row) {
    if (!row.last_ack_at && !row.last_ack_request_id) {
        return null;
    }
    return {
        request_id: row.last_ack_request_id || null,
        channel: row.last_ack_channel || null,
        status: row.last_ack_status || null,
        status_code: row.last_ack_status_code || null,
        ack_msg_type: row.last_ack_msg_type || null,
        ack_seq: row.last_ack_seq || null,
        source: row.last_ack_source || null,
        ack_at: row.last_ack_at || null
    };
}

function formatTimestamp(value) {
    const text = normalizeString(value);
    if (!text) return "-";
    const date = new Date(text);
    if (Number.isNaN(date.getTime())) return text;
    return new Intl.DateTimeFormat("de-DE", {
        day: "2-digit",
        month: "2-digit",
        year: "numeric",
        hour: "2-digit",
        minute: "2-digit",
        second: "2-digit",
        timeZone: process.env.TZ || "Europe/Berlin"
    }).format(date);
}

function relativeTimestamp(value) {
    const text = normalizeString(value);
    if (!text) return "-";
    const ts = Date.parse(text);
    if (!Number.isFinite(ts)) return text;
    const deltaSeconds = Math.max(0, Math.floor((Date.now() - ts) / 1000));
    if (deltaSeconds < 60) return "vor " + deltaSeconds + " s";
    if (deltaSeconds < 3600) return "vor " + Math.floor(deltaSeconds / 60) + " min";
    if (deltaSeconds < 86400) return "vor " + Math.floor(deltaSeconds / 3600) + " h";
    return formatTimestamp(text);
}

function suppressDisplayOnlyStateNoise(device, state, meta) {
    const suppressedKeys = [];
    const baseType = normalizeBaseType(device.base_type || device.device_class || device.device_id);
    if (baseType === "net_sen" && Object.prototype.hasOwnProperty.call(state, "motion") && !hasCapability(meta, "motion")) {
        delete state.motion;
        suppressedKeys.push("motion");
    }
    return suppressedKeys;
}

function pickHighlights(device, state, meta) {
    const keys = [];
    if (isCoverDevice(device, state, meta)) {
        keys.push("cover_direction");
    } else if (isRelayDevice(device, state, meta)) {
        keys.push("relay_1", "relay_2");
    }
    keys.push("temp_01c", "hum_01pct", "lux", "rain", "rain_raw", "pressure_pa", "pressure_hpa", "battery_pct", "battery_mv");
    if (hasCapability(meta, "motion") || normalizeBaseType(device.base_type || device.device_class || device.device_id) !== "net_sen") {
        keys.push("motion", "presence");
    }
    keys.push("contact_open", "window_open");
    return keys
        .filter((key, index) => keys.indexOf(key) === index)
        // Der große Lampen-Button zeigt relay_1 bei net_erl schon eindeutig an. Ein zweiter Ein/Aus-Hinweis bläht die Karte nur auf.
        .filter((key) => !(isNetErlDevice(device) && key === "relay_1"))
        .filter((key) => Object.prototype.hasOwnProperty.call(state, key))
        .slice(0, 4)
        .map((key) => ({
            key,
            label: labelForStateKey(key),
            value_text: formatStateValue(key, state[key])
        }));
}

function buildControls(device, state, meta) {
    if (isCoverDevice(device, state, meta)) {
        const positionCalibrated = state.cover_calibrated === true || state.is_calibrated === true;
        const positionValue = normalizeCoverPosition(state.cover_position, positionCalibrated);
        const allowIntermediatePositions = positionValue !== null && positionCalibrated;
        const calibrationHintText = coverCalibrationHint(positionCalibrated, positionValue);
        const moving = state.cover_moving === true;
        const direction = normalizeString(state.cover_direction).toLowerCase();
        return {
            kind: "cover",
            device_id: device.device_id,
            command_url: "/api/phase1/cover/command",
            position_text: positionValue === null ? "unbekannt" : String(positionValue) + " %",
            state_text: moving ? (direction === "down" ? "fährt ab" : direction === "up" ? "fährt auf" : "in Bewegung") : "gestoppt",
            position_value: positionValue === null ? 0 : positionValue,
            position_known: positionValue !== null,
            position_calibrated: positionCalibrated,
            allow_intermediate_positions: allowIntermediatePositions,
            calibration_hint_text: calibrationHintText,
            // Endlagen bleiben auch ohne Kalibrierung bedienbar. Nur Zwischenwerte hängen an verlässlicher Positionskalibrierung.
            allow_end_positions: true,
            // Die Übersicht bekommt bewusst nur die kompakte Direktbedienung; der volle Positionszugriff bleibt in der Detailseite.
            allow_overview_controls: true,
            motion_class: moving ? (direction === "down" ? "is-moving-down" : "is-moving-up") : "",
            shutter_style: positionValue === null ? "" : "height:" + String(positionValue) + "%;"
        };
    }
    if (isRelayDevice(device, state, meta)) {
        const hasRelay1 = Object.prototype.hasOwnProperty.call(state, "relay_1");
        const relay1Value = hasRelay1 ? state.relay_1 === true : null;
        return {
            kind: "relay",
            device_id: device.device_id,
            command_url: "/api/phase1/net-erl/relay-1",
            relay_1_value: relay1Value,
            allow_overview_toggle: isNetErlDevice(device) && hasRelay1,
            relays: ["relay_1", "relay_2"]
                .filter((key) => Object.prototype.hasOwnProperty.call(state, key))
                .map((key) => ({
                    key,
                    label: labelForStateKey(key),
                    state_label: state[key] === true ? "An" : "Aus"
                }))
        };
    }
    return null;
}

function classifyDevice(device, state, meta) {
    const baseType = normalizeBaseType(device.base_type || device.device_class || device.device_id);
    if (baseType === "master") {
        return { kind_label: "Master", icon: "mdi-router-wireless", surface_class: "is-master-card" };
    }
    if (isCoverDevice(device, state, meta)) {
        return { kind_label: "Rollladen", icon: "mdi-window-shutter", surface_class: "is-cover-card" };
    }
    if (isRelayDevice(device, state, meta)) {
        return { kind_label: "Aktor", icon: "mdi-lightbulb-outline", surface_class: "is-relay-card" };
    }
    if (isBatteryDevice(device)) {
        return { kind_label: "Batteriegerät", icon: "mdi-battery-outline", surface_class: "is-battery-card" };
    }
    return { kind_label: "Sensor", icon: "mdi-thermometer", surface_class: "is-sensor-card" };
}

function describeDevice(row) {
    const meta = buildMetaFromRow(row);
    const availability = buildAvailabilityFromRow(row);
    const state = buildStateFromRow(row);
    const config = buildConfigFromRow(row);
    const lastEvent = buildLastEventFromRow(row);
    const lastAck = buildLastAckFromRow(row);
    const preliminaryDevice = {
        device_id: row.device_id,
        base_type: normalizeBaseType(row.device_class || row.device_id),
        device_class: normalizeBaseType(row.device_class || row.device_id)
    };
    const suppressedStateKeys = suppressDisplayOnlyStateNoise(preliminaryDevice, state, meta);
    const diagnostics = {
        device_updated_at: row.device_updated_at || null,
        state_updated_at: row.state_updated_at || null,
        suppressed_display_state_keys: suppressedStateKeys,
        rain_from_last_event: Boolean(lastEvent && lastEvent.event_label === "rain_detected" && Object.prototype.hasOwnProperty.call(state, "rain"))
    };
    const customName = normalizeString(row.dashboard_display_name);
    const metaName = normalizeString(row.device_name);
    const device = {
        device_id: row.device_id,
        base_type: normalizeBaseType(row.device_class || row.device_id),
        device_class: normalizeBaseType(row.device_class || row.device_id),
        profile: normalizeString(row.config_profile),
        display_name: customName || metaName || row.device_id,
        custom_name: customName,
        has_custom_name: Boolean(customName),
        meta,
        availability,
        state,
        config,
        last_event: lastEvent,
        last_ack: lastAck,
        diagnostics,
        last_seen_at: row.last_seen_at,
        last_availability_at: row.last_seen_at,
        last_state_at: row.state_updated_at || row.last_seen_at,
        last_meta_at: row.device_updated_at,
        last_event_at: row.last_event_at,
        last_ack_at: row.last_ack_at,
        simulation: row.device_id.startsWith("sim_")
            || normalizeString(meta.source).toLowerCase() === "simulation"
            || Boolean(meta.sim_case)
    };
    const availabilityData = availabilityInfo(availability);
    const classification = classifyDevice(device, state, meta);
    const controls = buildControls(device, state, meta);
    const batteryValue = Object.prototype.hasOwnProperty.call(state, "battery_pct")
        ? formatStateValue("battery_pct", state.battery_pct)
        : Object.prototype.hasOwnProperty.call(state, "battery_mv")
            ? formatStateValue("battery_mv", state.battery_mv)
            : "";
    return Object.assign(device, {
        availability_label: availabilityData.label,
        availability_class: availabilityData.className,
        kind_label: classification.kind_label,
        type_icon: classification.icon,
        surface_class: classification.surface_class,
        controls,
        highlight_values: pickHighlights(device, state, meta),
        battery_value: batteryValue,
        detail_url: "/dashboard/geraet?device=" + encodeURIComponent(row.device_id),
        status_time_label: "Zuletzt gesehen",
        status_time_value: relativeTimestamp(row.last_seen_at),
        status_time_exact_label: formatTimestamp(row.last_seen_at)
    });
}

function buildOverviewQuery() {
    return [
        "SELECT",
        "    d.device_id,",
        "    d.device_name,",
        "    d.dashboard_display_name,",
        "    d.device_class,",
        "    d.power_type,",
        "    d.fw_version,",
        "    d.caps,",
        "    d.control_mode,",
        "    d.config_profile,",
        "    d.reporting_mode,",
        "    d.sensor_mask,",
        "    d.input_mask,",
        "    d.mac_address,",
        "    d.meta_schema_version,",
        "    d.updated_at AS device_updated_at,",
        "    l.availability,",
        "    l.online,",
        "    l.last_seen_at,",
        "    l.fault,",
        "    l.relay_1,",
        "    l.relay_2,",
        "    l.cover_mode,",
        "    l.cover_state,",
        "    l.cover_direction,",
        "    l.cover_position,",
        "    l.cover_calibrated,",
        "    l.is_calibrated,",
        "    l.travel_time_ms,",
        "    l.temp_01c,",
        "    l.hum_01pct,",
        "    l.lux,",
        "    l.pressure_pa,",
        "    l.gas_ohm,",
        "    l.aqi,",
        "    l.tvoc_ppb,",
        "    l.eco2_ppm,",
        "    l.motion,",
        "    l.rain,",
        "    l.rain_raw,",
        "    l.window_open,",
        "    l.battery_pct,",
        "    l.battery_mv,",
        "    l.button_flags,",
        "    l.button_last_action,",
        "    l.button_last_action_at,",
        "    l.report_interval_s,",
        "    l.lux_threshold_on,",
        "    l.auto_off_delay_s,",
        "    l.rain_threshold,",
        "    l.auto_up_time,",
        "    l.auto_down_time,",
        "    l.auto_schedule_enabled,",
        "    l.last_event_type,",
        "    l.last_event_label,",
        "    l.last_event_trigger,",
        "    l.last_event_param1,",
        "    l.last_event_param2,",
        "    l.last_event_at,",
        "    l.last_ack_request_id,",
        "    l.last_ack_channel,",
        "    l.last_ack_status,",
        "    l.last_ack_status_code,",
        "    l.last_ack_msg_type,",
        "    l.last_ack_seq,",
        "    l.last_ack_source,",
        "    l.last_ack_at,",
        "    l.updated_at AS state_updated_at,",
        "    (SELECT COUNT(*) FROM master_status) AS master_count",
        "FROM devices AS d",
        "LEFT JOIN device_state_latest AS l ON l.device_id = d.device_id",
        "ORDER BY",
        "    CASE",
        "        WHEN l.online = 1 OR lower(COALESCE(l.availability, '')) = 'online' THEN 0",
        "        WHEN lower(COALESCE(l.availability, '')) IN ('late', 'asleep', 'sleeping_expected') THEN 1",
        "        WHEN lower(COALESCE(l.availability, '')) = 'offline' THEN 3",
        "        ELSE 2",
        "    END,",
        "    lower(COALESCE(d.device_name, d.device_id));"
    ].join(" ");
}

function buildOverviewPayload(rows) {
    const devices = (rows || []).map(describeDevice)
        .filter((device) => device.base_type !== "master" && device.device_class !== "master");
    const masterCount = rows && rows.length ? Number(rows[0].master_count || 0) : 0;
    const offlineCount = devices.filter((device) => device.availability_label === "offline").length;
    return {
        page: { key: "overview", title: "Geräteübersicht" },
        summary: {
            device_count: devices.length,
            master_count: masterCount,
            active_count: devices.length - offlineCount,
            offline_count: offlineCount
        },
        devices
    };
}

function buildDeviceDetailQuery(deviceId) {
    const normalizedId = normalizeString(deviceId);
    if (!normalizedId) {
        return null;
    }
    return [
        "SELECT",
        "    d.device_id,",
        "    d.device_name,",
        "    d.dashboard_display_name,",
        "    d.device_class,",
        "    d.power_type,",
        "    d.fw_version,",
        "    d.caps,",
        "    d.control_mode,",
        "    d.config_profile,",
        "    d.reporting_mode,",
        "    d.sensor_mask,",
        "    d.input_mask,",
        "    d.mac_address,",
        "    d.meta_schema_version,",
        "    d.updated_at AS device_updated_at,",
        "    l.availability,",
        "    l.online,",
        "    l.last_seen_at,",
        "    l.fault,",
        "    l.relay_1,",
        "    l.relay_2,",
        "    l.cover_mode,",
        "    l.cover_state,",
        "    l.cover_direction,",
        "    l.cover_position,",
        "    l.cover_calibrated,",
        "    l.is_calibrated,",
        "    l.travel_time_ms,",
        "    l.temp_01c,",
        "    l.hum_01pct,",
        "    l.lux,",
        "    l.pressure_pa,",
        "    l.gas_ohm,",
        "    l.aqi,",
        "    l.tvoc_ppb,",
        "    l.eco2_ppm,",
        "    l.motion,",
        "    l.rain,",
        "    l.rain_raw,",
        "    l.window_open,",
        "    l.battery_pct,",
        "    l.battery_mv,",
        "    l.button_flags,",
        "    l.button_last_action,",
        "    l.button_last_action_at,",
        "    l.report_interval_s,",
        "    l.lux_threshold_on,",
        "    l.auto_off_delay_s,",
        "    l.rain_threshold,",
        "    l.auto_up_time,",
        "    l.auto_down_time,",
        "    l.auto_schedule_enabled,",
        "    l.last_event_type,",
        "    l.last_event_label,",
        "    l.last_event_trigger,",
        "    l.last_event_param1,",
        "    l.last_event_param2,",
        "    l.last_event_at,",
        "    l.last_ack_request_id,",
        "    l.last_ack_channel,",
        "    l.last_ack_status,",
        "    l.last_ack_status_code,",
        "    l.last_ack_msg_type,",
        "    l.last_ack_seq,",
        "    l.last_ack_source,",
        "    l.last_ack_at,",
        "    l.updated_at AS state_updated_at",
        "FROM devices AS d",
        "LEFT JOIN device_state_latest AS l ON l.device_id = d.device_id",
        "WHERE d.device_id = " + sqlStringLiteral(normalizedId) + ";"
    ].join(" ");
}

function buildDeviceDetailPayload(rows) {
    const row = Array.isArray(rows) && rows.length ? rows[0] : null;
    if (!row) {
        return { kind: "detail", device: null, technical_rows: [], state_rows: [], raw_sections: [] };
    }
    const device = describeDevice(row);
    const meta = device.meta || {};
    return {
        kind: "detail",
        device,
        technical_rows: [
            { label: "Basistyp", value_text: device.base_type || "-" },
            { label: "Device-Class", value_text: device.device_class || "-" },
            { label: "Profil", value_text: device.profile || "-" },
            { label: "Meta-Schema", value_text: normalizeString(meta.meta_schema_version) || "-" },
            { label: "Control-Mode", value_text: normalizeString(meta.control_mode) || "-" },
            { label: "Config-Profil", value_text: normalizeString(meta.config_profile) || "-" },
            { label: "Reporting-Mode", value_text: normalizeString(meta.reporting_mode) || "-" },
            { label: "Sensor-Maske", value_text: normalizeString(meta.sensor_mask) || "-" },
            { label: "Input-Maske", value_text: normalizeString(meta.input_mask) || "-" },
            { label: "Letzte Availability", value_text: formatTimestamp(device.last_availability_at) },
            { label: "Letzter State", value_text: formatTimestamp(device.last_state_at) },
            { label: "Letztes Meta", value_text: formatTimestamp(device.last_meta_at) },
            { label: "Letztes Event", value_text: formatTimestamp(device.last_event_at) },
            { label: "Letztes Ack", value_text: formatTimestamp(device.last_ack_at) }
        ],
        state_rows: Object.keys(device.state || {}).sort().map((key) => ({
            key,
            label: labelForStateKey(key),
            value_text: formatStateValue(key, device.state[key])
        })),
        raw_sections: [
            { key: "availability", label: "Availability JSON", value_text: JSON.stringify(device.availability || {}, null, 2) },
            { key: "state", label: "State JSON", value_text: JSON.stringify(device.state || {}, null, 2) },
            { key: "config", label: "Config JSON", value_text: JSON.stringify(device.config || {}, null, 2) },
            { key: "event", label: "Last Event JSON", value_text: JSON.stringify(device.last_event || {}, null, 2) },
            { key: "ack", label: "Last Ack JSON", value_text: JSON.stringify(device.last_ack || {}, null, 2) },
            { key: "meta", label: "Meta JSON", value_text: JSON.stringify(device.meta || {}, null, 2) },
            { key: "diagnostics", label: "Diagnostics JSON", value_text: JSON.stringify(device.diagnostics || {}, null, 2) }
        ]
    };
}

module.exports = {
    buildDeviceDeleteQuery,
    buildDeviceDetailPayload,
    buildDeviceDetailQuery,
    buildOverviewPayload,
    buildOverviewQuery
};
