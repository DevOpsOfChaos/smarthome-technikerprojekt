# Server-Kernumfang und bewusste Begrenzung

## Ziel dieser Einordnung

Der Server wird im Projekt bewusst als kleiner, klar begrenzter Kern ausgeführt. Ziel ist kein überladener Plattformserver, sondern eine nachvollziehbare und wartbare Zentrale für Gerätezustände, Anzeige und Steuerung.

## Aktuelle Hauptlinie

Der Server übernimmt im Zielstand insbesondere folgende Aufgaben:

- Geräte erkennen und verwalten
- Geräteübersicht und Gerätedetailseiten bereitstellen
- aktuelle Sensordaten anzeigen
- Online-/Offline-Status und letzte Aktualisierung abbilden
- Batterieanzeige für batteriebetriebene Geräte bereitstellen
- Aktoren schaltbar machen
- Rolladenfunktionen abhängig vom Gerätezustand und Kalibrierstatus korrekt behandeln
- Anzeigenamen und relevante Gerätemetadaten persistent speichern

## Persistenzprinzip

Der Server speichert für Geräte den jeweils aktuellen Zustand als Snapshot in SQLite. Damit bleibt der aktuelle Betriebszustand nachvollziehbar, ohne den Server unnötig um eine separate Verlaufs- oder Zeitreihenhaltung zu erweitern.

Gespeichert werden insbesondere:
- Geräte-Stammdaten
- aktuelle Zustände
- letzte Aktualisierung
- relevante Metadaten und Bedieninformationen

## Bewusste Begrenzung des Serverumfangs

Zeitreihen, Verlaufsdaten und Diagramme wurden nicht in den Kernumfang des aktuellen Serverstands aufgenommen. Solche Funktionen können grundsätzlich sinnvoll sein, sind für den hier verfolgten Projektkern jedoch nicht erforderlich.

Die bewusste Begrenzung dient dazu,

- den Server kleiner und robuster zu halten,
- die Zustands- und Steuerlogik klar nachvollziehbar zu machen,
- die Wartbarkeit zu verbessern,
- und den Fokus auf den tatsächlich benötigten Projektumfang zu legen.

## Gerätebehandlung

Der Server behandelt Geräte über ihre Vertragsdaten, Metadaten, Zustandsfelder und Fähigkeiten. Dadurch bleibt die Geräteintegration nachvollziehbar und nicht an unnötige Sonderpfade gebunden.

## Ergebnis dieser Linie

Der Zielserver ist damit kein allgemeines Analyse- oder Verlaufssystem, sondern eine kompakte und funktionale Zentrale für:

- aktuelle Gerätezustände,
- aktuelle Bedienbarkeit,
- persistente Geräteinformationen,
- und eine klare, wartbare Geräteverwaltung.
