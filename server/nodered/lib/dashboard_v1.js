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

function normalizeBaseType(value) {
    const text = normalizeString(value).toLowerCase();
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

function formatStateValue(key, value) {
    if (value === null || value === undefined) return "-";
    if (["relay_1", "relay_2"].includes(key)) return value ? "An" : "Aus";
    if (["motion", "presence"].includes(key)) return value ? "Erkannt" : "Nein";
    if (key === "contact_open") return value ? "Offen" : "Geschlossen";
    if (key === "cover_moving") return value ? "Ja" : "Nein";
    if (["cover_position", "cover_target", "battery_pct"].includes(key)) return formatNumber(value, 0, "%");
    if (key === "battery_mv") return formatNumber(value, 0, "mV");
    if (key === "temp_01c") return formatNumber(value, 10, "°C");
    if (key === "hum_01pct") return formatNumber(value, 10, "%");
    if (key === "pressure_hpa") return formatNumber(value, 0, "hPa");
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
        contact_open: "Kontakt",
        cover_direction: "Richtung",
        cover_moving: "Bewegung",
        cover_position: "Position",
        cover_target: "Ziel",
        gas_ohm: "Gas",
        hum_01pct: "Luftfeuchte",
        lux_01lx: "Helligkeit",
        motion: "Bewegung",
        presence: "Präsenz",
        pressure_hpa: "Druck",
        relay_1: "Relais 1",
        relay_2: "Relais 2",
        temp_01c: "Temperatur"
    };
    return labels[key] || key.replace(/_/g, " ");
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

function pickHighlights(device, state, meta) {
    const keys = [];
    if (isCoverDevice(device, state, meta)) {
        keys.push("cover_position", "cover_direction", "cover_moving");
    } else if (isRelayDevice(device, state, meta)) {
        keys.push("relay_1", "relay_2");
    }
    keys.push("temp_01c", "hum_01pct", "lux_01lx", "battery_pct", "battery_mv", "motion", "presence", "contact_open");
    return keys
        .filter((key, index) => keys.indexOf(key) === index)
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
        const positionValue = typeof state.cover_position === "number" ? Math.max(0, Math.min(100, state.cover_position)) : null;
        const moving = state.cover_moving === true;
        const direction = normalizeString(state.cover_direction).toLowerCase();
        return {
            kind: "cover",
            position_text: positionValue === null ? "unbekannt" : String(positionValue) + " %",
            state_text: moving ? (direction === "down" ? "fährt ab" : direction === "up" ? "fährt auf" : "in Bewegung") : "gestoppt",
            position_value: positionValue === null ? 55 : positionValue,
            position_known: positionValue !== null,
            motion_class: moving ? (direction === "down" ? "is-moving-down" : "is-moving-up") : "",
            shutter_style: positionValue === null ? "" : "height:" + String(positionValue) + "%;"
        };
    }
    if (isRelayDevice(device, state, meta)) {
        return {
            kind: "relay",
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
    const identity = parseJson(row.identity_json) || {};
    const meta = parseJson(row.meta_json) || {};
    const availability = parseJson(row.availability_json) || {};
    const state = parseJson(row.state_json) || {};
    const config = parseJson(row.config_json) || {};
    const lastEvent = parseJson(row.last_event_json) || null;
    const lastAck = parseJson(row.last_ack_json) || null;
    const diagnostics = parseJson(row.diagnostics_json) || {};
    const device = {
        device_id: row.device_id,
        base_type: normalizeBaseType(row.base_type || row.device_class || identity.base_type || row.device_id),
        device_class: normalizeBaseType(row.device_class || identity.device_class || row.base_type || row.device_id),
        profile: normalizeString(row.profile || identity.profile),
        display_name: normalizeString(row.display_name || identity.display_name || row.device_id) || row.device_id,
        meta,
        availability,
        state,
        config,
        last_event: lastEvent,
        last_ack: lastAck,
        diagnostics,
        last_seen_at: row.last_seen_at,
        last_availability_at: row.last_availability_at,
        last_state_at: row.last_state_at,
        last_meta_at: row.last_meta_at,
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
        "    d.base_type,",
        "    d.device_class,",
        "    d.profile,",
        "    d.display_name,",
        "    d.identity_json,",
        "    d.meta_json,",
        "    d.updated_at,",
        "    d.last_seen_at,",
        "    d.last_meta_at,",
        "    d.last_availability_at,",
        "    d.last_state_at,",
        "    d.last_event_at,",
        "    d.last_ack_at,",
        "    l.availability_json,",
        "    l.state_json,",
        "    l.config_json,",
        "    l.last_event_json,",
        "    l.last_ack_json,",
        "    l.diagnostics_json,",
        "    (SELECT COUNT(*) FROM master_status) AS master_count",
        "FROM devices AS d",
        "LEFT JOIN device_state_latest AS l ON l.device_id = d.device_id",
        "ORDER BY lower(COALESCE(d.display_name, d.device_id));"
    ].join(" ");
}

function buildOverviewPayload(rows) {
    const devices = (rows || []).map(describeDevice)
        .filter((device) => device.base_type !== "master" && device.device_class !== "master");
    const masterCount = rows && rows.length ? Number(rows[0].master_count || 0) : 0;
    return {
        page: { key: "overview", title: "Geräteübersicht" },
        summary: {
            device_count: devices.length,
            master_count: masterCount
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
        "    d.base_type,",
        "    d.device_class,",
        "    d.profile,",
        "    d.display_name,",
        "    d.identity_json,",
        "    d.meta_json,",
        "    d.updated_at,",
        "    d.last_seen_at,",
        "    d.last_meta_at,",
        "    d.last_availability_at,",
        "    d.last_state_at,",
        "    d.last_event_at,",
        "    d.last_ack_at,",
        "    l.availability_json,",
        "    l.state_json,",
        "    l.config_json,",
        "    l.last_event_json,",
        "    l.last_ack_json,",
        "    l.diagnostics_json",
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
    buildDeviceDetailPayload,
    buildDeviceDetailQuery,
    buildOverviewPayload,
    buildOverviewQuery
};
