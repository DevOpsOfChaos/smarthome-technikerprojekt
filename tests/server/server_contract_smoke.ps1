[CmdletBinding()]
param(
    [switch]$SkipStart,
    [int]$ReadyTimeoutSeconds = 120,
    [int]$IngestTimeoutSeconds = 30,
    [string]$DeviceId = "server_contract_smoke"
)

$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$ComposeFile = Join-Path $RepoRoot "server\docker-compose.yml"
$ExpectedButtonFlags = 5
$RequestId = "smoke-{0}" -f (Get-Date -Format "yyyyMMddHHmmss")

# Der Contract-Smoke-Test prueft den lokalen Compose-Broker.
# Lokale .env-Werte fuer einen Netz-Broker duerfen diesen Test nicht umlenken.
$env:MQTT_HOST = "mosquitto"
$env:MQTT_PORT = "1883"

function Invoke-DockerCompose {
    param(
        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]]$ComposeArgs
    )

    & docker compose -f $ComposeFile @ComposeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "docker compose $($ComposeArgs -join ' ') failed with exit code $LASTEXITCODE"
    }
}

function Get-NodeRedLogs {
    $logs = & docker compose -f $ComposeFile logs --no-color --tail=500 nodered 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "Could not read nodered logs."
    }

    return ($logs -join "`n")
}

function Assert-NoSqlMigrationErrors {
    $logs = Get-NodeRedLogs
    $errorPattern = "(?i)(SQLITE_ERROR|SQLITE_CANTOPEN|SQLITE_CORRUPT|no such table|no such column|migration.*(error|failed|fail)|schema.*(error|failed|fail)|sqlitedb:.*(error|failed|fail)|Error:.*sqlite)"
    if ($logs -match $errorPattern) {
        throw "Node-RED logs contain a SQL or migration error.`nMatched: $($Matches[0])"
    }
}

function Wait-NodeRedReady {
    $deadline = (Get-Date).AddSeconds($ReadyTimeoutSeconds)
    do {
        $logs = Get-NodeRedLogs
        $hasServer = $logs -match "Server now running"
        $hasSqlite = $logs -match "opened /data/sqlite/smarthome_phase1.db ok"
        $hasMqtt = $logs -match "Connected to broker: .*mqtt://mosquitto:1883"

        Assert-NoSqlMigrationErrors

        if ($hasServer -and $hasSqlite -and $hasMqtt) {
            return
        }

        Start-Sleep -Seconds 2
    } while ((Get-Date) -lt $deadline)

    throw "Node-RED was not ready after $ReadyTimeoutSeconds seconds."
}

function Publish-Mqtt {
    param(
        [string]$Topic,
        [string]$Payload,
        [switch]$Retain
    )

    $payloadBase64 = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($Payload))
    $retainFlag = if ($Retain) { "1" } else { "" }
    $publishScript = 'if [ -n "$MQTT_RETAIN" ]; then printf "%s" "$MQTT_PAYLOAD_B64" | base64 -d | mosquitto_pub -h localhost -t "$MQTT_TOPIC" -r -s; else printf "%s" "$MQTT_PAYLOAD_B64" | base64 -d | mosquitto_pub -h localhost -t "$MQTT_TOPIC" -s; fi'
    $publishArgs = @(
        "compose", "-f", $ComposeFile,
        "exec", "-T",
        "-e", "MQTT_TOPIC=$Topic",
        "-e", "MQTT_PAYLOAD_B64=$payloadBase64",
        "-e", "MQTT_RETAIN=$retainFlag",
        "mosquitto",
        "sh", "-lc", $publishScript
    )

    & docker @publishArgs
    if ($LASTEXITCODE -ne 0) {
        throw "docker $($publishArgs -join ' ') failed with exit code $LASTEXITCODE"
    }
}

