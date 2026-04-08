"use strict";

const fs = require("fs");

const STORAGE_PATH = process.env.COVER_AUTOMATION_PATH || "/data/cover_automation.json";
const TIME_ZONE = process.env.TZ || "Europe/Berlin";
const ALLOWED_POSITIONS = [0, 25, 50, 75, 100];
const SLOT_KEYS = ["slot_1", "slot_2"];

function htmlEscape(value) {
  return String(value ?? "")
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/\"/g, "&quot;")
    .replace(/'/g, "&#39;");
}

function isPlainObject(value) {
  return Boolean(value) && typeof value === "object" && !Array.isArray(value);
}

function normalizeInput(input) {
  if (isPlainObject(input)) {
    return input;
  }

  if (typeof input === "string") {
    try {
      const parsed = JSON.parse(input);
      return isPlainObject(parsed) ? parsed : {};
    } catch (error) {
      return {};
    }
  }

  return {};
}

function normalizeDeviceId(value) {
  return typeof value === "string" ? value.trim() : "";
}

function normalizeTime(value) {
  const text = typeof value === "string" ? value.trim() : "";
  return /^\d{2}:\d{2}$/.test(text) ? text : "";
}

function normalizePosition(value, fallback = 100) {
  const numeric = Number(value);
  if (ALLOWED_POSITIONS.includes(numeric)) {
    return numeric;
  }
  return fallback;
}

function normalizeEnabled(value) {
  if (typeof value === "boolean") {
    return value;
  }

  const normalized = String(value ?? "").trim().toLowerCase();
  return ["1", "true", "on", "yes"].includes(normalized);
}

function createEmptyStore() {
  return { devices: {} };
}

function normalizeSlot(input, fallbackPosition) {
  const slot = isPlainObject(input) ? input : {};
  return {
    time: normalizeTime(slot.time),
    position: normalizePosition(slot.position, fallbackPosition),
    last_run_local_date: typeof slot.last_run_local_date === "string" ? slot.last_run_local_date : ""
  };
}

function normalizeConfig(input) {
  const config = isPlainObject(input) ? input : {};
  return {
    enabled: normalizeEnabled(config.enabled),
    slot_1: normalizeSlot(config.slot_1, 100),
    slot_2: normalizeSlot(config.slot_2, 0),
    updated_at: typeof config.updated_at === "string" ? config.updated_at : null
  };
}

function loadStore() {
  try {
    if (!fs.existsSync(STORAGE_PATH)) {
      return createEmptyStore();
    }

    const raw = fs.readFileSync(STORAGE_PATH, "utf8");
    const parsed = JSON.parse(raw);
    if (!isPlainObject(parsed) || !isPlainObject(parsed.devices)) {
      return createEmptyStore();
    }

    const devices = {};
    Object.entries(parsed.devices).forEach(([deviceId, config]) => {
      const normalizedId = normalizeDeviceId(deviceId);
      if (normalizedId) {
        devices[normalizedId] = normalizeConfig(config);
      }
    });

    return { devices };
  } catch (error) {
    return createEmptyStore();
  }
}

function saveStore(store) {
  fs.writeFileSync(STORAGE_PATH, JSON.stringify(store, null, 2));
}

function getDeviceConfig(deviceId) {
  const normalizedId = normalizeDeviceId(deviceId);
  const store = loadStore();
  return normalizeConfig(store.devices[normalizedId] || {});
}

function saveDeviceConfig(deviceId, input, updatedAt) {
  const normalizedId = normalizeDeviceId(deviceId);
  if (!normalizedId) {
    return { ok: false, error: "device_id_required", message: "device_id is required" };
  }

  const payload = normalizeInput(input);
  const config = {
    enabled: normalizeEnabled(payload.enabled),
    slot_1: {
      time: normalizeTime(payload.slot_1_time),
      position: normalizePosition(payload.slot_1_position, 100),
      last_run_local_date: ""
    },
    slot_2: {
      time: normalizeTime(payload.slot_2_time),
      position: normalizePosition(payload.slot_2_position, 0),
      last_run_local_date: ""
    },
    updated_at: updatedAt
  };

  const store = loadStore();
  const existing = normalizeConfig(store.devices[normalizedId] || {});
  config.slot_1.last_run_local_date = existing.slot_1.time === config.slot_1.time && existing.slot_1.position === config.slot_1.position
    ? existing.slot_1.last_run_local_date
    : "";
  config.slot_2.last_run_local_date = existing.slot_2.time === config.slot_2.time && existing.slot_2.position === config.slot_2.position
    ? existing.slot_2.last_run_local_date
    : "";

  store.devices[normalizedId] = config;
  saveStore(store);

  return { ok: true, config };
}

function formatLocalParts(timestamp) {
  const formatter = new Intl.DateTimeFormat("sv-SE", {
    timeZone: TIME_ZONE,
    year: "numeric",
    month: "2-digit",
    day: "2-digit",
    hour: "2-digit",
    minute: "2-digit",
    hour12: false
  });
  const parts = formatter.formatToParts(new Date(timestamp));
  const map = {};
  parts.forEach((part) => {
    map[part.type] = part.value;
  });
  return {
    date: `${map.year}-${map.month}-${map.day}`,
    time: `${map.hour}:${map.minute}`
  };
}

function isCoverDevice(runtime, deviceId) {
  const device = runtime && runtime.devices ? runtime.devices[deviceId] : null;
  if (!device || !isPlainObject(device)) {
    return false;
  }

  const meta = isPlainObject(device.meta) ? device.meta : {};
  const caps = Array.isArray(meta.caps) ? meta.caps.map((cap) => String(cap).toLowerCase()) : [];
  return String(meta.control_mode || "").toLowerCase() === "cover"
    || /^net[-_]?zrl$/i.test(String(meta.device_class || ""))
    || caps.includes("cover");
}

function buildOptionMarkup(selectedValue) {
  return ALLOWED_POSITIONS.map((value) => {
    const selected = Number(selectedValue) === value ? " selected" : "";
    return `<option value="${value}"${selected}>${value}%</option>`;
  }).join("");
}

function describeDevice(device, deviceId) {
  const meta = isPlainObject(device && device.meta) ? device.meta : {};
  const state = isPlainObject(device && device.state) ? device.state : {};
  const availability = isPlainObject(device && device.availability) ? device.availability : {};

  return {
    deviceId,
    name: meta.device_name || deviceId,
    deviceClass: meta.device_class || "unbekannt",
    online: availability.online === true,
    coverState: state.cover_state || "unbekannt",
    coverPosition: Number.isFinite(Number(state.cover_position)) ? Number(state.cover_position) : null,
    calibrated: state.cover_calibrated === true || state.is_calibrated === true
  };
}

function renderDetailPage(runtime, deviceId, notice = "") {
  const normalizedId = normalizeDeviceId(deviceId);
  const device = runtime && runtime.devices ? runtime.devices[normalizedId] : null;
  if (!isCoverDevice(runtime, normalizedId)) {
    return {
      statusCode: 404,
      html: `<!doctype html><html lang="de"><meta charset="utf-8"><title>Gerät nicht gefunden</title><body><h1>Gerät nicht gefunden</h1><p>Für <code>${htmlEscape(normalizedId)}</code> gibt es hier keine Cover-Detailseite.</p></body></html>`
    };
  }

  const summary = describeDevice(device, normalizedId);
  const config = getDeviceConfig(normalizedId);
  const noticeHtml = notice
    ? `<p class="notice">${htmlEscape(notice)}</p>`
    : "";
  const positionText = summary.coverPosition === null ? "unbekannt" : `${summary.coverPosition}%`;
  const checked = config.enabled ? " checked" : "";

  const html = `<!doctype html>
<html lang="de">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>${htmlEscape(summary.name)} – Detail</title>
  <style>
    :root {
      color-scheme: light;
      --bg: #f3efe6;
      --panel: #fffaf0;
      --line: #d8cdb9;
      --ink: #1e1f1a;
      --muted: #6f6a5f;
      --accent: #9e5d2c;
      --accent-strong: #7d4319;
      --ok: #2f6f44;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: "Segoe UI", "Trebuchet MS", sans-serif;
      background:
        radial-gradient(circle at top right, rgba(158,93,44,.18), transparent 30%),
        linear-gradient(180deg, #f8f4eb 0%, var(--bg) 100%);
      color: var(--ink);
    }
    main {
      max-width: 820px;
      margin: 0 auto;
      padding: 32px 18px 48px;
    }
    .hero, .panel {
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 18px;
      box-shadow: 0 18px 40px rgba(31, 24, 16, 0.08);
    }
    .hero {
      padding: 24px;
      margin-bottom: 18px;
    }
    h1 { margin: 0 0 6px; font-size: 2rem; }
    .meta {
      margin: 0;
      color: var(--muted);
      font-size: .95rem;
    }
    .status-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
      gap: 12px;
      margin-top: 18px;
    }
    .stat {
      background: rgba(255,255,255,.75);
      border: 1px solid var(--line);
      border-radius: 14px;
      padding: 12px;
    }
    .stat strong {
      display: block;
      font-size: .8rem;
      color: var(--muted);
      text-transform: uppercase;
      letter-spacing: .06em;
      margin-bottom: 6px;
    }
    .stat span {
      font-size: 1.1rem;
      font-weight: 600;
    }
    .panel {
      padding: 24px;
    }
    .notice {
      margin: 0 0 16px;
      padding: 12px 14px;
      border-radius: 12px;
      background: rgba(47,111,68,.1);
      color: var(--ok);
      border: 1px solid rgba(47,111,68,.25);
      font-weight: 600;
    }
    .lead {
      margin: 0 0 20px;
      color: var(--muted);
    }
    .toggle {
      display: flex;
      align-items: center;
      gap: 10px;
      margin-bottom: 20px;
      font-weight: 600;
    }
    .slot-grid {
      display: grid;
      gap: 16px;
    }
    .slot {
      border: 1px solid var(--line);
      border-radius: 16px;
      padding: 16px;
      background: rgba(255,255,255,.72);
    }
    .slot h2 {
      margin: 0 0 12px;
      font-size: 1.05rem;
    }
    .fields {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
      gap: 12px;
    }
    label {
      display: grid;
      gap: 6px;
      font-size: .92rem;
      color: var(--muted);
    }
    input, select, button {
      font: inherit;
    }
    input[type="time"], select {
      width: 100%;
      padding: 10px 12px;
      border-radius: 10px;
      border: 1px solid var(--line);
      background: #fff;
      color: var(--ink);
    }
    button {
      margin-top: 18px;
      border: 0;
      border-radius: 999px;
      padding: 12px 18px;
      background: var(--accent);
      color: #fff;
      font-weight: 700;
      cursor: pointer;
    }
    button:hover {
      background: var(--accent-strong);
    }
    .hint {
      margin-top: 16px;
      color: var(--muted);
      font-size: .88rem;
    }
  </style>
</head>
<body>
  <main>
    <section class="hero">
      <h1>${htmlEscape(summary.name)}</h1>
      <p class="meta">Gerät <code>${htmlEscape(summary.deviceId)}</code> · Klasse <code>${htmlEscape(summary.deviceClass)}</code></p>
      <div class="status-grid">
        <div class="stat"><strong>Online</strong><span>${summary.online ? "ja" : "nein"}</span></div>
        <div class="stat"><strong>Cover-State</strong><span>${htmlEscape(summary.coverState)}</span></div>
        <div class="stat"><strong>Position</strong><span>${htmlEscape(positionText)}</span></div>
        <div class="stat"><strong>Kalibriert</strong><span>${summary.calibrated ? "ja" : "nein"}</span></div>
      </div>
    </section>

    <section class="panel">
      ${noticeHtml}
      <p class="lead">Zwei feste Fahrten pro Tag. Jede Fahrt setzt nur eine offizielle Zielposition über den bestehenden Cover-Pfad. Mehr nicht.</p>
      <form id="automation-form">
        <label class="toggle">
          <input type="checkbox" id="enabled"${checked}>
          Zeitautomatik für dieses Gerät aktiv
        </label>

        <div class="slot-grid">
          <section class="slot">
            <h2>Fahrt 1</h2>
            <div class="fields">
              <label>
                Uhrzeit
                <input type="time" id="slot_1_time" value="${htmlEscape(config.slot_1.time)}">
              </label>
              <label>
                Zielwert
                <select id="slot_1_position">${buildOptionMarkup(config.slot_1.position)}</select>
              </label>
            </div>
          </section>

          <section class="slot">
            <h2>Fahrt 2</h2>
            <div class="fields">
              <label>
                Uhrzeit
                <input type="time" id="slot_2_time" value="${htmlEscape(config.slot_2.time)}">
              </label>
              <label>
                Zielwert
                <select id="slot_2_position">${buildOptionMarkup(config.slot_2.position)}</select>
              </label>
            </div>
          </section>
        </div>

        <button type="submit">Speichern</button>
      </form>
      <p class="hint">Erlaubte Zielwerte sind bewusst hart begrenzt auf 0, 25, 50, 75 oder 100 Prozent. Der neutrale Serververtrag bleibt davon unberührt.</p>
    </section>
  </main>

  <script>
    const deviceId = ${JSON.stringify(summary.deviceId)};
    const form = document.getElementById("automation-form");
    form.addEventListener("submit", async (event) => {
      event.preventDefault();
      const payload = {
        enabled: document.getElementById("enabled").checked,
        slot_1_time: document.getElementById("slot_1_time").value,
        slot_1_position: Number(document.getElementById("slot_1_position").value),
        slot_2_time: document.getElementById("slot_2_time").value,
        slot_2_position: Number(document.getElementById("slot_2_position").value)
      };

      const response = await fetch("/api/phase1/cover/automation/" + encodeURIComponent(deviceId), {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload)
      });
      const result = await response.json();
      if (!response.ok || !result.ok) {
        alert(result.message || "Speichern fehlgeschlagen");
        return;
      }
      window.location.href = "/device/" + encodeURIComponent(deviceId) + "?saved=1";
    });
  </script>
</body>
</html>`;

  return {
    statusCode: 200,
    html
  };
}

function buildDueMessages(runtime, commandMinimal, timestamp) {
  const store = loadStore();
  const messages = [];
  const errors = [];
  const nowLocal = formatLocalParts(timestamp);
  let changed = false;

  Object.entries(store.devices).forEach(([deviceId, rawConfig]) => {
    if (!isCoverDevice(runtime, deviceId)) {
      return;
    }

    const config = normalizeConfig(rawConfig);
    if (!config.enabled) {
      return;
    }

    SLOT_KEYS.forEach((slotKey) => {
      const slot = config[slotKey];
      if (!slot.time || slot.time !== nowLocal.time || slot.last_run_local_date === nowLocal.date) {
        return;
      }

      const result = commandMinimal.buildCoverCommand(runtime, {
        device_id: deviceId,
        command: "set_position",
        position: slot.position
      }, timestamp);

      slot.last_run_local_date = nowLocal.date;
      changed = true;

      if (!result.ok) {
        errors.push({
          device_id: deviceId,
          slot: slotKey,
          error: result.error,
          message: result.message
        });
        return;
      }

      messages.push({
        topic: result.command.topic,
        payload: result.command.payload,
        qos: 0,
        retain: false
      });
    });

    store.devices[deviceId] = config;
  });

  if (changed) {
    saveStore(store);
  }

  return { messages, errors };
}

module.exports = {
  getDeviceConfig,
  renderDetailPage,
  saveDeviceConfig,
  buildDueMessages
};
