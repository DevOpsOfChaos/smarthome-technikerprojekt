# NET-SEN Raumsensor

Konkrete Geraeteauspraegung auf Basis `net_sen`.

Umfang dieses Geraets:
- BME280 fuer Temperatur + Feuchtigkeit
- VEML7700 fuer Helligkeit
- eigene Sensorinitialisierung und Sensorauslesung
- eigene Delta-/Intervall-Defaults fuer STATE-Meldungen

Kommunikation:
- HELLO, HEARTBEAT, STATE ueber den Basistyp `net_sen`
- `report_interval` via CFG
