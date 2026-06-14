# Sprecherleitfaden – 10-Folien-Fassung

## Grundsatz

Der Vortrag beweist den lokalen Systempfad. Er ist keine Nacherzaehlung der ganzen Technikerarbeit.

Roter Faden:

```text
Geraet -> Master -> MQTT/JSON -> Node-RED/SQLite -> Dashboard -> ACK/State -> Testnachweis
```

## 1. Titel und Leitfrage – 0:45

Kernaussage: Das Projekt ist eine lokale Nachweiskette, nicht nur eine einzelne Smart-Home-Funktion.

Sprechrichtung:

- Thema kurz nennen.
- Sofort die Leitfrage setzen.
- Keine Details und keine Materialliste.

## 2. Problem und Ziel – 1:15

Kernaussage: Unterschiedliche Sensor- und Aktorgeraete sollen in ein einheitliches lokales System gebracht werden.

Sprechrichtung:

- Cloud- und Herstellerbindung als Ausgangsproblem nennen.
- Ziel: lokaler, modularer Prototyp.
- Abgrenzen: keine Serienreife, keine Produktplattform.

## 3. Architektur – 1:20

Kernaussage: Die Trennung in Geraet, Master und Server macht das System pruefbar.

Sprechrichtung:

- Geraeteebene: Sensorik und Aktorik.
- Master: Bruecke zwischen Funkstrecke und Serverwelt.
- Server: MQTT, Node-RED, SQLite, Dashboard.
- Wichtig: Der Server spricht in der Hauptlinie nicht direkt mit den Feldgeraeten.

## 4. Systemumfang – 1:30

Kernaussage: Mehrere reale Geraetetypen laufen ueber einen gemeinsamen Pfad.

Sprechrichtung:

- Master, net_erl, net_zrl, net_sen, bat_sen kurz einordnen.
- Nicht jedes Geraet technisch ausbreiten.
- Fokus: gemeinsame Struktur statt Einzelgeraetechaos.

## 5. Hardware-Eigenleistung – 1:30

Kernaussage: Das Projekt besteht nicht nur aus Software, sondern aus realer Hardware.

Sprechrichtung:

- Eigene Leiterplatten und Baugruppen nennen.
- Bestueckung, Gehaeuse und Teststation zeigen.
- Keine BOM, keine Gerberdetails, keine Schaltplananalyse im Hauptvortrag.

## 6. Kommunikations- und Steuerpfad – 1:40

Kernaussage: Eine Aktion ist erst durch Command, ACK und aktualisierten State belastbar belegt.

Sprechrichtung:

- Nicht sagen: Button gedrueckt, Licht geht an.
- Besser: Bedienaktion erzeugt Kommando, System bestaetigt, Zustand wird sichtbar.
- Das ist der technische Kern der Nachweisbarkeit.

## 7. Dashboard und Bedienung – 1:20

Kernaussage: Das Dashboard ist eine Pruefebene fuer echte Serverdaten.

Sprechrichtung:

- Dashboard nicht als Deko verkaufen.
- Sichtbarkeit, Bedienung und Rueckmeldung verbinden.
- Screenshot nur verwenden, wenn er vorher geprueft wurde.

## 8. Tests und Stabilitaet – 1:30

Kernaussage: Das Projekt ist ueber Tests, Recovery und Praxisdauerlauf abgesichert.

Sprechrichtung:

- Testarten nennen, nicht jedes Protokoll vorlesen.
- End-to-End, Roundtrip, Restart, Reconnect, Dashboard-Trace.
- 48h-Praxisdauerlauf korrekt einordnen: Integrationsnachweis, keine Langzeitgarantie.

## 9. Fehler, Korrekturen und Grenzen – 1:10

Kernaussage: Offen dokumentierte Fehler und Grenzen wirken staerker als uebertriebene Produktbehauptungen.

Sprechrichtung:

- Fehlerdiagnose und Wiederholungstests als Reifezeichen darstellen.
- Grenzen offen nennen: keine CE-/EMV-/VDE-Pruefung, keine Serienreife, keine Cloudplattform.
- Nicht defensiv werden.

## 10. Fazit – 0:50

Kernaussage: Ziel erreicht im Prototypumfang.

Sprechrichtung:

- Drei Schlussanker: lokal, modular, belegt.
- Keine neue Technik einfuehren.
- Kurz und sicher beenden.

## Backup-Folien

Nur nutzen, wenn gefragt oder wenn die Live-Demo nicht funktioniert.

- Demo-Fallback: Screenshot, Trace und Command-ACK-Ablauf.
- Prueferfragen: Schaltplan, Codepfade, Node-RED, SQLite, Datenblaetter.

## Harte Kuerzungsregel

Wenn der Vortrag ueber 14 Minuten laeuft, zuerst streichen:

1. Geraetedetails
2. Hardware-Nahaufnahmen
3. Dashboard-Zweitansicht
4. technische Nebenpfade
5. jedes Detail, das nicht den roten Faden staerkt