function Invoke-DbCheck {
    $checkScript = @'
const sqlite3 = require('/data/node_modules/sqlite3');
const [deviceId, requestId, expectedButtonFlagsText] = process.argv.slice(1);
const expectedButtonFlags = Number(expectedButtonFlagsText);
const db = new sqlite3.Database('/data/sqlite/smarthome_phase1.db');

function get(sql, params) {
  return new Promise((resolve, reject) => {
    db.get(sql, params, (error, row) => {
      if (error) {
        reject(error);
        return;
      }
      resolve(row || null);
    });
  });
}

(async () => {
  const device = await get(
    'select device_id, caps from devices where device_id = ?',
    [deviceId]
  );
  const latest = await get(
    'select device_id, button_flags, last_ack_request_id, last_ack_channel, last_ack_status, last_ack_status_code, last_ack_msg_type, last_ack_seq, last_ack_source from device_state_latest where device_id = ?',
    [deviceId]
  );

  const failures = [];
  const caps = device && device.caps ? JSON.parse(device.caps) : [];
  for (const expected of ['switchable', 'lux', 'motion', 'online_state', 'fault_state', 'ack_tracking']) {
    if (!caps.includes(expected)) {
      failures.push(`missing cap ${expected}`);
    }
  }

  if (!latest) {
    failures.push('missing device_state_latest row');
  } else {
    if (latest.button_flags !== expectedButtonFlags) {
      failures.push(`button_flags expected ${expectedButtonFlags}, got ${latest.button_flags}`);
    }
    if (latest.last_ack_request_id !== requestId) {
      failures.push(`last_ack_request_id expected ${requestId}, got ${latest.last_ack_request_id}`);
    }
    if (latest.last_ack_channel !== 'command') {
      failures.push(`last_ack_channel expected command, got ${latest.last_ack_channel}`);
    }
    if (latest.last_ack_status !== 'ok') {
      failures.push(`last_ack_status expected ok, got ${latest.last_ack_status}`);
    }
    if (String(latest.last_ack_status_code) !== '0') {
      failures.push(`last_ack_status_code expected 0, got ${latest.last_ack_status_code}`);
    }
    if (String(latest.last_ack_msg_type) !== '5') {
      failures.push(`last_ack_msg_type expected 5, got ${latest.last_ack_msg_type}`);
    }
    if (String(latest.last_ack_seq) !== '1') {
      failures.push(`last_ack_seq expected 1, got ${latest.last_ack_seq}`);
    }
    if (latest.last_ack_source !== 'node_ack') {
      failures.push(`last_ack_source expected node_ack, got ${latest.last_ack_source}`);
    }
  }

  const result = {
    ok: failures.length === 0,
    device_id: deviceId,
    request_id: requestId,
    caps,
    button_flags: latest ? latest.button_flags : null,
    last_ack_channel: latest ? latest.last_ack_channel : null,
    last_ack_status: latest ? latest.last_ack_status : null,
    last_ack_status_code: latest ? latest.last_ack_status_code : null,
    last_ack_msg_type: latest ? latest.last_ack_msg_type : null,
    last_ack_seq: latest ? latest.last_ack_seq : null,
    last_ack_source: latest ? latest.last_ack_source : null,
    failures
  };

  console.log(JSON.stringify(result, null, 2));
  if (failures.length > 0) {
    process.exit(1);
  }
})().catch((error) => {
  console.error(error && error.stack ? error.stack : String(error));
  process.exit(1);
}).finally(() => db.close());
'@

    $output = & docker compose -f $ComposeFile exec -T nodered node -e $checkScript $DeviceId $RequestId "$ExpectedButtonFlags" 2>&1
    $exitCode = $LASTEXITCODE
    return @{
        ExitCode = $exitCode
        Output = ($output -join "`n")
    }
}

function Wait-DbProjection {
    $deadline = (Get-Date).AddSeconds($IngestTimeoutSeconds)
    $lastCheck = $null

    do {
        $lastCheck = Invoke-DbCheck
        if ($lastCheck.ExitCode -eq 0) {
            Write-Host $lastCheck.Output
            return
        }

        Start-Sleep -Seconds 1
    } while ((Get-Date) -lt $deadline)

    throw "SQLite projection check failed after $IngestTimeoutSeconds seconds.`n$($lastCheck.Output)"
}

Write-Host "Server contract smoke test"
Write-Host "Compose file: $ComposeFile"

if (-not $SkipStart) {
    Invoke-DockerCompose "up" "-d"
}

Wait-NodeRedReady
Assert-NoSqlMigrationErrors

$metaPayload = [ordered]@{
    device_id = $DeviceId
    device_name = "Server Contract Smoke"
    device_class = "net_erl"
    power_type = "mains"
    caps = 81
    fw_version = "smoke"
    meta_schema_version = 1
    control_mode = "relay_light"
    config_profile = "server_contract_smoke"
    reporting_mode = "hybrid"
    sensor_mask = "TPL_______"
    input_mask = "B____"
} | ConvertTo-Json -Compress

