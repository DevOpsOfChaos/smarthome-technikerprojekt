param(
    [string]$SshHost = "ha",
    [string]$RemoteDbPath = "/addon_configs/a0d7b954_nodered/sqlite/smarthome_phase1.db",
    [int]$StaleAfterMinutes = 15
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$deviceRoot = Join-Path $repoRoot "esphome\devices"

if (!(Test-Path $deviceRoot)) {
    throw "ESPHome-Geraeteordner fehlt: $deviceRoot"
}

function Convert-JsonArrayText {
    param([string]$Value)

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return @()
    }

    try {
        $parsed = $Value | ConvertFrom-Json
        if ($null -eq $parsed) {
            return @()
        }
        return @($parsed)
    } catch {
        return @()
    }
}

function Get-ExpectedDevice {
    param([string]$Path)

    $result = [ordered]@{
        file = Split-Path -Leaf $Path
        device_id = $null
        device_name = $null
        device_class = $null
    }

    foreach ($line in Get-Content -LiteralPath $Path) {
        if ($line -match "^\s*device_id:\s*(.+?)\s*$") {
            $result.device_id = $Matches[1].Trim().Trim('"', "'")
        } elseif ($line -match "^\s*device_name:\s*(.+?)\s*$") {
            $result.device_name = $Matches[1].Trim().Trim('"', "'")
        } elseif ($line -match "^\s*device_class:\s*(.+?)\s*$") {
            $result.device_class = $Matches[1].Trim().Trim('"', "'")
        }
    }

    if (!$result.device_id) {
        return $null
    }

    [pscustomobject]$result
}

function Invoke-PythonJson {
    param(
        [string]$DatabasePath
    )

    $pythonCode = @'
import json
import sqlite3
import sys

db_path = sys.argv[1]
con = sqlite3.connect(db_path)
con.row_factory = sqlite3.Row
rows = con.execute("""
SELECT
  d.device_id,
  d.device_name,
  d.device_class,
  d.caps,
  d.fw_version,
  d.updated_at AS meta_updated_at,
  s.availability,
  s.online,
  s.last_seen_at,
  s.updated_at AS state_updated_at,
  s.last_ack_status,
  s.last_ack_status_code,
  s.last_ack_at
FROM devices d
LEFT JOIN device_state_latest s USING(device_id)
ORDER BY d.device_id
""").fetchall()
print(json.dumps([dict(row) for row in rows], ensure_ascii=False))
'@

    $output = $pythonCode | python - $DatabasePath
    if ($LASTEXITCODE -ne 0) {
        throw "SQLite-Auswertung per Python ist fehlgeschlagen."
    }
    if ([string]::IsNullOrWhiteSpace($output)) {
        return @()
    }
    return @($output | ConvertFrom-Json)
}

$expectedDevices = Get-ChildItem -LiteralPath $deviceRoot -Filter "*.yaml" |
    Sort-Object Name |
    ForEach-Object { Get-ExpectedDevice -Path $_.FullName } |
    Where-Object { $null -ne $_ }

if (!$expectedDevices) {
    throw "Keine erwarteten ESPHome-Geraete in $deviceRoot gefunden."
}

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("smarthome-device-contract-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $tempRoot | Out-Null
$localDb = Join-Path $tempRoot "smarthome_phase1.db"

try {
    & scp "${SshHost}:$RemoteDbPath" $localDb | Out-Null
    if ($LASTEXITCODE -ne 0 -or !(Test-Path $localDb)) {
        throw "Konnte SQLite-DB nicht von ${SshHost}:$RemoteDbPath kopieren."
    }

    $actualRows = Invoke-PythonJson -DatabasePath $localDb
    $actualById = @{}
    foreach ($row in $actualRows) {
        $actualById[$row.device_id] = $row
    }

    $now = Get-Date
    $reportRows = @()
    $issueRows = @()

    foreach ($expected in $expectedDevices) {
        $actual = $actualById[$expected.device_id]
        $issues = New-Object System.Collections.Generic.List[string]

        if (!$actual) {
            $issues.Add("missing_in_server")
            $issueRows += [pscustomobject]@{
                device_id = $expected.device_id
                issues = ($issues -join ",")
            }
            $reportRows += [pscustomobject]@{
                device_id = $expected.device_id
                expected_class = $expected.device_class
                server_class = ""
                availability = ""
                last_seen_at = ""
                issues = ($issues -join ",")
            }
            continue
        }

        $caps = Convert-JsonArrayText -Value $actual.caps
        if ([string]::IsNullOrWhiteSpace($actual.device_class) -or $caps.Count -eq 0) {
            $issues.Add("meta_incomplete")
        }
        if ($actual.device_class -and $expected.device_class -and $actual.device_class -ne $expected.device_class) {
            $issues.Add("class_mismatch")
        }
        $onlineValue = 0
        if ($null -ne $actual.online) {
            $onlineValue = [int]$actual.online
        }
        if ($actual.availability -ne "online" -or $onlineValue -ne 1) {
            $issues.Add("offline")
        }
        if ($actual.last_seen_at) {
            try {
                $lastSeen = [datetime]::Parse($actual.last_seen_at, [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::AssumeUniversal)
                if (($now.ToUniversalTime() - $lastSeen.ToUniversalTime()).TotalMinutes -gt $StaleAfterMinutes) {
                    $issues.Add("stale")
                }
            } catch {
                $issues.Add("invalid_last_seen")
            }
        } else {
            $issues.Add("missing_last_seen")
        }

        if ($issues.Count -gt 0) {
            $issueRows += [pscustomobject]@{
                device_id = $expected.device_id
                issues = ($issues -join ",")
            }
        }

        $reportRows += [pscustomobject]@{
            device_id = $expected.device_id
            expected_class = $expected.device_class
            server_class = $actual.device_class
            availability = $actual.availability
            last_seen_at = $actual.last_seen_at
            issues = ($issues -join ",")
        }
    }

    foreach ($actual in $actualRows) {
        if (!($expectedDevices | Where-Object { $_.device_id -eq $actual.device_id })) {
            $issueRows += [pscustomobject]@{
                device_id = $actual.device_id
                issues = "unexpected_in_server"
            }
            $reportRows += [pscustomobject]@{
                device_id = $actual.device_id
                expected_class = ""
                server_class = $actual.device_class
                availability = $actual.availability
                last_seen_at = $actual.last_seen_at
                issues = "unexpected_in_server"
            }
        }
    }

    $reportRows | Sort-Object device_id | Format-Table -AutoSize

    if ($issueRows.Count -gt 0) {
        Write-Host ""
        Write-Host "Probleme:"
        $issueRows | Sort-Object device_id | Format-Table -AutoSize
        exit 2
    }

    Write-Host "OK: Alle erwarteten ESPHome-Geraete sind im Serververtrag sauber sichtbar."
}
finally {
    if (Test-Path $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
