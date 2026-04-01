# net_sen

Neutraler Basistyp fuer netzbetriebene Sensor-Knoten ohne Aktorik.

Enthalten:
- HELLO, HEARTBEAT, STATE im gemeinsamen Protokoll
- ESP-NOW-Grundkommunikation inkl. HELLO/ACK/CFG
- generische State-/Heartbeat-/CFG-Grundstruktur
- optionale I2C-Basis und Device-Hooks fuer konkrete Sensorik

Nicht enthalten:
- konkrete Sensorinitialisierung
- konkrete Sensorauslesung
- geraetespezifische Defaults
