# net_sen_env_bme680_veml

Konkreter Umweltpfad fuer den Basistyp `net_sen`.

Offizielle Linienzuordnung in diesem Repo:
- `device_id = net_sen_01`
- `fw_variant = net_sen_env_bme680_veml`

Ziel dieses Pfads:
- real validiertes Sensorpaket `BME680 + VEML7700` aus dem Alt-Repo sauber im neuen Repo abbilden
- klare Trennung zum DHT22-Referenzpfad behalten
- keine Scope-Erweiterung auf ENS160 oder Bewegungssensorik

Was dieser Pfad liefert:
- `temp_01c` aus BME680
- `hum_01pct` aus BME680
- `lux` aus VEML7700
- `motion = 0`
- `fault`, wenn einer der verpflichtenden Sensorpfade ungueltig ist

Was dieser Pfad bewusst nicht liefert:
- kein ENS160
- keine AQI-/CO2-/IAQ-Aussagen
- keine Umdeutung von `gas_ohm` zu Luftqualitaet
