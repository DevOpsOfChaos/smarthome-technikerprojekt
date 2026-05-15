# net_sen_env_bme680_veml

Konkreter Umweltpfad fuer den Basistyp `net_sen`.

Offizielle Linienzuordnung: `device_id = net_sen_01`, `fw_variant = net_sen_env_bme680_veml`

## Ziel dieses Pfads

- real validiertes Sensorpaket `BME680 + VEML7700` aus dem Alt-Repo
- ENS160 als enger Folgepfad mit externer BME680-Kompensation
- klare Trennung zum DHT22-Referenzpfad

## Was dieser Pfad liefert

- `temp_01c` aus BME680
- `hum_01pct` aus BME680
- `lux` aus VEML7700
- `pressure_pa` und `gas_ohm` aus BME680 (`gas_ohm` erst nach Warm-up belastbar)
- `aqi`, `tvoc_ppb`, `eco2_ppm` aus ENS160
- `motion = 0`, `fault` bei Sensor-Fehlern

## Was bewusst nicht geliefert wird

- keine Bewegungssensorik
- keine Umdeutung von `gas_ohm` zu IAQ/CO2
