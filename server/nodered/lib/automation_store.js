/**
 * =============================================================================
 * @modul     automation_store
 * @beschreibung  SQL-Abfragen und Eingabevalidierung für Automatisierungen.
 *
 * @prinzip   Alle Funktionen sind zustandslos. Eingabe sind Roh-Payloads aus
 *            HTTP-Anfragen, Ausgabe validierte SQL-Strings oder Fehlerobjekte.
 *            Keine direkte DB-Verbindung – der aufrufende Flow übergibt die
 *            Ergebnisse dem SQLite-Node via msg.topic.
 *
 * @tabellen
 *   - automations             → Stammdaten + Aktionsdefinition
 *   - automation_conditions   → Bedingungen (1:n, CASCADE)
 *
 * @funktionen
 *   - validateAutomation      → Eingabe prüfen, normalisierten Record zurückgeben
 *   - validateCondition       → Einzelne Bedingung prüfen
 *   - buildListQuery          → SELECT alle Automatisierungen (mit Bedingungsanzahl)
 *   - buildGetQuery           → SELECT eine Automatisierung + Bedingungen
 *   - buildUpsertStatements   → INSERT OR REPLACE für automation + conditions
 *   - buildDeleteStatement    → DELETE automation (Bedingungen via CASCADE)
 *   - buildUpdateEnabledStatement → enabled-Flag toggeln
 *
 * @nutzung   70_dashboard_automations.json, 71_automation_runner.json
 * @export    validateAutomation, validateCondition, buildListQuery,
 *            buildGetQuery, buildUpsertStatements, buildDeleteStatement,
 *            buildUpdateEnabledStatement
 * =============================================================================
 */

"use strict";

// ===========================================================================
// KONSTANTEN
// ===========================================================================

/** Erlaubte Operatoren für lokale Bedingungen. */
const ALLOWED_OPERATORS = new Set(["eq", "neq", "gt", "gte", "lt", "lte"]);

/** Erlaubte Aktionstypen – nur über commandMinimal realisiert. */
const ALLOWED_ACTION_KINDS = new Set(["net_erl_relay_1", "net_zrl_cover"]);

/** Erlaubte Bedingungsscopes. */
const ALLOWED_SCOPES = new Set(["global", "local"]);

/** Erlaubte Bedingungsarten. */
const ALLOWED_KINDS = new Set(["weekdays", "time_exact", "time_window", "device_state"]);

// ===========================================================================
// PRIVATE HILFSFUNKTIONEN
// ===========================================================================

/**
 * Gibt einen String zurück, getrimmt. Leerer Wert → "".
 * @param {*} value
 * @returns {string}
 */
function str(value) {
    return typeof value === "string" ? value.trim() : "";
}

/**
 * Wandelt einen Wert in ein SQLite-String-Literal um (einfache Anführungszeichen).
 * Schützt vor SQL-Injection durch Escaping innerer Apostrophe.
 * @param {*} value
 * @returns {string} z. B. 'mein wert'
 */
