/**
 * =============================================================================
 * @modul     automation_engine
 * @beschreibung  Auswertungslogik für den Automation-Runner.
 *
 * @prinzip   Zustandslose Funktionen: Eingabe sind DB-Rows (Automatisierungen +
 *            Bedingungen) sowie der aktuelle Flow-/Runtime-Kontext (Gerätezustände).
 *            Kein direkter SQLite-/MQTT-Aufruf – der Runner-Flow übernimmt das.
 *
 * @auswertung
 *   Bedingungen werden UND-verknüpft: Alle Bedingungen einer Automatisierung
 *   müssen erfüllt sein, damit die Aktion ausgelöst wird.
 *
 *   Globale Bedingungen (scope: global):
 *     - weekdays:    aktueller Wochentag muss in der Wochentag-Liste enthalten sein
 *     - time_exact:  aktuelle Uhrzeit muss exakt mit time_start übereinstimmen (HH:MM)
 *     - time_window: aktuelle Uhrzeit muss zwischen time_start und time_end liegen
 *
 *   Lokale Bedingungen (scope: local):
 *     - device_state: Gerätezustand aus dem Runtime-Store wird über Operator verglichen
 *
 * @duplikatsschutz
 *   Der Runner übergibt eine lastRunMap (flow context) aus
 *   { automation_id → "YYYY-MM-DD HH:MM" }.
 *   Eine Automatisierung wird nicht erneut ausgelöst, wenn sie in dieser Minute
 *   schon gelaufen ist (bzw. im Zeitfenster, das noch aktiv ist).
 *
 * @funktionen
 *   - evaluateConditions   → Prüft alle Bedingungen einer Automatisierung
 *   - shouldTrigger        → Duplikatsschutz + Bedingungsprüfung in einem
 *   - buildActionInput     → Erstellt das Eingabe-Payload für commandMinimal
 *
 * @nutzung   71_automation_runner.json
 * @export    evaluateConditions, shouldTrigger, buildActionInput
 * =============================================================================
 */

"use strict";

// ===========================================================================
// PRIVATE HELFER
// ===========================================================================

/**
 * Gibt die aktuelle Uhrzeit als "HH:MM"-String zurück (Systemzeit des Servers).
 * @param {Date} [now] - Zeitpunkt (Standard: jetzt)
 * @returns {string} z. B. "14:35"
 */
function currentHHMM(now) {
    const d = now || new Date();
    const h = String(d.getHours()).padStart(2, "0");
    const m = String(d.getMinutes()).padStart(2, "0");
    return h + ":" + m;
}

/**
 * Gibt das aktuelle Datum als "YYYY-MM-DD"-String zurück.
 * @param {Date} [now]
 * @returns {string}
 */
function currentDateStr(now) {
    const d = now || new Date();
    // Der Duplikatsschutz nutzt lokale HH:MM-Zeit. Das Datum muss deshalb
    // ebenfalls aus der lokalen Serverzeit kommen; UTC würde rund um Mitternacht
    // falsche Minutenschlüssel erzeugen.
    const y = String(d.getFullYear()).padStart(4, "0");
    const m = String(d.getMonth() + 1).padStart(2, "0");
    const day = String(d.getDate()).padStart(2, "0");
    return y + "-" + m + "-" + day;
}

/**
 * Gibt den aktuellen Wochentag als Zahl zurück (0=Sonntag, 1=Mo, …, 6=Sa).
 * @param {Date} [now]
 * @returns {number}
 */
function currentWeekday(now) {
    return (now || new Date()).getDay();
}

/**
 * Vergleicht zwei "HH:MM"-Strings lexikografisch.
 * Gibt -1, 0 oder 1 zurück.
 * @param {string} a
 * @param {string} b
 * @returns {number}
 */
