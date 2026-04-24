# Minimalserver Smoke-Checkliste

## Ziel

Diese Checkliste dient zur lokalen Prüfung des aktuellen Minimalserver-Stands nach dem Rückbau auf Snapshot-Persistenz ohne InfluxDB, Zeitreihen und Verlaufslogik.

Der Test soll bestätigen, dass der reduzierte Server weiterhin die Kernfunktionen sauber erfüllt.

## Kernfragen

Der Lauf gilt nur dann als erfolgreich, wenn der Server weiterhin:

- sauber startet
- Geräte sauber anzeigt
- aktuelle Sensordaten sauber anzeigt
- Online-/Offline-Status sauber anzeigt
- Batterieanzeige sauber darstellt
- Aktoren schaltbar macht
- Rolladenfunktionen abhängig vom Kalibrierstatus korrekt behandelt
- den aktuellen Snapshot in SQLite sauber fortschreibt
- keine InfluxDB mehr benötigt

---

## 1. Stack-Start prüfen

### Schritte
1. In den Ordner `server/` wechseln
2. `docker compose up -d` ausführen
3. `docker compose ps` prüfen

### Soll
- Mosquitto läuft
- Node-RED läuft
- kein InfluxDB-Dienst mehr vorhanden
- keine offensichtlichen Compose- oder Startfehler

---

## 2. Node-RED-Start prüfen

### Schritte
1. Node-RED im Browser öffnen
2. auf erkennbare Startfehler achten
3. falls nötig Logs prüfen

### Soll
- Node-RED startet sauber
- aktive Flows sind geladen
- keine offensichtlichen Fehler durch entfernte Historien- oder Influx-Pfade

---

## 3. Server-Contract-Smoke-Test ausführen

### Schritte
1. Aus dem Repo-Root den vorhandenen Smoke-Test starten
2. falls der Stack bereits läuft, die passende Skip-Start-Variante nutzen

### Soll
- Test läuft ohne harte Fehler durch
- keine SQL-/Migrationsfehler
- kein Verweis auf Influx-Abhängigkeit
- Snapshot-/Vertragslogik bleibt intakt

---

## 4. Geräteübersicht prüfen

### Schritte
1. Dashboard-Übersicht öffnen
2. bekannte Geräte kontrollieren

### Soll
- Geräte erscheinen weiterhin
- Anzeigenamen sind vorhanden
- Online-/Offline-Zustand wirkt plausibel
- letzte Aktualisierung wirkt plausibel
- Sensorkacheln / Zustände wirken nicht leer oder beschädigt

---

## 5. Gerätedetailseite prüfen

### Schritte
1. mindestens ein Sensorgerät öffnen
2. mindestens ein Aktorgerät öffnen
3. wenn vorhanden, Rolladengerät öffnen

### Soll
- Detailseite lädt sauber
- relevante aktuelle Werte werden angezeigt
- Batterieanzeige ist sichtbar, falls Batteriegerät
- keine offensichtlichen UI-Reste aus alter Verlaufslogik

---

## 6. Snapshot in SQLite prüfen

### Schritte
1. SQLite-Datei prüfen
2. kontrollieren, ob aktuelle Zustände fortgeschrieben werden
3. prüfen, ob nur der aktuelle Snapshot relevant ist

### Soll
- `devices`, `device_state_latest`, `master_status` wirken plausibel
- keine Erwartung mehr an InfluxDB oder Zeitreihenhaltung
- aktuelle Werte und Zeitbezüge werden sauber aktualisiert

---

## 7. Aktorsteuerung prüfen

### Schritte
1. Relaisfunktion testweise schalten
2. ACK-/Statusrücklauf prüfen
3. UI-Verhalten prüfen

### Soll
- Command wird angenommen
- ACK/Status kommt sauber zurück
- UI zeigt den aktuellen Zustand plausibel
- keine offensichtliche Abhängigkeit von alter Verlaufsspeicherung

---

## 8. Rolladenlogik prüfen

### Schritte
1. Rolladengerät öffnen
2. Kalibrierstatus beachten
3. Bedienbarkeit gegen den Gerätezustand prüfen

### Soll
- unkalibrierte Geräte erlauben keine unpassenden Positionsfunktionen
- kalibrierte Geräte verhalten sich passend zum aktuellen Zustand
- keine unlogische Freigabe nur wegen alter UI-Reste

---

## 9. Neustartfestigkeit grob prüfen

### Schritte
1. Stack neu starten
2. Dashboard erneut prüfen

### Soll
- Geräte bleiben bekannt
- Anzeigenamen bleiben erhalten
- letzter Snapshot bleibt plausibel erhalten
- keine Influx-bezogene Wiederanlaufabhängigkeit

---

## Bewertung

Der Smoke-Test ist erfolgreich, wenn der reduzierte Server als Snapshot-, Anzeige- und Steuerkern stabil arbeitet, ohne für Kernfunktionen noch auf Historien- oder Influx-Pfade angewiesen zu sein.
