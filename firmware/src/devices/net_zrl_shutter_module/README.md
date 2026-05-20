# NET-ZRL Rolladen

Eigenstaendige Firmware fuer das konkrete Rollo-Modul mit Master-Kommunikation.

- `main.cpp`: ESP-NOW, Master-Bindung, Provisioning, Kalibrierung, Relaisverriegelung, Taster, LEDs und serielle Diagnose
- `DeviceConfig.h`: Device-ID, Pin-Mapping und Pegellogik
- `NetZrlProvisioning.h`: Setup-Portal-Konfiguration fuer dieses Device

## Abnahme-Check

1. Firmware bauen: `pio run -e net_zrl_shutter_module`
2. Master bauen: `pio run -e master_firmware`
3. Erststart ohne gespeicherte Master-MAC: Setup-Modus muss starten.
4. Master-MAC im Setup setzen, Setup verlassen, danach HELLO/STATE im Master pruefen.
5. Lokale Taster pruefen: Up, Down, Stop. Beim Richtungswechsel muss die Relais-Dead-Time greifen.
6. Kalibrierung ueber Stop-Hold starten und Fahrzeiten uebernehmen.
7. Master-Kommandos pruefen: open, close, stop, set_position.
