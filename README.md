
# smarthome-technikerprojekt

Smart-Home-Grundsystem – Technikerarbeit 2026

## Projektübersicht

Eigenständig entwickeltes, lokal betriebenes Smart-Home-Grundsystem mit selbst aufgebauten Mikrocontrollergeräten, ESP-NOW-Funkkommunikation und Node-RED-Dashboard.

- **Keine Cloud** – alles läuft lokal
- **Keine Herstellerbindung** – ESP32-C3, eigene Platinen, eigene Firmware
- **Vollständig nachvollziehbar** – von der Platine bis zur Bedienoberfläche

## Architektur

```
Geräte (ESP32-C3) ──ESP-NOW──▶ Master ──MQTT──▶ Server (Raspberry Pi)
                                                 ├── Mosquitto (Broker)
                                                 ├── Node-RED (Logik + Dashboard)
                                                 └── SQLite (Persistenz)
```

## Gerätetypen

| Typ | Instanzen | Funktion |
|-----|-----------|----------|
| **net_erl** | Hall-Light, Kitchen | Relais, Umweltsensorik, Bewegung |
| **net_zrl** | Rollladen | 2-Relais-Steuerung, Kalibrierung |
| **net_sen** | Außensensor | BME280, VEML7700, Regensensor |
| **bat_sen** | Regen, Fenster | Batteriebetrieb, Deep-Sleep |

## Repository-Struktur

| Verzeichnis | Inhalt |
|-------------|--------|
| `firmware/` | **Hauptbestandteil** – PlatformIO-Projekt mit Basistypen und konkreten Geräteinstanzen |
| `hardware/` | KiCAD-Schaltpläne, Platinenlayouts, 3D-Modelle |
| `server/` | Docker-Compose-Stack (Mosquitto, Node-RED, SQLite) |
| `docs/` | Technische Dokumentation und Referenz |
| `tests/` | Testskripte und Checklisten |
| `PROTOKOLL/` | Entwicklungs- und Testprotokolle (Nachweise) |
| `esphome/` | ESPHome-Alternativlinie (nicht Bestandteil der Technikerarbeit) |

## Zwei Firmware-Linien

### Eigene Firmware (Hauptlinie)
Die eigene C++-Firmware unter `firmware/` ist der Kern des Technikerprojekts. Sie nutzt:
- **Basistyp-Architektur** – wiederverwendbare Runtime-Klassen (`NetErlRuntime`, `NetSenRuntime`, etc.)
- **ESP-NOW** – direkte Funkkommunikation zwischen Geräten und Master
- **Master-Bridge** – Übersetzung zwischen ESP-NOW und MQTT
- **Vollständige Kontrolle** – Timing, Provisionierung, Fehlerbehandlung in eigenem Code

### ESPHome-Alternative
Unter `esphome/` liegt eine alternative MQTT-Linie für Nutzer, die ESPHome oder Home Assistant bevorzugen. Diese Geräte sprechen direkt MQTT mit demselben Serververtrag. Die ESPHome-Linie ist nicht Bestandteil der Technikerarbeit, sondern eine praktische Erweiterung für Anwender.

## Quick Start

### Server starten
```bash
cd server
docker compose up -d
```
Dashboard: http://localhost:1880/ui

### Firmware bauen
```bash
cd firmware
pio run -e net_erl_hall_light -t upload
```

## Technologien

- **MCU:** ESP32-C3 (RISC-V)
- **Kommunikation:** ESP-NOW (lokal), MQTT (Server)
- **Server:** Node-RED, Mosquitto, SQLite, Docker
- **Firmware:** PlatformIO, Arduino-Framework
- **Hardware:** KiCAD, Eigenentwurf

## Lizenz

All rights reserved. Dieses Repository ist Teil einer schulischen Prüfungsleistung.  
Nach Abschluss der Prüfung behält sich der Autor einen Wechsel auf eine freizügigere Lizenz vor.

## Autor

Manuel Ries – Heinrich-Hertz-Schule Hamburg – Technikerarbeit 2026
