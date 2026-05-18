# Geräte und Rollen

## Master

Der Master sammelt Daten von den dezentralen Geräten über ESP-NOW und überträgt relevante Informationen und Befehle per MQTT an die Server-Ebene.

## NET-ZRL

Netzbetriebener Basistyp mit zwei Relais. In der Basislinie ist dieser Typ für die Rolladensteuerung vorgesehen.

## NET-SEN

Netzbetriebener Sensorknoten ohne Relais. Dieser Typ dient zur Erfassung und Meldung von Umweltwerten.

## NET-ERL

Netzbetriebener Basistyp mit einem Relais. Dieser Typ bildet die Grundlage für Lichtgeräte wie Hall-Modul und Hall-Modul mit LED-Ring.

## BAT-SEN

Batteriebetriebener Sensorknoten mit Deep-Sleep-Konzept. Dieser Typ eignet sich für Ereignis- oder Statussensorik wie Fensterkontakt oder Regensensor.

## Trennungsregel

Basistypen liefern nur die gemeinsame Grundlage ihres Typs. Konkrete Sensorik, Pins, Sonderfunktionen und lokale Zustandslogik gehören in die jeweilige Geräteschicht.