function compareHHMM(a, b) {
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

/**
 * Parst einen Wochentag-String zu einem Set von Zahlen.
 * Akzeptiert CSV ("1,2,3") oder JSON-Array ("[1,2,3]").
 * @param {string} weekdaysStr
 * @returns {Set<number>}
 */
function parseWeekdays(weekdaysStr) {
    if (!weekdaysStr) {
        return new Set();
    }
    let values;
    if (weekdaysStr.startsWith("[")) {
        try {
            values = JSON.parse(weekdaysStr);
        } catch (_err) {
            values = [];
        }
    } else {
        values = weekdaysStr.split(",");
    }
    return new Set(values.map((v) => Number(String(v).trim())).filter((n) => Number.isInteger(n)));
}

/**
 * Vergleicht zwei Werte über den angegebenen Operator.
 * Erlaubte Operatoren: eq, neq, gt, gte, lt, lte.
 *
 * Wenn beide Werte numerisch sind, wird numerisch verglichen.
 * Ansonsten Zeichenkettenvergleich.
 *
 * @param {*}      actual   - Tatsächlicher Zustandswert
 * @param {string} operator - eq | neq | gt | gte | lt | lte
 * @param {string} expected - Erwarteter Wert als String
 * @returns {boolean}
 */
function applyOperator(actual, operator, expected) {
    // Boolean-Normalisierung für relay, motion, etc.
    const actualBool = actual === true || actual === 1 || actual === "true" || actual === "1";
    const expectedLower = String(expected).toLowerCase();

    // Wenn expected ein boolescher Ausdruck ist, boolean vergleichen
    if (["true", "false", "1", "0"].includes(expectedLower) && typeof actual === "boolean") {
        const expectedBool = expectedLower === "true" || expectedLower === "1";
        if (operator === "eq") return actualBool === expectedBool;
        if (operator === "neq") return actualBool !== expectedBool;
        // gt/gte/lt/lte auf booleans ergeben keinen Sinn, als Fallthrough zum String
    }

    // Numerischer Vergleich wenn beide Werte als Zahl interpretierbar sind
    const actualNum = Number(actual);
    const expectedNum = Number(expected);
    if (Number.isFinite(actualNum) && Number.isFinite(expectedNum)) {
        if (operator === "eq") return actualNum === expectedNum;
        if (operator === "neq") return actualNum !== expectedNum;
        if (operator === "gt") return actualNum > expectedNum;
        if (operator === "gte") return actualNum >= expectedNum;
        if (operator === "lt") return actualNum < expectedNum;
        if (operator === "lte") return actualNum <= expectedNum;
    }

    // String-Vergleich als Fallback
    const actualStr = String(actual === null || actual === undefined ? "" : actual);
    if (operator === "eq") return actualStr === expected;
    if (operator === "neq") return actualStr !== expected;
    if (operator === "gt") return actualStr > expected;
    if (operator === "gte") return actualStr >= expected;
    if (operator === "lt") return actualStr < expected;
    if (operator === "lte") return actualStr <= expected;

    return false;
}

// ===========================================================================
// BEDINGUNGSAUSWERTUNG
// ===========================================================================

/**
 * Wertet eine einzelne Bedingung aus.
 *
 * Gibt { met: boolean, reason: string } zurück.
 * reason wird für Debug-Logging genutzt.
 *
 * @param {object} condition - DB-Row aus automation_conditions
 * @param {object} context   - { runtime, now }
 *   runtime: global.get("deviceStore").getRuntime() – Gerätezustände
 *   now:     Date – aktueller Zeitpunkt (für Tests überschreibbar)
 * @returns {{ met: boolean, reason: string }}
 */
function evaluateSingleCondition(condition, context) {
    const now = context.now || new Date();
    const runtime = context.runtime || {};

    const kind = condition.condition_kind;

    // ------------------------------------------------------------------
    // Wochentag-Bedingung
    // ------------------------------------------------------------------
    if (kind === "weekdays") {
        const allowed = parseWeekdays(condition.weekdays);
        const today = currentWeekday(now);
        const met = allowed.has(today);
        return {
            met,
            reason: met
                ? "Wochentag " + today + " ist in [" + condition.weekdays + "] enthalten"
                : "Wochentag " + today + " ist nicht in [" + condition.weekdays + "]"
        };
    }

    // ------------------------------------------------------------------
    // Exakte Uhrzeit-Bedingung
    // ------------------------------------------------------------------
    if (kind === "time_exact") {
        const currentTime = currentHHMM(now);
        const met = currentTime === condition.time_start;
        return {
            met,
            reason: met
                ? "Uhrzeit " + currentTime + " stimmt mit " + condition.time_start + " überein"
                : "Uhrzeit " + currentTime + " stimmt nicht mit " + condition.time_start + " überein"
        };
    }

    // ------------------------------------------------------------------
    // Zeitfenster-Bedingung
    // ------------------------------------------------------------------
    if (kind === "time_window") {
        const currentTime = currentHHMM(now);
        const afterStart = compareHHMM(currentTime, condition.time_start) >= 0;
        const beforeEnd = compareHHMM(currentTime, condition.time_end) <= 0;
        // Mitternacht-übergreifende Fenster (z. B. 23:00–01:00) werden unterstützt
        let met;
        if (condition.time_start <= condition.time_end) {
            met = afterStart && beforeEnd;
        } else {
            // Fenster über Mitternacht: entweder nach Start oder vor Ende
            met = afterStart || beforeEnd;
        }
        return {
            met,
            reason: met
                ? "Uhrzeit " + currentTime + " liegt im Fenster [" + condition.time_start + "-" + condition.time_end + "]"
                : "Uhrzeit " + currentTime + " liegt nicht im Fenster [" + condition.time_start + "-" + condition.time_end + "]"
        };
    }

    // ------------------------------------------------------------------
    // Gerätezustand-Bedingung
    // ------------------------------------------------------------------
    if (kind === "device_state") {
        const deviceId = condition.source_device_id;
        const device = runtime && runtime.devices ? runtime.devices[deviceId] : null;
        if (!device) {
            return { met: false, reason: "Quellgerät " + deviceId + " nicht im Runtime-Store gefunden" };
        }
        const state = device.state || {};
        const fieldName = condition.field_name;
        if (!Object.prototype.hasOwnProperty.call(state, fieldName)) {
            return { met: false, reason: "Feld " + fieldName + " nicht im State von " + deviceId };
        }
        const actual = state[fieldName];
        const met = applyOperator(actual, condition.operator, condition.expected_value);
        return {
            met,
            reason: met
                ? deviceId + "." + fieldName + " " + condition.operator + " " + condition.expected_value + " (Istwert: " + actual + ")"
                : deviceId + "." + fieldName + " = " + actual + " erfüllt " + condition.operator + " " + condition.expected_value + " nicht"
        };
    }

    // Unbekannte condition_kind
    return { met: false, reason: "Unbekannte condition_kind: " + kind };
}

/**
 * Wertet alle Bedingungen einer Automatisierung aus (AND-Verknüpfung).
 *
 * Bricht bei der ersten nicht erfüllten Bedingung ab (Short-Circuit).
 * Gibt ein detailliertes Ergebnis zurück (für Logging und Diagnose).
 *
 * @param {object[]} conditions - DB-Rows aus automation_conditions
 * @param {object}   context    - { runtime, now }
 * @returns {{ allMet: boolean, results: Array<{ condition_id, met, reason }> }}
 */
function evaluateConditions(conditions, context) {
    if (!Array.isArray(conditions) || conditions.length === 0) {
        // Keine Bedingungen → Automatisierung immer auslösen
        return { allMet: true, results: [] };
    }

    const results = [];
    let allMet = true;

    for (const condition of conditions) {
        const result = evaluateSingleCondition(condition, context);
        results.push({
            condition_id: condition.condition_id,
            condition_kind: condition.condition_kind,
            met: result.met,
            reason: result.reason
        });
        if (!result.met) {
            allMet = false;
            // Short-Circuit: Rest nicht prüfen
            break;
        }
    }

    return { allMet, results };
}

// ===========================================================================
// DUPLIKATSSCHUTZ
// ===========================================================================

/**
 * Berechnet den Minutenschlüssel für den Duplikatsschutz.
 *
 * Format: "YYYY-MM-DD HH:MM"
 * Innerhalb derselben Minute wird eine Automatisierung nur einmal ausgelöst.
 *
 * @param {Date} [now]
 * @returns {string}
 */
function currentMinuteKey(now) {
    return currentDateStr(now) + " " + currentHHMM(now);
}

/**
 * Prüft, ob eine Automatisierung in diesem Minutenfenster bereits ausgelöst wurde.
 *
 * lastRunMap: flow-context-Objekt { automation_id → "YYYY-MM-DD HH:MM" }
 *
 * @param {string} automationId
 * @param {object} lastRunMap   - Flow-Kontext-Map (Minutenschlüssel pro ID)
 * @param {Date}   [now]
 * @returns {boolean} true wenn bereits ausgelöst
 */
function alreadyRanThisMinute(automationId, lastRunMap, now) {
    const map = lastRunMap || {};
    const lastKey = map[automationId];
    if (!lastKey) {
        return false;
    }
    return lastKey === currentMinuteKey(now);
}

/**
 * Zentraler Einstiegspunkt des Runners pro Automatisierung:
 * Prüft Duplikatsschutz + alle Bedingungen in einem Aufruf.
 *
 * @param {object}   automation  - DB-Row aus automations
 * @param {object[]} conditions  - DB-Rows aus automation_conditions für diese ID
 * @param {object}   context     - { runtime, now, lastRunMap }
 * @returns {{ trigger: boolean, reason: string, conditionResults: object[] }}
 */
function shouldTrigger(automation, conditions, context) {
    const now = context.now || new Date();
    const lastRunMap = context.lastRunMap || {};

    // Duplikatsschutz: nicht in derselben Minute erneut auslösen
    if (alreadyRanThisMinute(automation.automation_id, lastRunMap, now)) {
        return {
            trigger: false,
            reason: "Bereits in dieser Minute ausgelöst (" + currentMinuteKey(now) + ")",
            conditionResults: []
        };
    }

    // Bedingungen auswerten
    const { allMet, results } = evaluateConditions(conditions, { runtime: context.runtime, now });

    return {
        trigger: allMet,
        reason: allMet
            ? "Alle Bedingungen erfüllt"
            : "Mindestens eine Bedingung nicht erfüllt",
        conditionResults: results
    };
}

// ===========================================================================
// AKTIONS-INPUT-ERZEUGUNG
// ===========================================================================

/**
 * Baut das Eingabe-Payload für commandMinimal aus einem Automatisierungs-Record.
 *
 * action_payload (JSON-String) wird geparst und um device_id ergänzt.
 * Für Cover-Befehle (net_zrl_cover) werden cover_calibrated und is_calibrated
 * aus dem aktuellen Runtime-Gerätezustand übernommen, damit buildCoverCommand
 * die Kalibrierungsprüfung korrekt durchführen kann.
 *
 * Kalibrierungs-Priorisierung:
 *   1. Laufzeitzustand aus smarthome_runtime (aktuell, zuverlässig)
 *   2. Expliziter Wert aus action_payload (nur falls 1 nicht vorhanden)
 *   3. Kein Wert → commandMinimal lehnt set_position ohne Endlage korrekt ab
 *
 * @param {object} automation - DB-Row aus automations
 * @param {object} [runtime]  - smarthome_runtime aus global context
 * @returns {{ ok: true, actionKind, commandInput } | { ok: false, error }}
 */
function buildActionInput(automation, runtime) {
    let payloadObj;
    try {
        payloadObj = typeof automation.action_payload === "string"
            ? JSON.parse(automation.action_payload)
            : automation.action_payload;
    } catch (_err) {
        return { ok: false, error: "action_payload ist kein gültiges JSON" };
    }

    if (!payloadObj || typeof payloadObj !== "object") {
        return { ok: false, error: "action_payload ist kein Objekt" };
    }

    // device_id wird aus der Automatisierung übernommen (Zielgerät)
    const commandInput = Object.assign({}, payloadObj, {
        device_id: automation.target_device_id
    });

    // Kalibrierungsstatus für Cover-Befehle aus dem Laufzeitzustand lesen.
    // buildCoverCommand liest cover_calibrated bereits aus runtime.devices[id].state,
    // aber wir übergeben es zusätzlich explizit im commandInput – so greift die
    // Prüfung auch dann korrekt, wenn das Gerät gerade nicht im Runtime-Store ist.
    if (automation.action_kind === "net_zrl_cover") {
        const deviceState = runtime && runtime.devices
            ? ((runtime.devices[automation.target_device_id] || {}).state || {})
            : {};

        // Runtime-Wert hat Vorrang vor einem ggf. im action_payload gesetzten Wert
        const runtimeCalibrated = deviceState.cover_calibrated !== undefined
            ? deviceState.cover_calibrated
            : deviceState.is_calibrated;

        if (runtimeCalibrated !== undefined) {
            commandInput.cover_calibrated = runtimeCalibrated;
            commandInput.is_calibrated    = runtimeCalibrated;
        }
        // Kein else: fehlt der Wert komplett, lehnt buildCoverCommand
        // set_position-Zwischenwerte korrekt mit 409 not_calibrated ab.
    }

    return {
        ok: true,
        actionKind: automation.action_kind,
        commandInput
    };
}

module.exports = {
    evaluateConditions,
    shouldTrigger,
    buildActionInput,
    currentMinuteKey
};
