# net_sen_dht22_reference

Kleiner, konkreter Referenzpfad fuer den Basistyp `net_sen`.

Ziel dieses Pfads:
- frueher realer Sensorpfad fuer Temperatur und Feuchte mit DHT22
- einfache Bring-up- und End-to-End-Pruefbasis
- kein Umbau von `net_sen_room`

Was dieser Pfad liefert:
- `temp_01c` aus DHT22
- `hum_01pct` aus DHT22
- `fault` bei ungueltiger Messung

Was dieser Pfad bewusst nicht liefert:
- keine Lichtsensorik (`lux = 0`)
- keine Bewegungssensorik (`motion = 0`)
- keine BME680-/VEML7700-/ENS160-Logik
