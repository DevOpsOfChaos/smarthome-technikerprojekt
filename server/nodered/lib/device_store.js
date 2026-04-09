"use strict";

const capabilityHelpers = require("./capability_helpers");
const timeHelpers = require("./time_helpers");

function createRuntimeState() {
    return {
        devices: {},
        masters: {},
        initialized_at: timeHelpers.nowIso(),
        schema_version: 1
    };
}

function createEmptyDevice(deviceId, now) {
    const baseType = capabilityHelpers.inferBaseTypeFromDeviceId(deviceId);
    return {
        identity: {
            device_id: deviceId,
            base_type: baseType,
            device_class: baseType,
            profile: null,
            display_name: deviceId
        },
        meta: null,
        availability: null,
        state: {},
        config: {},
        last_event: null,
        last_ack: null,
        diagnostics: {
            auto_created: true,
            dropped_state_fields: [],
            last_handler: null,
            last_topic: null
        },
        created_at: now,
        updated_at: now,
        first_seen_at: now,
        last_seen_at: now,
        last_meta_at: null,
        last_availability_at: null,
        last_state_at: null,
        last_event_at: null,
        last_ack_at: null
    };
}

function createEmptyMaster(masterId, now) {
    return {
        master_id: masterId,
        status: null,
        last_event: null,
        diagnostics: {
            last_handler: null,
            last_topic: null
        },
        created_at: now,
        updated_at: now,
        first_seen_at: now,
        last_seen_at: now,
        last_status_at: null,
        last_event_at: null
    };
}

function ensureDevice(runtimeState, deviceId, now, options) {
    const runtime = runtimeState || createRuntimeState();
    const opts = options || {};
    let created = false;

    if (!runtime.devices[deviceId]) {
        if (!opts.autoCreate) {
            return { created: false, device: null, runtime };
        }
        runtime.devices[deviceId] = createEmptyDevice(deviceId, now);
        created = true;
    }

    return {
        created,
        device: runtime.devices[deviceId],
        runtime
    };
}

function ensureMaster(runtimeState, masterId, now) {
    const runtime = runtimeState || createRuntimeState();
    let created = false;

    if (!runtime.masters[masterId]) {
        runtime.masters[masterId] = createEmptyMaster(masterId, now);
        created = true;
    }

    return {
        created,
        master: runtime.masters[masterId],
        runtime
    };
}

function buildDeviceRow(device) {
    return {
        device_id: device.identity.device_id,
        device_role: "node",
        base_type: device.identity.base_type,
        device_class: device.identity.device_class,
        profile: device.identity.profile,
        display_name: device.identity.display_name,
        default_name: device.identity.device_id,
        identity_json: device.identity,
        meta_json: device.meta,
        created_at: device.created_at,
        updated_at: device.updated_at,
        first_seen_at: device.first_seen_at,
        last_seen_at: device.last_seen_at,
        last_meta_at: device.last_meta_at,
        last_availability_at: device.last_availability_at,
        last_state_at: device.last_state_at,
        last_event_at: device.last_event_at,
        last_ack_at: device.last_ack_at
    };
}

function buildDeviceStateLatestRow(device) {
    return {
        device_id: device.identity.device_id,
        identity_json: device.identity,
        meta_json: device.meta,
        availability_json: device.availability,
        state_json: device.state,
        config_json: device.config,
        last_event_json: device.last_event,
        last_ack_json: device.last_ack,
        diagnostics_json: device.diagnostics,
        created_at: device.created_at,
        updated_at: device.updated_at,
        first_seen_at: device.first_seen_at,
        last_seen_at: device.last_seen_at,
        last_meta_at: device.last_meta_at,
        last_availability_at: device.last_availability_at,
        last_state_at: device.last_state_at,
        last_event_at: device.last_event_at,
        last_ack_at: device.last_ack_at
    };
}

function buildMasterStatusRow(master) {
    return {
        master_id: master.master_id,
        status_json: master.status,
        last_status_at: master.last_status_at,
        last_event_json: master.last_event,
        last_event_at: master.last_event_at,
        diagnostics_json: master.diagnostics,
        created_at: master.created_at,
        updated_at: master.updated_at,
        first_seen_at: master.first_seen_at,
        last_seen_at: master.last_seen_at
    };
}

function buildUpsertSql(tableName, keyColumn, row, jsonColumns, updateColumns) {
    const columns = Object.keys(row);
    const jsonColumnSet = new Set(jsonColumns || []);
    const updates = updateColumns || columns.filter((columnName) => columnName !== keyColumn && columnName !== "created_at" && columnName !== "first_seen_at");
    const values = columns.map((columnName) => {
        const value = row[columnName];
        return jsonColumnSet.has(columnName)
            ? timeHelpers.toSqlJsonLiteral(value)
            : timeHelpers.toSqlLiteral(value);
    });

    return [
        "INSERT INTO " + tableName + " (" + columns.join(", ") + ")",
        "VALUES (" + values.join(", ") + ")",
        "ON CONFLICT(" + keyColumn + ") DO UPDATE SET",
        updates.map((columnName) => columnName + " = excluded." + columnName).join(", "),
        ";"
    ].join(" ");
}

function buildDevicesUpsertSql(row) {
    return buildUpsertSql(
        "devices",
        "device_id",
        row,
        ["identity_json", "meta_json"]
    );
}

function buildDeviceStateLatestUpsertSql(row) {
    return buildUpsertSql(
        "device_state_latest",
        "device_id",
        row,
        [
            "identity_json",
            "meta_json",
            "availability_json",
            "state_json",
            "config_json",
            "last_event_json",
            "last_ack_json",
            "diagnostics_json"
        ]
    );
}

function buildMasterStatusUpsertSql(row) {
    return buildUpsertSql(
        "master_status",
        "master_id",
        row,
        ["status_json", "last_event_json", "diagnostics_json"]
    );
}

module.exports = {
    buildDeviceRow,
    buildDeviceStateLatestRow,
    buildDeviceStateLatestUpsertSql,
    buildDevicesUpsertSql,
    buildMasterStatusRow,
    buildMasterStatusUpsertSql,
    createRuntimeState,
    ensureDevice,
    ensureMaster
};
