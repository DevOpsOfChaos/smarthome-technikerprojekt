param(
    [string[]]$Device,
    [switch]$Compile,
    [switch]$Clean,
    [switch]$CleanPlatformioCache,
    [switch]$VerboseOutput,
    [string]$UvPath
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$esphomeRoot = Join-Path $repoRoot "esphome"
$deviceRoot = Join-Path $esphomeRoot "devices"
$repoDriveRoot = [System.IO.Path]::GetPathRoot($repoRoot.Path)
$cacheRoot = Join-Path $repoDriveRoot ".sh-esphome"
$platformioCoreDir = Join-Path $cacheRoot "esphome-platformio"
$uvCacheDir = Join-Path $cacheRoot "uv-cache"
$workRoot = Join-Path $cacheRoot "esphome-work"

function Find-Uv {
    param([string]$ExplicitPath)

    if ($ExplicitPath) {
        if (!(Test-Path $ExplicitPath)) {
            throw "uv wurde unter '$ExplicitPath' nicht gefunden."
        }
        return (Resolve-Path $ExplicitPath).Path
    }

    if ($env:UV_EXE -and (Test-Path $env:UV_EXE)) {
        return (Resolve-Path $env:UV_EXE).Path
    }

    $candidates = @(
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python312\Scripts\uv.exe"),
        (Join-Path $env:LOCALAPPDATA "Programs\Python\Python314\Scripts\uv.exe")
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    $cmd = Get-Command uv -ErrorAction SilentlyContinue
    if ($cmd -and $cmd.Source -notmatch "\\\.platformio\\penv\\") {
        return $cmd.Source
    }

    throw "uv wurde nicht gefunden. Installiere uv oder setze UV_EXE auf den vollstaendigen Pfad zu uv.exe."
}

function Resolve-DevicePath {
    param([string]$Value)

    $candidate = $Value
    if (!(Test-Path $candidate)) {
        $candidate = Join-Path $deviceRoot $Value
    }
    if (!(Test-Path $candidate) -and $Value -notlike "*.yaml") {
        $candidate = Join-Path $deviceRoot "$Value.yaml"
    }
    if (!(Test-Path $candidate)) {
        throw "ESPHome-Geraet '$Value' wurde nicht gefunden."
    }
    return (Resolve-Path $candidate).Path
}

$uv = Find-Uv -ExplicitPath $UvPath

if (!(Test-Path $deviceRoot)) {
    throw "ESPHome-Geraeteordner fehlt: $deviceRoot"
}

if (!(Test-Path $cacheRoot)) {
    New-Item -ItemType Directory -Path $cacheRoot | Out-Null
}
if (!(Test-Path $platformioCoreDir)) {
    New-Item -ItemType Directory -Path $platformioCoreDir | Out-Null
}
if (!(Test-Path $uvCacheDir)) {
    New-Item -ItemType Directory -Path $uvCacheDir | Out-Null
}

if (!$Device -or $Device.Count -eq 0) {
    $devicePaths = Get-ChildItem -Path $deviceRoot -Filter "*.yaml" | Sort-Object Name | ForEach-Object { $_.FullName }
} else {
    $devicePaths = $Device | ForEach-Object { Resolve-DevicePath $_ }
}

if ($Clean -and (Test-Path $workRoot)) {
    Remove-Item -LiteralPath $workRoot -Recurse -Force
}
if ($CleanPlatformioCache -and (Test-Path $platformioCoreDir)) {
    Remove-Item -LiteralPath $platformioCoreDir -Recurse -Force
    New-Item -ItemType Directory -Path $platformioCoreDir | Out-Null
}
if (!(Test-Path $workRoot)) {
    New-Item -ItemType Directory -Path $workRoot | Out-Null
}

try {
    $workEsphomeRoot = Join-Path $workRoot "esphome"
    $workDeviceRoot = Join-Path $workEsphomeRoot "devices"

    if (!(Test-Path $workEsphomeRoot)) {
        New-Item -ItemType Directory -Path $workEsphomeRoot | Out-Null
    }
    if (!(Test-Path $workDeviceRoot)) {
        New-Item -ItemType Directory -Path $workDeviceRoot | Out-Null
    }

    Copy-Item -Path (Join-Path $esphomeRoot "packages") -Destination $workEsphomeRoot -Recurse -Force
    Copy-Item -Path (Join-Path $deviceRoot "*.yaml") -Destination $workDeviceRoot -Force

    $dummySecrets = @"
wifi_ssid: dummy-wifi
wifi_password: dummy-password
mqtt_broker: 127.0.0.1
mqtt_username: dummy-user
mqtt_password: dummy-password
ota_password: dummy-ota-password
"@

    $dummySecrets | Set-Content -Path (Join-Path $workEsphomeRoot "secrets.yaml") -Encoding UTF8
    $dummySecrets | Set-Content -Path (Join-Path $workDeviceRoot "secrets.yaml") -Encoding UTF8

    $oldPlatformioCoreDir = $env:PLATFORMIO_CORE_DIR
    $oldPythonNoUserSite = $env:PYTHONNOUSERSITE
    $oldPlatformioTelemetry = $env:PLATFORMIO_SETTING_ENABLE_TELEMETRY
    $oldUvCacheDir = $env:UV_CACHE_DIR

    $env:PLATFORMIO_CORE_DIR = $platformioCoreDir
    $env:PYTHONNOUSERSITE = "1"
    $env:PLATFORMIO_SETTING_ENABLE_TELEMETRY = "No"
    $env:UV_CACHE_DIR = $uvCacheDir

    foreach ($devicePath in $devicePaths) {
        $relative = [System.IO.Path]::GetRelativePath($esphomeRoot, $devicePath)
        $workDevicePath = Join-Path $workEsphomeRoot $relative
        $mode = if ($Compile) { "compile" } else { "config" }
        Write-Host "ESPHome $mode $relative"

        $logPath = Join-Path $workRoot ("last-" + ($relative -replace '[\\/:*?"<>|]', '_') + "-$mode.log")
        & $uv tool run --from esphome esphome $mode $workDevicePath *> $logPath
        if ($LASTEXITCODE -ne 0) {
            Get-Content $logPath | Select-Object -Last 120
            throw "ESPHome $mode ist fuer '$relative' fehlgeschlagen."
        }

        if ($VerboseOutput) {
            Get-Content $logPath
        } else {
            $summary = Get-Content $logPath | Where-Object {
                $_ -match "^(INFO|WARNING|ERROR)\s" -or $_ -match "\[(SUCCESS|FAILED)\]"
            } | Select-Object -Last 25
            if ($summary) {
                $summary
            } else {
                Get-Content $logPath | Select-Object -Last 10
            }
        }
    }
}
finally {
    $env:PLATFORMIO_CORE_DIR = $oldPlatformioCoreDir
    $env:PYTHONNOUSERSITE = $oldPythonNoUserSite
    $env:PLATFORMIO_SETTING_ENABLE_TELEMETRY = $oldPlatformioTelemetry
    $env:UV_CACHE_DIR = $oldUvCacheDir

    Write-Host "Arbeitskopie: $workRoot"
}
