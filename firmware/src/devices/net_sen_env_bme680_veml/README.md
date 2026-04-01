# net_sen_env_bme680_veml

Konkreter Umweltpfad fuer den Basistyp `net_sen`.

Offizielle Linienzuordnung in diesem Repo:
- `device_id = net_sen_01`
- `fw_variant = net_sen_env_bme680_veml`

Ziel dieses Pfads:
- real validiertes Sensorpaket `BME680 + VEML7700` aus dem Alt-Repo sauber im neuen Repo abbilden
- ENS160 als enger Folgepfad mit externer BME680-Temperatur/Feuchte-Kompensation integrieren
- klare Trennung zum DHT22-Referenzpfad behalten
- keine Scope-Erweiterung auf Bewegungssensorik oder weitere Sensorpfade

Was dieser Pfad liefert:
- `temp_01c` aus BME680
- `hum_01pct` aus BME680
- `lux` aus VEML7700
- `pressure_pa` und `gas_ohm` aus BME680 (`gas_ohm` erst nach Warm-up belastbar)
- `aqi`, `tvoc_ppb`, `eco2_ppm` aus ENS160
- `motion = 0`
- `fault`, wenn BME680 oder VEML7700 ungueltig sind oder ENS160 nach Warm-up zu lange keine gueltigen Daten liefert

Was dieser Pfad bewusst nicht liefert:
- keine quantitative Behauptung zur fachlichen Luftqualitaetsverbesserung
- keine Umdeutung von `gas_ohm` zu IAQ/CO2
- keine Bewegungssensorik