$statePayload = [ordered]@{
    device_id = $DeviceId
    relay_1 = $true
    motion = $true
    lux = 120
    button_flags = $ExpectedButtonFlags
    fault = $false
} | ConvertTo-Json -Compress

$ackPayload = [ordered]@{
    device_id = $DeviceId
    request_id = $RequestId
    channel = "command"
    status = "ok"
    status_code = 0
    ack_msg_type = 5
    ack_seq = 1
    source = "node_ack"
} | ConvertTo-Json -Compress

Publish-Mqtt -Topic "smarthome/device/$DeviceId/meta" -Payload $metaPayload -Retain
Publish-Mqtt -Topic "smarthome/device/$DeviceId/state" -Payload $statePayload -Retain
Publish-Mqtt -Topic "smarthome/device/$DeviceId/ack" -Payload $ackPayload

Wait-DbProjection
Assert-NoSqlMigrationErrors

Write-Host "Smoke test passed: numeric caps, button_flags, ACK snapshot fields and SQL/migration startup gate are ok."

# =============================================================================
# Erweiterung: Automatisierungs-Schema-Prüfung
# =============================================================================
# Prüft, ob die Tabellen automations und automation_conditions nach dem Start
# korrekt angelegt wurden und FOREIGN KEY-Constraints aktiv sind.
# Keine Daten werden geschrieben – nur Schema-Prüfung (idempotent).
# =============================================================================

function Test-AutomationsSchema {
    $checkScript = @'
const sqlite3 = require('/data/node_modules/sqlite3');
const db = new sqlite3.Database('/data/sqlite/smarthome_phase1.db');

function all(sql) {
  return new Promise((resolve, reject) => {
    db.all(sql, [], (error, rows) => {
      if (error) { reject(error); return; }
      resolve(rows || []);
    });
  });
}

(async () => {
  const failures = [];

  // Tabelle automations prüfen
  const autoInfo = await all("PRAGMA table_info(automations)");
  const autoColNames = autoInfo.map(c => c.name);
  for (const col of ['automation_id','name','enabled','target_device_id','action_kind','action_payload','created_at','updated_at','last_run_at','last_result']) {
    if (!autoColNames.includes(col)) {
      failures.push('automations: Spalte fehlt: ' + col);
    }
  }

  // Tabelle automation_conditions prüfen
  const condInfo = await all("PRAGMA table_info(automation_conditions)");
  const condColNames = condInfo.map(c => c.name);
  for (const col of ['condition_id','automation_id','condition_scope','condition_kind','source_device_id','field_name','operator','expected_value','weekdays','time_start','time_end','created_at','updated_at']) {
    if (!condColNames.includes(col)) {
      failures.push('automation_conditions: Spalte fehlt: ' + col);
    }
  }

  // FOREIGN KEY auf automation_id prüfen
  const fkInfo = await all("PRAGMA foreign_key_list(automation_conditions)");
  const hasFk = fkInfo.some(fk => fk.table === 'automations' && fk.from === 'automation_id');
  if (!hasFk) {
    failures.push('automation_conditions: FOREIGN KEY auf automations fehlt');
  }

  console.log(JSON.stringify({ ok: failures.length === 0, failures }, null, 2));
  if (failures.length > 0) { process.exit(1); }
})().catch(e => {
  console.error(String(e));
  process.exit(1);
}).finally(() => db.close());
'@

    # Das mehrzeilige Node-Skript wird per stdin übergeben. `node -e` ist unter
    # PowerShell mit längeren Skripten und Sonderzeichen unnötig brüchig.
    $output = $checkScript | & docker compose -f $ComposeFile exec -T nodered node - 2>&1
    $exitCode = $LASTEXITCODE
    return @{
        ExitCode = $exitCode
        Output   = ($output -join "`n")
    }
}

Write-Host "`nPrüfe Automatisierungs-Schema..."
$schemaCheck = Test-AutomationsSchema
Write-Host $schemaCheck.Output
if ($schemaCheck.ExitCode -ne 0) {
    throw "Automatisierungs-Schema-Prüfung fehlgeschlagen.`n$($schemaCheck.Output)"
}
Write-Host "Schema-Prüfung bestanden: automations und automation_conditions korrekt angelegt."
