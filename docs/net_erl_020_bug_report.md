
---

## Anforderungskatalog (vom Betreiber)

### Zeitkritikalitaet
- **Relais (Lampe):** einziges zeitkritisches Element. LD2410-Radar-Out muss so oft
  gelesen werden, dass das Relais **innerhalb 1 Sekunde** nach Bewegung schaltet.
- **Sensorwerte:** unkritisch, 1x pro Minute ausreichend.
- **MQTT-Publish:** 1x pro Minute ausreichend.
- **CPU-Last:** Chip darf und soll entlastet werden (Single-Core ESP32-C3).

### Sensor-Datenqualitaet
- **Mittelwertbildung:** gleitender Mittelwert/Median aus 3 Messungen pro Sensor.
- **Kaltstart:** bei nur 1-2 Messungen wird der verfuegbare Wert gesendet (kein Blockieren).
- **Ziel:** Messwerte sollen ueber Zeit "immer besser werden" (Rauschen reduziert).

### Lampenlogik
- Geraet haengt an der Decke.
- Schaltet bei Bewegung, **wenn es im Raum nicht zu hell ist**.
- **Solange die Lampe AN ist, darf kein Lux-Wert gemessen werden**, da sonst das
  Lampenlicht den echten Raum-Lichtwert verfaelscht.
- Auto-Off-Nachlauf: 15 s nach letzter Bewegung.

### LED-Ring
- Aktiv bei Bewegungserkennung, **auch wenn Lampe nicht schaltet** (weil zu hell).
- Zeigt AQI / Temperatur / Feuchte als Komfortfunktion.
- 45 s Anzeigedauer (15 s pro Phase), dann aus.

### I2C-Bus-Strategie
- Bus ist instabil (2kO Pull-ups, 3 Sensoren).
- Messungen muessen **zeitlich verteilt** werden, um Bus-Last zu minimieren.
- Kein gleichzeitiges Pollen von BME680 + VEML7700 + ENS160 im selben Zyklus.
- ENS160-Kompensation nur schreiben wenn BME680 frische Werte hat.
- **Post-OTA I2C-Lockup:** akzeptierter Workaround (Power-Cycle).

### Bekannte Bugs (geloest)
1. Boot-Crash (AddressableLightEffect auf ESP32-C3/2026.5.1)
2. LED-Ring Phasen-Modulo fehlte
3. ENS160 Kompensation fehlte
4. Auto-On ohne Lux-Wert (5s Timeout)
5. 20-Minuten-Fault (ENS160-Warmup-Burst -> BUG-001)

### Offene Optimierungen
- Sensor-Mittelwertbildung (3-Wert gleitend)
- I2C-Messungen staffeln (Staggering)
- Lux-Messung aussetzen waehrend Lampe AN
- Poll-Intervalle reduzieren (60s statt 10s fuer Sensoren)
- MQTT-Publish-Rate auf 60s reduzieren
- LD2410-Poll auf 50ms halten (fuer <1s Reaktion)
