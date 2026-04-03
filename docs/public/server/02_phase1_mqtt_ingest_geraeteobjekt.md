# Phase 1 MQTT-Ingest und Geräteobjekt

## Ingest-Regeln

Der Server verarbeitet MQTT nicht frei nach Bauchgefuehl, sondern nach einer festen Reihenfolge:

1. Topic erkennen
2. Geraet oder Master zuordnen
3. Payload als JSON lesen
4. passenden Block aktualisieren
5. den gemeinsamen Laufzeitzustand fortschreiben

## Topic auf Zielblock

### `meta`

- aktualisiert `identity` und `meta`
- ersetzt die bisher bekannte Metasicht
- darf unbekannte Geraete auto-anlegen
- leitet die faehigkeitsnahe Sicht aus `caps` und `device_class` ab

### `availability`

- aktualisiert nur `availability`
- setzt `online`
- setzt `last_seen_at`
- veraendert keine Sensor- oder Aktorfelder

### `state`

- aktualisiert nur bekannte State-Felder
- spiegelt wenige echte Laufzeitparameter nach `config`
- setzt `last_seen_at`
- behandelt fehlende Felder nicht als Loeschsignal

### `event`

- schreibt nur in `last_event`
- ergaenzt den Zustand, ersetzt ihn nicht

### `ack`

- schreibt nur in `last_ack`
- ist fuer Nachvollziehbarkeit da, nicht fuer den fachlichen Hauptzustand

### `master/status` und `master/event`

- laufen komplett getrennt vom normalen Geraetepfad
- dienen nur der Server- und Verbindungsdiagnose

## Zielstruktur pro Geraet

```text
identity
meta
availability
state
config
last_event
last_ack
diagnostics
```

Diese Struktur ist absichtlich simpel. Wer hier wieder Sondermodelle pro Geraetetyp einzieht, produziert denselben Wartungsschaden wie vorher, nur mit anderem Dateinamen.

## Bekannte Pflichtregeln

- `state` ist die Hauptwahrheit fuer den sichtbaren Geraetezustand
- `event` darf den State nicht heimlich ersetzen
- `ack` darf keine Bedienerfolge vortaeuschen
- `meta`, `availability` und `state` duerfen unbekannte Geraete robust anlegen
- der Master bleibt separat

## Phase-1-Grenze

Noch nicht Teil dieses Schritts:

- Timeseries-Schreiben
- Logs- und Console-Historien
- UI-Ableitungen
- Command-Versand
- Wetter und Automationen

Das ist kein Mangel. Das ist Scope-Kontrolle.
