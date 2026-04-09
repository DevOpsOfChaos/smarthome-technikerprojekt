# Aktueller Status und nächste Schritte

## Öffentlicher Stand
Das öffentliche Repo enthält bereits eine klar erkennbare und real belegte Grundlinie für:
- Projektstruktur
- öffentliche technische Dokumentation
- Server-Phase-1-Basis
- Tests und Nachweise

Im Serverbereich ist aktuell eine belastbare Phase-1-Linie sichtbar:
- MQTT-Ingest für Geräte und Master
- gemeinsames Geräteobjekt
- separater Masterpfad
- minimales SQLite-Schema als Grundlage
- Dashboard V1 mit Geräteübersicht und versteckter Detailseite

## Bereits real nachgewiesen
Folgende Punkte sind im Repo inzwischen nicht nur beschrieben, sondern durch Protokolle und Testläufe belegt:

- realer Command-/ACK-/State-Roundtrip für `net_erl_01`
- erfolgreicher Restart-/Kaltstart-Nachweis des Minimalpfads
- erfolgreicher Node-Recovery-Nachweis für denselben Pfad
- Diagnose eines zu strengen Master-Recovery-Gates
- fachlich korrigierte Bewertung von Master-Recovery über frischen Master-Status plus frische `availability` und `state`
- grüner gegateter Mehrstufenlauf nach Gate-Korrektur
- Diagnose eines echten Master-Fehlers hinter Null-/Minimal-States nach Master-Recovery
- kleinster gezielter Fix im Master gegen dieses Wegsanitisieren valider State-Werte
- realer Hardware-Rerun des Master-Fixes auf `MASTER-001`

Damit ist der öffentliche Stand kein bloßer Strukturentwurf mehr, sondern ein bereits belastbar nachgewiesener technischer Kern.

## Was dieser Stand bedeutet
Der öffentliche Stand ist **nicht** als vollständiges Endsystem zu verstehen.
Er ist die aktuelle offizielle technische Linie, auf die weitere Arbeit sauber aufsetzen soll.

Wichtig ist dabei:
- zuerst fachliche Mitte stabil
- dann saubere Persistenz und sichere Ableitungen
- dann Bedienpfade und Ansichten breiter ausbauen
- Komfortfunktionen erst danach

## Bereits sichtbar im Repo
- grundlegende öffentliche Projektdokumentation
- technische Trennung von Geräteschicht, Master und Server
- erste Server-Phase für Ingest, Zustandsmodell und kleines Dashboard V1
- offizielle Nachweis- und Teststruktur
- reale Recovery- und Roundtrip-Protokolle
- bestätigter Master-Fix im belegten Hardwarepfad

## Bewusst noch nicht voll ausgebaut
- breite Server-UI
- Zeitreihenpfad als vollständige Sicht
- breitere Command-Welt über den engen Minimalpfad hinaus
- Komfortfunktionen, Automationen oder Zusatzwelten
- Vollabdeckung aller Geräteklassen durch denselben Hardware-Nachweis

Das ist keine Schwäche, sondern bewusste Scope-Kontrolle.

## Nächste sinnvolle technische Schritte
1. belegten Phase-1-Kern weiter konsolidieren
2. Persistenzpfad sauber und fachlich eng erweitern
3. erst danach Commands, UI-Ableitungen und Zeitreihen breiter aufziehen
4. zusätzliche Gerätepfade kontrolliert und nachweisbar ergänzen

## Öffentliche Repo-Perspektive
Für Außenstehende soll das Repo jetzt schon professionell lesbar sein:
- klare Projektlinie
- sauberer Einstieg
- keine unnötige interne Unordnung
- nachvollziehbare Trennung zwischen bestätigtem Stand und späteren Ausbaustufen

## Empfehlung für Leser
Wer den aktuellen technischen Kern verstehen will, liest als Nächstes:
- `04_server_schnellstart_phase1.md`
- `docs/public/server/01_server_v1_ueberblick.md`
- `docs/public/server/02_phase1_mqtt_ingest_geraeteobjekt.md`
- die zugehörigen Protokolle unter `PROTOKOLL/`
