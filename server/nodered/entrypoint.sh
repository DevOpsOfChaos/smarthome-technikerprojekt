#!/bin/sh
set -eu

SQLITE_TARGET="${SQLITE_PATH:-/data/sqlite/smarthome.sqlite}"
SQLITE_DIR="$(dirname "$SQLITE_TARGET")"
DESIRED_PACKAGE_JSON="/opt/smarthome/package.json"
DESIRED_SETTINGS_JS="/opt/smarthome/settings.js"

ensure_column() {
  table_name="$1"
  column_name="$2"
  column_def="$3"
  if ! sqlite3 "$SQLITE_TARGET" "PRAGMA table_info(${table_name});" | cut -d'|' -f2 | grep -Fxq "$column_name"; then
    sqlite3 "$SQLITE_TARGET" "ALTER TABLE ${table_name} ADD COLUMN ${column_name} ${column_def};"
  fi
}

mkdir -p "$SQLITE_DIR"
sqlite3 "$SQLITE_TARGET" < /opt/smarthome/schema.sql

ensure_column devices device_role "TEXT NOT NULL DEFAULT 'node'"
ensure_column devices default_name "TEXT NOT NULL DEFAULT ''"
ensure_column devices base_type "TEXT"
ensure_column devices profile "TEXT"
ensure_column devices identity_json "TEXT NOT NULL DEFAULT '{}'"

if [ ! -f /data/package.json ] || ! cmp -s "$DESIRED_PACKAGE_JSON" /data/package.json; then
  cp "$DESIRED_PACKAGE_JSON" /data/package.json
  cd /data
  npm install --unsafe-perm --no-update-notifier --no-fund --omit=dev
fi

if [ ! -f /data/settings.js ] || ! cmp -s "$DESIRED_SETTINGS_JS" /data/settings.js; then
  cp "$DESIRED_SETTINGS_JS" /data/settings.js
fi

node /opt/smarthome/build-flows.js > /data/flows.json

if [ -n "${MQTT_USERNAME:-}" ] && [ -n "${MQTT_PASSWORD:-}" ]; then
  node <<'EOF'
const crypto = require("crypto");
const fs = require("fs");

const credentialPath = "/data/flows_cred.json";
const credentials = {
  cfg_mqtt_broker: {
    user: process.env.MQTT_USERNAME,
    password: process.env.MQTT_PASSWORD
  }
};

if (process.env.NODERED_CREDENTIAL_SECRET) {
  const key = crypto.createHash("sha256").update(process.env.NODERED_CREDENTIAL_SECRET).digest();
  const initVector = crypto.randomBytes(16);
  const cipher = crypto.createCipheriv("aes-256-ctr", key, initVector);
  const payload =
    initVector.toString("hex") +
    cipher.update(JSON.stringify(credentials), "utf8", "base64") +
    cipher.final("base64");

  fs.writeFileSync(credentialPath, JSON.stringify({ $: payload }));
} else {
  fs.writeFileSync(credentialPath, JSON.stringify(credentials));
}
EOF
else
  rm -f /data/flows_cred.json
fi

exec /usr/src/node-red/entrypoint.sh "$@"
