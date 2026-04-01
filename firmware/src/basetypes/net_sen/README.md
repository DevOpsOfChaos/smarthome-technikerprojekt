# net_sen

Schlanker Basistyp fuer netzbetriebene Sensor-Knoten ohne Aktorik.

Enthalten:
- HELLO, HEARTBEAT, STATE im gemeinsamen Protokoll
- fester Grundablauf fuer BME280 (Temperatur/Feuchte) und VEML7700 (Lux)
- einfache Konfiguration nur fuer `report_interval`

Nicht enthalten:
- lokale Automationslogik
- Sonderbehandlungen fuer andere Geraete
- serverseitige Konfigurationspfade
