# Aktueller Status und nächste Schritte

## Öffentlicher Stand
Das öffentliche Repo enthält bereits eine klar erkennbare Grundlinie für:
- Projektstruktur
- öffentliche technische Dokumentation
- Server-Phase-1-Basis
- Tests und Nachweise

Im Serverbereich ist aktuell eine erste Phase-1-Linie sichtbar:
- MQTT-Ingest für Geräte und Master
- gemeinsames Geräteobjekt
- separater Masterpfad
- minimales SQLite-Schema als Grundlage
- minimaler Command-/ACK-Pfad fuer `net_erl_01`

## Real bestaetigt auf `main`
Diese Phase-1-Linie ist nicht nur konzeptionell beschrieben, sondern durch konkrete Nachweise im Repo belegt:

- der minimale Server-Ausgang `POST /api/phase1/net-erl/relay-1` ist dokumentiert
- der echte HTTP -> MQTT -> Master -> Geraet -> ACK/State-Rueckweg fuer `net_erl_01` ist belegt
- der Pfad wurde nach Server-Restart erneut erfolgreich nachgewiesen
- der Pfad wurde nach gezieltem Node-Recovery erneut erfolgreich nachgewiesen
- der Master-Recovery-Pfad wurde fachlich korrekt auf frischen Master-Status plus frische `availability`/`state` eingeordnet
- der fruehere Null-State-Fehler nach Master-Recovery ist diagnostiziert, behoben und per Hardware-Rerun auf `MASTER-001` bestaetigt

Sinnvolle Nachweise dazu liegen unter `PROTOKOLL/`, insbesondere:
- `PROTOKOLL_2026-04-04_server_phase1_4_real_command_roundtrip.txt`
- `PROTOKOLL_2026-04-04_server_phase1_5_restart_roundtrip.txt`
- `PROTOKOLL_2026-04-04_server_phase1_6_reconnect_recovery.txt`
- `PROTOKOLL_2026-04-04_master_recovery_meta_gate_clarify.txt`
- `PROTOKOLL_2026-04-04_gate_fix_and_rerun_multistage_recovery.txt`
- `PROTOKOLL_2026-04-04_master_fix_hardware_rerun.txt`

## Was dieser Stand bedeutet
Der öffentliche Stand ist **nicht** als vollständiges Endsystem zu verstehen.
Er ist die aktuelle offizielle technische Linie, auf die weitere Arbeit sauber aufsetzen soll.

Wichtig ist dabei:
- zuerst fachliche Mitte stabil
- dann saubere Persistenz
- dann Bedienpfade und Ansichten
- Komfortfunktionen erst danach

## Bereits sichtbar im Repo
- Grundlegende öffentliche Projektdokumentation
- technische Trennung von Geräteschicht, Master und Server
- erste Server-Phase für Ingest und Zustandsmodell
- minimaler Command-/ACK-Pfad mit realem Hardwarebezug
- Nachweis- und Teststruktur mit echten Recovery- und Rerun-Belegen

## Bewusst noch nicht voll ausgebaut
- breite Server-UI
- Zeitreihenpfad als vollständige Sicht
- breitere allgemeine Command-Welt ueber den vorhandenen Minimalpfad hinaus
- Komfortfunktionen, Automationen oder Zusatzwelten

Das ist keine Schwäche, sondern bewusste Scope-Kontrolle.

## Nächste sinnvolle technische Schritte
1. Phase-1-Kern auf der bestaetigten Linie halten und nur gezielt erweitern
2. Persistenz- und Auswertungspfade ueber dem vorhandenen Kern sauber ausbauen
3. erst danach breitere Commands, UI-Ableitungen und Zeitreihen aufziehen

## Öffentliche Repo-Perspektive
Für Außenstehende soll das Repo jetzt schon professionell lesbar sein:
- klare Projektlinie
- sauberer Einstieg
- keine unnötige interne Unordnung
- nachvollziehbare Trennung zwischen aktuellem Stand und späteren Ausbaustufen

## Empfehlung für Leser
Wer den aktuellen technischen Kern verstehen will, liest als Nächstes:
- `04_server_schnellstart_phase1.md`
- `docs/public/server/01_server_v1_ueberblick.md`
- `docs/public/server/02_phase1_mqtt_ingest_geraeteobjekt.md`
- danach die zugehoerigen Nachweise unter `PROTOKOLL/`
