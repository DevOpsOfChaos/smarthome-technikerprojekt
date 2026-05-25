# Device-Types & Capabilities

**Stand:** Firmware v1 · **Basis:** `DeviceTypes.h`

---

## Inhaltsverzeichnis

1. [Device-Classes](#1-device-classes)
2. [Power-Types](#2-power-types)
3. [Control-Modes](#3-control-modes)
4. [Config-Profiles](#4-config-profiles)
5. [Reporting-Modes](#5-reporting-modes)
6. [Capabilities (Bitmask)](#6-capabilities-bitmask)
7. [Meta-Schema-Version](#7-meta-schema-version)
8. [Gerätematrix](#8-gerätematrix)
9. [ID-Format](#9-id-format)

---

## 1. Device-Classes

Die Device-Class definiert den grundlegenden Gerätetyp.

| Klasse | Wert | Bezeichnung | Beschreibung |
|---|---|---|---|
| `NET_ERL` | `0x01` | **E**inzel**R**elais-**L**ichtmodul | Netzbetriebenes Einzelrelais (Flurlicht, Keller, …) |
| `NET_ZRL` | `0x02` | **Z**wei-**R**elais + Co**v**er-**L**ichtmodul | Dual-Relais mit Cover-Steuerung (Rollladen) |
| `NET_SEN` | `0x03` | **SEN**sor | Netzbetriebener Sensor (Temperatur, Feuchte, Helligkeit, Bewegung) |
| `BAT_SEN` | `0x04` | **Bat**terie-**Sen**sor | Batteriebetriebener Sensor (Fensterkontakt, Regen, …) |
| `MASTER` | `0xFE` | **Master** | Zentrale Steuereinheit (ESP32 Gateway) |
| `UNKNOWN` | `0xFF` | — | Unbekannter / nicht initialisierter Typ |

---

## 2. Power-Types

| Typ | Wert | Beschreibung |
|---|---|---|
| `MAINS` | `0x00` | Netzbetrieben (Dauerhaft eingeschaltet) |
| `BATTERY` | `0x01` | Batteriebetrieben (Schlafmodus, periodisches Aufwachen) |

**Abhängigkeiten:**
- `MAINS`-Geräte können immer empfangen → unterstützen `SH_FLAG_ACK_REQUEST` zuverlässig.
- `BATTERY`-Geräte haben ein RX-Fenster nach dem Senden, Master muss in diesem Fenster antworten (`SH_CFG_RX_WINDOW_MS`).

---

## 3. Control-Modes

Der Control-Mode beschreibt, wie ein Gerät seine Aktoren steuert.

| Mode | Wert | Geeignet für | Beschreibung |
|---|---|---|---|
| `NONE` | `0x00` | Sensoren | Keine Aktorsteuerung (reiner Sensor) |
| `RELAY` | `0x01` | NET_ERL | Einfaches Ein-/Ausschalten eines Relais |
| `RELAY_LIGHT` | `0x02` | NET_ERL | Lichtsteuerung mit Automatik (Lux-Abhängigkeit, Bewegungsmelder) |
| `DUAL_RELAY` | `0x03` | NET_ZRL | Zwei unabhängige Relais |
| `DUAL_RELAY_LIGHT` | `0x04` | NET_ZRL | Zwei Relais mit Lichtautomatik |
| `COVER` | `0x05` | NET_ZRL | Rollladen-/Jalousiesteuerung (Auf/Ab/Stopp/Position) |

**Auswirkungen:**
- `RELAY_LIGHT` / `DUAL_RELAY_LIGHT` aktiviert die Comfort-Automatik (§10 der Protokoll-Referenz).
- `COVER` aktiviert die Cover-Zustandsmaschine (Kalibrierung, Positionserkennung).

---

## 4. Config-Profiles

Profile bündeln Vorkonfigurationen für bestimmte Einsatzszenarien.

| Profil | Wert | Control-Mode | Beschreibung |
|---|---|---|---|
| `NONE` | `0x00` | — | Kein Profil (individuelle Konfiguration) |
| `HALL_LIGHT` | `0x01` | `RELAY_LIGHT` | Flurlicht-Automatik – Licht ein bei Bewegung+Helligkeit, aus nach Timer |
| `COVER_BASIC` | `0x03` | `COVER` | Rollladen-Basis – Auf/Ab per Taster, Positionserkennung |
| `HALL_MODULE_LED_RING` | `0x04` | `RELAY_LIGHT` | Flurlicht mit LED-Ring (RGB-Statusring) |

---

## 5. Reporting-Modes

| Mode | Wert | Power | Beschreibung |
|---|---|---|---|
| `PERIODIC` | `0x01` | MAINS | Regelmäßige STATE-Sendungen (alle `report_interval_s`s) |
| `EVENT_DRIVEN` | `0x02` | MAINS | Nur bei Ereignissen senden (Taster, Sensoränderung) |
| `HYBRID` | `0x03` | MAINS | Ereignisgesteuert + periodischer STATE als Fallback |
| `SLEEP_PERIODIC` | `0x04` | BATTERY | Aufwachen, senden, schlafen (Zyklus = `wake_interval_s`) |
| `SLEEP_EVENT` | `0x05` | BATTERY | Nur bei Ereignis aufwachen + senden (z.B. Fensterkontakt) |

---

## 6. Capabilities (Bitmask)

Capabilities werden als **16-Bit-Bitmask** (`caps_hi` + `caps_lo`) im Hello-Payload gesendet.

```
Bit     15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
       ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
       │PRS│GAS│LED│CVR│MBN│BTN│BAT│RAI│WIN│MOT│AQI│LUX│HUM│TMP│RL2│RL1│
       └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘
```

| Bit | Capability | Wert | Geräte | Beschreibung |
|---|---|---|---|---|
| 0 | `RELAY` | `0x0001` | ERx, ZRL | Relais 1 vorhanden |
| 1 | `RELAY2` | `0x0002` | ZRL | Relais 2 vorhanden |
| 2 | `TEMP` | `0x0004` | SEN, ERx* | Temperatursensor |
| 3 | `HUM` | `0x0008` | SEN, ERx* | Feuchtesensor |
| 4 | `LUX` | `0x0010` | SEN, ERx* | Helligkeitssensor |
| 5 | `AQI` | `0x0020` | SEN | Luftqualitätssensor (eCO2, TVOC) |
| 6 | `MOTION` | `0x0040` | SEN, ERx* | Bewegungsmelder (PIR) |
| 7 | `WINDOW` | `0x0080` | BAT_SEN | Fensterkontakt |
| 8 | `RAIN` | `0x0100` | BAT_SEN | Regensensor |
| 9 | `BATTERY` | `0x0200` | BAT_SEN | Batterieüberwachung |
| 10 | `BUTTON` | `0x0400` | ERx, ZRL | Einfacher Tastereingang |
| 11 | `MULTIBUTTON` | `0x0800` | ZRL | Mehrfach-Taster (Auf/Ab/Stopp) |
| 12 | `LED_RING` | `0x1000` | ERx | Neopixel-LED-Ring |
| 13 | `COVER` | `0x2000` | ZRL | Rollladen-Steuerung |
| 14 | `GAS` | `0x4000` | SEN, ERx | Gassensor (BME680 gas resistance) |
| 15 | `PRESSURE` | `0x8000` | SEN | Luftdrucksensor (BMP280, …) |

*) ERx-Geräte können optional Sensor-Capabilities haben (Comfort-Varianten).

### 6.1 Typische Capability-Kombinationen

```
NET-ERL-001 (hall basic):
  RELAY | TEMP | HUM | LUX | MOTION
  = 0x0001 | 0x0004 | 0x0008 | 0x0010 | 0x0040
  = 0x005D (93)

NET-ERL-002 (LED ring):
  RELAY | TEMP | HUM | LUX | MOTION | AQI | PRESSURE | BUTTON | LED_RING | GAS
  = 0x0001 | 0x0004 | 0x0008 | 0x0010 | 0x0040 | 0x0020 | 0x8000 | 0x0400 | 0x1000 | 0x4000
  = 0xD47D (54397)

NET-ZRL-002 (shutter):
  RELAY | RELAY2 | COVER | MULTIBUTTON
  = 0x0001 | 0x0002 | 0x2000 | 0x0800
  = 0x2803 (10243)

NET-SEN-002 (weather):
  TEMP | HUM | LUX | PRESSURE | RAIN
  = 0x0004 | 0x0008 | 0x0010 | 0x8000 | 0x0100
  = 0x811C (33052)

BAT-SEN-001 (window):
  BATTERY | WINDOW
  = 0x0200 | 0x0080
  = 0x0280 (640)

BAT-SEN-002 (rain):
  BATTERY | RAIN
  = 0x0200 | 0x0100
  = 0x0300 (768)
```

---

## 7. Meta-Schema-Version

Definiert die Version des Hello-Meta-Schemas.

| Konstante | Wert | Beschreibung |
|---|---|---|
| `SH_META_SCHEMA_VERSION_1` | `0x01` | Aktuelle Schema-Version |

Eine Erhöhung der Version erlaubt zukünftige Erweiterungen des HelloPayload-Schemas bei gleichzeitiger Abwärtskompatibilität.

---

## 8. Gerätematrix

| Gerät | Typ | Power | Control-Modes | Typische Caps | Profile |
|---|---|---|---|---|---|
| **NET-ERL-xxx** | `NET_ERL` | MAINS | RELAY, RELAY_LIGHT | RELAY, MOTION, LUX, BUTTON, TEMP, HUM, LED_RING | HALL_LIGHT, HALL_MODULE_LED_RING |
| **NET-ZRL-xxx** | `NET_ZRL` | MAINS | DUAL_RELAY, DUAL_RELAY_LIGHT, COVER | RELAY, RELAY2, COVER, MULTIBUTTON | COVER_BASIC |
| **NET-SEN-xxx** | `NET_SEN` | MAINS | NONE | TEMP, HUM, LUX, MOTION, AQI, PRESSURE | — |
| **BAT-SEN-xxx** | `BAT_SEN` | BATTERY | NONE | BATTERY, WINDOW, RAIN, BUTTON, TEMP | — |
| **MASTER-xxx** | `MASTER` | MAINS | — | — | — |

---

## 9. ID-Format

Die Geräte-ID folgt dem Schema `{KLASSE}-{KÜRZEL}-{NR}` mit fester Länge von 10–11 Zeichen.

### 9.1 Gültige IDs

```
NET-ERL-001
NET-ZRL-042
NET-SEN-007
BAT-SEN-015
MASTER-001
```

### 9.2 Prüfung (`isValidDeviceId`)

1. Länge: 10–11 Zeichen
2. Zeichen: A–Z, 0–9, Bindestrich (`-`)
3. Letzte 3 Zeichen: Ziffern (0–9)
4. Format: `XXX-XXXX-XXX` oder `XXX-XXX-XXX`

---

> **Änderungshistorie**  
> v1 – Initiale DeviceTypes-Definition, Firmware Release 1.