function sqlStr(value) {
    return "'" + String(value).replace(/'/g, "''") + "'";
}

/**
 * Gibt den aktuellen ISO-Zeitstempel zurück (UTC).
 * @returns {string}
 */
function nowIso() {
    return new Date().toISOString();
}

/**
 * Erzeugt eine einfache UUID-ähnliche ID aus Zeitstempel + Zufallszahl.
 * Kein kryptografisches Zufall, aber für DB-PKs ausreichend eindeutig.
 * @param {string} prefix - Präfix für Lesbarkeit (z. B. "auto", "cond")
 * @returns {string}
 */
function generateId(prefix) {
    const ts = Date.now().toString(36);
    const rand = Math.random().toString(36).slice(2, 8);
    return (prefix || "id") + "_" + ts + "_" + rand;
}

/**
 * Baut einen Fehlerobjekt zurück.
 * @param {string} field   - Betroffenes Feld
 * @param {string} message - Klartextmeldung
 * @returns {{ ok: false, field: string, message: string }}
 */
function fieldError(field, message) {
    return { ok: false, field, message };
}

// ===========================================================================
// VALIDIERUNG: AUTOMATISIERUNG
// ===========================================================================

/**
 * Validiert und normalisiert einen Automatisierungs-Record aus einem HTTP-Payload.
 *
 * Pflichtfelder: name, target_device_id, action_kind, action_payload.
 * action_kind muss einer der erlaubten Werte (ALLOWED_ACTION_KINDS) sein.
 * action_payload muss ein gültiges JSON-Objekt (String oder Objekt) sein.
 * enabled wird auf 0/1 normiert (Standard: 1).
 * automation_id wird übernommen oder neu generiert.
 *
 * @param {object} input - Rohdaten aus HTTP-Anfrage
 * @returns {{ ok: true, record: object } | { ok: false, field: string, message: string }}
 */
function validateAutomation(input) {
    const name = str(input && input.name);
    if (!name) {
        return fieldError("name", "name ist erforderlich");
    }

    const targetDeviceId = str(input && input.target_device_id);
    if (!targetDeviceId) {
        return fieldError("target_device_id", "target_device_id ist erforderlich");
    }

    const actionKind = str(input && input.action_kind);
    if (!ALLOWED_ACTION_KINDS.has(actionKind)) {
        return fieldError(
            "action_kind",
            "action_kind muss net_erl_relay_1 oder net_zrl_cover sein"
        );
    }

    // action_payload: Objekt oder JSON-String akzeptieren, als String speichern
    let actionPayloadObj = input && input.action_payload;
    if (typeof actionPayloadObj === "string") {
        try {
            actionPayloadObj = JSON.parse(actionPayloadObj);
        } catch (_err) {
            return fieldError("action_payload", "action_payload muss gültiges JSON sein");
        }
    }
    if (!actionPayloadObj || typeof actionPayloadObj !== "object" || Array.isArray(actionPayloadObj)) {
        return fieldError("action_payload", "action_payload muss ein Objekt sein");
    }
    const actionPayloadStr = JSON.stringify(actionPayloadObj);

    // enabled: boolean, "1"/"0", 1/0 akzeptieren
    let enabled = 1;
    const rawEnabled = input && input.enabled;
    if (rawEnabled !== undefined && rawEnabled !== null) {
        enabled = (rawEnabled === true || rawEnabled === 1 || rawEnabled === "1") ? 1 : 0;
    }

    const now = nowIso();
    const automationId = str(input && input.automation_id) || generateId("auto");

    return {
        ok: true,
        record: {
            automation_id: automationId,
            name,
            enabled,
            target_device_id: targetDeviceId,
            action_kind: actionKind,
            action_payload: actionPayloadStr,
            created_at: str(input && input.created_at) || now,
            updated_at: now,
            last_run_at: (input && input.last_run_at) ? str(input.last_run_at) : null,
            last_result: (input && input.last_result) ? str(input.last_result) : null
        }
    };
}

// ===========================================================================
// VALIDIERUNG: BEDINGUNG
// ===========================================================================

/**
 * Validiert eine einzelne Bedingung aus dem Automatisierungs-Payload.
 *
 * Globale Bedingungen:
 *   - weekdays:     weekdays muss gesetzt sein (JSON-Array oder CSV)
 *   - time_exact:   time_start muss im Format HH:MM sein
 *   - time_window:  time_start und time_end müssen gesetzt sein
 *
 * Lokale Bedingungen (device_state):
 *   - source_device_id, field_name, operator (ALLOWED_OPERATORS), expected_value
 *
 * @param {object} input        - Rohdaten einer Bedingung
 * @param {string} automationId - Zugehörige Automatisierungs-ID
 * @returns {{ ok: true, record: object } | { ok: false, field: string, message: string }}
 */
function validateCondition(input, automationId) {
    if (!automationId) {
        return fieldError("automation_id", "automation_id ist erforderlich");
    }

    const scope = str(input && input.condition_scope);
    if (!ALLOWED_SCOPES.has(scope)) {
        return fieldError("condition_scope", "condition_scope muss global oder local sein");
    }

    const kind = str(input && input.condition_kind);
    if (!ALLOWED_KINDS.has(kind)) {
        return fieldError(
            "condition_kind",
            "condition_kind muss weekdays, time_exact, time_window oder device_state sein"
        );
    }

    // Globale Bedingungen: scope muss "global" sein für Zeit-/Wochentag-Arten
    if (["weekdays", "time_exact", "time_window"].includes(kind) && scope !== "global") {
        return fieldError("condition_scope", kind + " erfordert scope global");
    }
    if (kind === "device_state" && scope !== "local") {
        return fieldError("condition_scope", "device_state erfordert scope local");
    }

    const now = nowIso();
    const conditionId = str(input && input.condition_id) || generateId("cond");

    // Basisstruktur – optionale Felder werden je nach kind gefüllt
    const record = {
        condition_id: conditionId,
        automation_id: automationId,
        condition_scope: scope,
        condition_kind: kind,
        source_device_id: null,
        field_name: null,
        operator: null,
        expected_value: null,
        weekdays: null,
        time_start: null,
        time_end: null,
        created_at: str(input && input.created_at) || now,
        updated_at: now
    };

    if (kind === "weekdays") {
        // Wochentage: Array [0..6] oder CSV-String "1,2,3"
        const rawWeekdays = input && input.weekdays;
        const weekdaysStr = Array.isArray(rawWeekdays)
            ? rawWeekdays.join(",")
            : str(rawWeekdays);
        if (!weekdaysStr) {
            return fieldError("weekdays", "weekdays ist für condition_kind weekdays erforderlich");
        }
        record.weekdays = weekdaysStr;
    }

    if (kind === "time_exact") {
        const timeStart = str(input && input.time_start);
        if (!timeStart || !/^\d{2}:\d{2}$/.test(timeStart)) {
            return fieldError("time_start", "time_start muss im Format HH:MM sein");
        }
        record.time_start = timeStart;
    }

    if (kind === "time_window") {
        const timeStart = str(input && input.time_start);
        const timeEnd = str(input && input.time_end);
        if (!timeStart || !/^\d{2}:\d{2}$/.test(timeStart)) {
            return fieldError("time_start", "time_start muss im Format HH:MM sein");
        }
        if (!timeEnd || !/^\d{2}:\d{2}$/.test(timeEnd)) {
            return fieldError("time_end", "time_end muss im Format HH:MM sein");
        }
        record.time_start = timeStart;
        record.time_end = timeEnd;
    }

    if (kind === "device_state") {
        const sourceDeviceId = str(input && input.source_device_id);
        if (!sourceDeviceId) {
            return fieldError("source_device_id", "source_device_id ist für device_state erforderlich");
        }
        const fieldName = str(input && input.field_name);
        if (!fieldName) {
            return fieldError("field_name", "field_name ist für device_state erforderlich");
        }
        const operator = str(input && input.operator);
        if (!ALLOWED_OPERATORS.has(operator)) {
            return fieldError("operator", "operator muss eq, neq, gt, gte, lt oder lte sein");
        }
        // expected_value darf auch "0" oder "false" sein – nur undefined/null ist unerlaubt
        const expectedValue = input && input.expected_value !== undefined ? String(input.expected_value) : null;
        if (expectedValue === null) {
            return fieldError("expected_value", "expected_value ist für device_state erforderlich");
        }
        record.source_device_id = sourceDeviceId;
        record.field_name = fieldName;
        record.operator = operator;
        record.expected_value = expectedValue;
    }

    return { ok: true, record };
}

// ===========================================================================
// SQL: LISTEN-ABFRAGE
// ===========================================================================

/**
 * Erzeugt die SQL-Abfrage für die Automatisierungs-Übersicht.
 *
 * Gibt alle Automatisierungen zurück, sortiert nach name.
 * condition_count wird als Subquery mitgeliefert (Anzeige in der Liste).
 *
 * @returns {string} SQL-String
 */
function buildListQuery() {
    return [
        "SELECT",
        "  a.automation_id,",
        "  a.name,",
        "  a.enabled,",
        "  a.target_device_id,",
        "  a.action_kind,",
        "  a.action_payload,",
        "  a.created_at,",
        "  a.updated_at,",
        "  a.last_run_at,",
        "  a.last_result,",
        "  (SELECT COUNT(*) FROM automation_conditions c",
        "   WHERE c.automation_id = a.automation_id) AS condition_count",
        "FROM automations a",
        "ORDER BY lower(a.name);"
    ].join(" ");
}

// ===========================================================================
// SQL: EINZELABFRAGE (mit Bedingungen)
// ===========================================================================

/**
 * Erzeugt zwei SQL-Abfragen für eine einzelne Automatisierung + ihre Bedingungen.
 * Rückgabe als Objekt, da der Flow zwei SQLite-Aufrufe benötigt.
 *
 * @param {string} automationId - automation_id
 * @returns {{ automationSql: string|null, conditionsSql: string|null }}
 */
function buildGetQuery(automationId) {
    const id = str(automationId);
    if (!id) {
        return { automationSql: null, conditionsSql: null };
    }
    const idSql = sqlStr(id);
    return {
        automationSql: [
            "SELECT * FROM automations WHERE automation_id = " + idSql + ";"
        ].join(""),
        conditionsSql: [
            "SELECT * FROM automation_conditions",
            "WHERE automation_id = " + idSql,
            "ORDER BY condition_scope, condition_kind;"
        ].join(" ")
    };
}

// ===========================================================================
// SQL: UPSERT (INSERT/UPDATE für Automatisierung + Bedingungen)
// ===========================================================================

/**
 * Erzeugt SQL-Statements für das Anlegen/Aktualisieren einer Automatisierung
 * inklusive aller Bedingungen.
 *
 * Ablauf:
 *   1. INSERT INTO automations ... ON CONFLICT DO UPDATE
 *   2. DELETE FROM automation_conditions WHERE automation_id = ...
 *   3. INSERT INTO automation_conditions (...) für jede Bedingung
 *
 * Alles in einer atomaren Transaktion (BEGIN IMMEDIATE … COMMIT).
 *
 * @param {object}   automationRecord - Validierter Record aus validateAutomation
 * @param {object[]} conditionRecords - Validierte Records aus validateCondition
 * @returns {string|null} SQL-Batch oder null bei ungültiger Eingabe
 */
function buildUpsertStatements(automationRecord, conditionRecords) {
    if (!automationRecord || !automationRecord.automation_id) {
        return null;
    }

    const r = automationRecord;
    const idSql = sqlStr(r.automation_id);

    // Kein INSERT OR REPLACE: SQLite würde dabei die alte Zeile löschen und neu
    // anlegen. Das wäre für created_at, last_run_at und abhängige Conditions
    // unnötig riskant. ON CONFLICT aktualisiert nur die bearbeitbaren Felder.
    const automationInsert = [
        "INSERT INTO automations",
        "(automation_id, name, enabled, target_device_id, action_kind, action_payload,",
        " created_at, updated_at, last_run_at, last_result)",
        "VALUES (",
        idSql + ",",
        sqlStr(r.name) + ",",
        String(r.enabled) + ",",
        sqlStr(r.target_device_id) + ",",
        sqlStr(r.action_kind) + ",",
        sqlStr(r.action_payload) + ",",
        sqlStr(r.created_at) + ",",
        sqlStr(r.updated_at) + ",",
        (r.last_run_at ? sqlStr(r.last_run_at) : "NULL") + ",",
        (r.last_result ? sqlStr(r.last_result) : "NULL"),
        ")",
        "ON CONFLICT(automation_id) DO UPDATE SET",
        "  name = excluded.name,",
        "  enabled = excluded.enabled,",
        "  target_device_id = excluded.target_device_id,",
        "  action_kind = excluded.action_kind,",
        "  action_payload = excluded.action_payload,",
        "  updated_at = excluded.updated_at;"
    ].join(" ");

    // Alle alten Bedingungen löschen (werden komplett ersetzt)
    const conditionsDelete = "DELETE FROM automation_conditions WHERE automation_id = " + idSql + ";";

    // Neue Bedingungen einfügen
    const conditionInserts = (Array.isArray(conditionRecords) ? conditionRecords : []).map((c) => {
        return [
            "INSERT INTO automation_conditions",
            "(condition_id, automation_id, condition_scope, condition_kind,",
            " source_device_id, field_name, operator, expected_value,",
            " weekdays, time_start, time_end, created_at, updated_at)",
            "VALUES (",
            sqlStr(c.condition_id) + ",",
            sqlStr(c.automation_id) + ",",
            sqlStr(c.condition_scope) + ",",
            sqlStr(c.condition_kind) + ",",
            (c.source_device_id ? sqlStr(c.source_device_id) : "NULL") + ",",
            (c.field_name ? sqlStr(c.field_name) : "NULL") + ",",
            (c.operator ? sqlStr(c.operator) : "NULL") + ",",
            (c.expected_value !== null && c.expected_value !== undefined ? sqlStr(c.expected_value) : "NULL") + ",",
            (c.weekdays ? sqlStr(c.weekdays) : "NULL") + ",",
            (c.time_start ? sqlStr(c.time_start) : "NULL") + ",",
            (c.time_end ? sqlStr(c.time_end) : "NULL") + ",",
            sqlStr(c.created_at) + ",",
            sqlStr(c.updated_at),
            ");"
        ].join(" ");
    });

    // Alles in einer Transaktion bündeln
    return [
        "BEGIN IMMEDIATE;",
        automationInsert,
        conditionsDelete,
        ...conditionInserts,
        "COMMIT;"
    ].join("\n");
}

// ===========================================================================
// SQL: LÖSCHEN
// ===========================================================================

/**
 * Erzeugt das DELETE-Statement für eine Automatisierung.
 * automation_conditions werden explizit gelöscht. SQLite-Foreign-Keys sind pro
 * Verbindung aktivierbar; der Flow soll nicht davon abhängen, ob eine spätere
 * SQLite-Verbindung PRAGMA foreign_keys korrekt gesetzt hat.
 *
 * @param {string} automationId - automation_id
 * @returns {string|null} SQL-String oder null
 */
function buildDeleteStatement(automationId) {
    const id = str(automationId);
    if (!id) {
        return null;
    }
    const idSql = sqlStr(id);
    return [
        "BEGIN IMMEDIATE;",
        "DELETE FROM automation_conditions WHERE automation_id = " + idSql + ";",
        "DELETE FROM automations WHERE automation_id = " + idSql + ";",
        "COMMIT;"
    ].join("\n");
}

// ===========================================================================
// SQL: ENABLED-TOGGLE
// ===========================================================================

/**
 * Erzeugt das UPDATE-Statement zum Umschalten des enabled-Flags.
 *
 * @param {string}  automationId - automation_id
 * @param {boolean} enabled      - Neuer Wert (true = 1, false = 0)
 * @returns {string|null} SQL-String oder null
 */
function buildUpdateEnabledStatement(automationId, enabled) {
    const id = str(automationId);
    if (!id) {
        return null;
    }
    const enabledValue = enabled ? 1 : 0;
    const now = sqlStr(nowIso());
    return [
        "UPDATE automations SET",
        "  enabled = " + enabledValue + ",",
        "  updated_at = " + now,
        "WHERE automation_id = " + sqlStr(id) + ";"
    ].join(" ");
}

// ===========================================================================
// SQL: RUNNER-ABFRAGE (alle aktiven Automatisierungen + Bedingungen)
// ===========================================================================

/**
 * Erzeugt die SQL-Abfrage für den Automation-Runner.
 * Gibt alle enabled=1 Automatisierungen zurück (ohne JOIN auf conditions –
 * der Runner lädt Bedingungen separat per buildGetQuery).
 *
 * @returns {string} SQL-String
 */
function buildEnabledListQuery() {
    return [
        "SELECT",
        "  a.*,",
        "  (SELECT COUNT(*) FROM automation_conditions c",
        "   WHERE c.automation_id = a.automation_id) AS condition_count",
        "FROM automations a",
        "WHERE a.enabled = 1",
        "ORDER BY a.automation_id;"
    ].join(" ");
}

/**
 * Erzeugt die SQL-Abfrage aller Bedingungen für eine Liste von Automatisierungen.
 * Wird vom Runner einmalig für alle aktiven Automationen geladen.
 *
 * @returns {string} SQL-String (keine Parameter – Runner lädt alles auf einmal)
 */
function buildAllConditionsForEnabledQuery() {
    return [
        "SELECT c.*",
        "FROM automation_conditions c",
        "INNER JOIN automations a ON a.automation_id = c.automation_id",
        "WHERE a.enabled = 1",
        "ORDER BY c.automation_id, c.condition_scope, c.condition_kind;"
    ].join(" ");
}

/**
 * Erzeugt ein UPDATE-Statement, das last_run_at und last_result einer
 * Automatisierung nach der Ausführung aktualisiert.
 *
 * @param {string} automationId - automation_id
 * @param {string} result       - "ok" oder Fehlermeldung
 * @returns {string|null} SQL-String oder null
 */
function buildUpdateRunResultStatement(automationId, result) {
    const id = str(automationId);
    if (!id) {
        return null;
    }
    const now = nowIso();
    return [
        "UPDATE automations SET",
        "  last_run_at = " + sqlStr(now) + ",",
        "  last_result = " + sqlStr(str(result) || "ok") + ",",
        "  updated_at = " + sqlStr(now),
        "WHERE automation_id = " + sqlStr(id) + ";"
    ].join(" ");
}

module.exports = {
    validateAutomation,
    validateCondition,
    buildListQuery,
    buildGetQuery,
    buildUpsertStatements,
    buildDeleteStatement,
    buildUpdateEnabledStatement,
    buildEnabledListQuery,
    buildAllConditionsForEnabledQuery,
    buildUpdateRunResultStatement
};
