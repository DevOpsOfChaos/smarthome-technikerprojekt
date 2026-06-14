---
theme: default
title: Smart-Home-Technikerprojekt
info: |
  Technikerprojekt-Präsentation von Manuel Ries.
  Ziel: lokaler, modularer Smart-Home-Prototyp mit eigener Hardware, Firmware, Kommunikationspfad und Dashboard.
class: text-left
colorSchema: dark
transition: slide-left
mdc: true
fonts:
  sans: Inter
  mono: JetBrains Mono
---

# Smart-Home-Grundsystem

## Lokaler Prototyp mit eigener Hardware, Firmware und Bedienoberfläche

**Leitfrage:**  
Wurde ein belastbarer lokaler Systempfad von einem realen Gerät bis zur nachvollziehbaren Rückmeldung aufgebaut und getestet?

<div class="mt-10 text-sm opacity-70">
Technikerarbeit 2026 · Manuel Ries
</div>

<!--
Bildidee: kein Materialwust auf der Titelfolie. Optional ein dezentes Hero-Bild aus Auswahlfotos/Teststation oder net_sen.
Quelle prüfen: 02_Projektvortrag/01_Auswahlfotos/Teststation.png

Notes:
Nicht mit Details starten. Die Prüfer müssen sofort verstehen: Es geht nicht um ein einzelnes Licht oder einen einzelnen Sensor, sondern um eine nachweisbare lokale Systemkette. Diese Leitfrage bleibt der rote Faden des gesamten Vortrags.
Zielzeit: 0:45.
-->

---
layout: two-cols
---

# 1 · Problem und Ziel

::left::

## Ausgangslage

- Smart-Home-Systeme sind oft cloud- oder herstellergebunden
- Unterschiedliche Geräte brauchen gemeinsame Struktur
- Bedienung allein beweist noch keinen zuverlässigen Systempfad

::right::

## Projektziel

- lokaler Prototyp ohne Cloud-Abhängigkeit
- modularer Aufbau für Sensoren und Aktoren
- nachvollziehbarer Pfad: Gerät → Server → Dashboard → Rückmeldung
- klare Abgrenzung: Prototyp, keine Serienreife

<!--
Bildquelle als Hauptvisual: 02_Projektvortrag/02_Dokumentationsgrafiken/Kernprobleme und Nachweisrichtung in IoT.png

Notes:
Hier nicht über Komfortfunktionen sprechen. Die technische Problemstellung ist wichtiger: Wie bringt man verschiedene Sensor- und Aktorgeräte in ein einheitliches lokales System, das prüfbar bleibt? Betonen: Ziel ist nicht ein fertiges Produkt, sondern ein sauber belegter Prototyp.
Zielzeit: 1:15.
-->

---
layout: center
---

# 2 · Architektur

<div class="text-2xl leading-relaxed mt-8">
Gerät <span class="opacity-60">→</span> Master/Gateway <span class="opacity-60">→</span> Server/Dashboard
</div>

<div class="grid grid-cols-3 gap-6 mt-12 text-center">
  <div class="border rounded-xl p-5">
    <div class="text-xl font-bold">Geräteebene</div>
    <div class="opacity-75 mt-2">Sensorik, Aktorik, lokale Firmware</div>
  </div>
  <div class="border rounded-xl p-5">
    <div class="text-xl font-bold">Master</div>
    <div class="opacity-75 mt-2">Brücke zwischen ESP-NOW und MQTT/JSON</div>
  </div>
  <div class="border rounded-xl p-5">
    <div class="text-xl font-bold">Server</div>
    <div class="opacity-75 mt-2">Mosquitto, Node-RED, SQLite, Dashboard</div>
  </div>
</div>

<!--
Bildquelle als spätere Vollbild-/Overlayfolie: 02_Projektvortrag/02_Dokumentationsgrafiken/Dreischichtige Systemarchitektur-Infografik.png
Interaktionsidee später: Schrittweise Einblendung Gerät -> Master -> Server.

Notes:
Die Architektur ist die Mitte des Vortrags. Wichtig: Der Server spricht in der Hauptlinie nicht direkt mit den Feldgeräten. Der Master trennt Funkstrecke und Serverwelt. Diese Trennung macht das System erklärbar und testbar.
Zielzeit: 1:20.
-->

---
layout: two-cols
---

# 3 · Systemumfang

::left::

## Gerätetypen

- **Master** als Gateway
- **net_erl** für Relais, Licht, Sensorik
- **net_zrl** für Rollladensteuerung
- **net_sen** als Außensensorik
- **bat_sen** als batteriebetriebener Meldeknoten

::right::

## Gemeinsame Idee

- unterschiedliche Hardwareaufgaben
- ein gemeinsamer Kommunikations- und Serververtrag
- kein Einzelgerätechaos
- Erweiterbarkeit über Klassen und Fähigkeiten

<!--
Bildquelle: 02_Projektvortrag/02_Dokumentationsgrafiken/Geräteübersicht und Prüfpunkte.png
Optional: Gerätefotos aus 02_Projektvortrag/01_Auswahlfotos/

Notes:
Nicht jedes Gerät einzeln erklären. Es geht darum, dass mehrere reale Gerätetypen in dieselbe Systemlogik passen. Ein Satz pro Klasse reicht. Wenn Prüfer Details wollen, kommen die in Fragen oder Backup-Folien.
Zielzeit: 1:30.
-->

---
layout: two-cols
---

# 4 · Hardware-Eigenleistung

::left::

## Sichtbare Eigenleistung

- eigene Leiterplatten und Baugruppen
- reale Bestückung, Montage und Gehäuse
- Sensorik und Aktorik praktisch aufgebaut
- Teststation als Integrationsumgebung

::right::

## Warum das zählt

- kein reines Softwareprojekt
- elektrische und mechanische Umsetzung nachvollziehbar
- Hardware ist Grundlage für reale Tests
- Nachweise statt nur Konzeptgrafiken

<!--
Bildquellen:
- 02_Projektvortrag/01_Auswahlfotos/bat_sen/bat_sen_Platine.png
- 02_Projektvortrag/01_Auswahlfotos/net_sen/net_sen_wetterstation.png
- 02_Projektvortrag/01_Auswahlfotos/Teststation.png
- 02_Projektvortrag/02_Dokumentationsgrafiken/Hardware-Nachweiskette.png

Notes:
Diese Folie muss stark wirken, aber nicht überladen sein. Maximal zwei große Bilder später einsetzen: eine Platine und ein fertiges Gerät/Testaufbau. Keine BOM, keine Gerberdetails, keine Schaltplanausschnitte im Hauptvortrag.
Zielzeit: 1:30.
-->

---
layout: center
---

# 5 · Kommunikations- und Steuerpfad

<div class="text-xl mt-8">
Command → Weiterleitung → ACK → aktualisierter State → Dashboard
</div>

<div class="grid grid-cols-5 gap-3 mt-12 text-center text-sm">
  <div class="border rounded-lg p-4">Bedienaktion</div>
  <div class="border rounded-lg p-4">MQTT/JSON</div>
  <div class="border rounded-lg p-4">Master / Gerät</div>
  <div class="border rounded-lg p-4">ACK</div>
  <div class="border rounded-lg p-4">State sichtbar</div>
</div>

<div class="mt-10 text-lg opacity-80">
Eine Aktion gilt erst dann als belastbar, wenn Befehl, Bestätigung und Zustand zusammenpassen.
</div>

<!--
Bildquellen:
- 02_Projektvortrag/02_Dokumentationsgrafiken/Kommunikationsfluss Diagramm mit Prozessen.png
- 02_Projektvortrag/02_Dokumentationsgrafiken/Command-ACK-Ablauf der Aktorsteuerung.png
- 02_Projektvortrag/02_Dokumentationsgrafiken/MQTT-Architektur und Kommando-Pfad Übersicht.png
Interaktionsidee später: Die fünf Schritte einzeln hervorheben.

Notes:
Das ist die wichtigste technische Folie. Nicht sagen: Ich drücke im Dashboard einen Button und dann geht etwas an. Das ist zu schwach. Sagen: Der technische Erfolg wird über Command, ACK und aktualisierten State nachvollziehbar gemacht. Genau damit unterscheidet sich die Arbeit von einer bloßen Basteldemo.
Zielzeit: 1:40.
-->

---
layout: two-cols
---

# 6 · Dashboard und Bedienung

::left::

## Dashboard-Aufgabe

- aktuelle Geräte sichtbar machen
- Bedienaktionen auslösen
- Rückmeldungen prüfen
- Fehler- und Zustandsdaten unterscheidbar darstellen

::right::

## Wichtigste Aussage

Das Dashboard ist keine Dekoration. Es ist die sichtbare Prüfebene für Serverdaten, Geräteantworten und Bedienzustände.

<!--
Bildquellen:
- 02_Projektvortrag/01_Auswahlfotos/Dashboard/Dashboard_dark_neu.png
- 02_Projektvortrag/01_Auswahlfotos/Dashboard/Geräte_Detailansicht.png
- 02_Projektvortrag/02_Dokumentationsgrafiken/Dashboard-Navigation und Geräteübersicht analysiert.png
Vor Nutzung auf private IDs, IPs, Topics und Personendaten prüfen.

Notes:
Hier kann später ein großer Screenshot fast die ganze Folie tragen. Wichtig: Dashboard nicht als hübsche Oberfläche verkaufen, sondern als überprüfbare Sicht auf echte Serverdaten. Falls der Screenshot private Details enthält, vorher bereinigen.
Zielzeit: 1:20.
-->

---
layout: two-cols
---

# 7 · Tests und Stabilität

::left::

## Nachweislinie

- End-to-End-Tests
- Command-Roundtrip
- Restart- und Reconnect-Recovery
- Dashboard-Aktorsteuerung mit MQTT-Trace
- 48h-Praxisdauerlauf

::right::

## Einordnung

- Tests belegen den Prototypumfang
- Recovery ist wichtiger als Schönwetterbetrieb
- 48h sind Integrationsnachweis, keine Langzeitgarantie

<!--
Bildquelle:
- 02_Projektvortrag/02_Dokumentationsgrafiken/Server- und Dashboard-Nachweise Fokus.png
- 03_Wichtige_Nachweise/01_Testnachweise/01_Anhaenge/08_Dashboard_Aktorsteuerung_MQTT_Trace_bereinigt.png
Quellen nennen: T-08, T-12, T-13, T-14, T-16, Anlage E, 48h-Praxisdauerlauf.

Notes:
Diese Folie schützt vor dem Eindruck eines reinen Bastelprojekts. Nicht jeden Test erklären. Nur Testarten und Zweck nennen. Bei 48h klar bleiben: Integrationsnachweis, keine Aussage über Monate oder Serienbetrieb.
Zielzeit: 1:30.
-->

---
layout: two-cols
---

# 8 · Fehler, Korrekturen und Grenzen

::left::

## Technische Reife zeigt sich an Korrekturen

- Fehler wurden dokumentiert
- Ursachen wurden eingegrenzt
- Fixes wurden wiederholt geprüft
- Grenzen wurden nicht schöngeredet

::right::

## Bewusste Grenzen

- keine CE-/EMV-/VDE-Prüfung
- keine Serienreife
- keine Cloudplattform
- keine voll ausgebaute Komfortautomation
- keine überzogene Batterielaufzeitbehauptung

<!--
Bildidee:
- schlichte Textfolie oder Prozessgrafik: 02_Projektvortrag/02_Dokumentationsgrafiken/Methode der stufenweisen Integration.png
- optional: Prozesskette der Systementwicklung

Notes:
Diese Folie nicht defensiv halten. Grenzen offen zu benennen ist stärker, als Produktreife zu behaupten. Projektstärke: sauberer Prototyp, klare Nachweise, echte Korrekturen.
Zielzeit: 1:10.
-->

---
layout: center
class: text-center
---

# 9 · Fazit

<div class="grid grid-cols-3 gap-5 mt-10 text-left">
  <div class="border rounded-xl p-5">
    <div class="font-bold text-xl mb-2">Lokal</div>
    <div class="opacity-75">ohne Cloud-Abhängigkeit, eigener Kommunikationspfad</div>
  </div>
  <div class="border rounded-xl p-5">
    <div class="font-bold text-xl mb-2">Modular</div>
    <div class="opacity-75">mehrere Gerätetypen über gemeinsamen Systemvertrag</div>
  </div>
  <div class="border rounded-xl p-5">
    <div class="font-bold text-xl mb-2">Belegt</div>
    <div class="opacity-75">Hardware, Firmware, Dashboard und Tests nachvollziehbar dokumentiert</div>
  </div>
</div>

<div class="mt-12 text-xl">
Ziel erreicht im Prototypumfang: ein durchgängiger, lokaler und testbarer Smart-Home-Systempfad.
</div>

<!--
Bildidee: dezenter Hintergrund mit Teststation oder Architekturlinie. Keine neue Information mehr.

Notes:
Kurz abschließen. Keine neue Technik einführen. Die drei Begriffe lokal, modular, belegt sind die Schlussanker. Danach bereit sein für Fragen.
Zielzeit: 0:50.
-->

---
layout: center
class: text-center
---

# Backup · Demo-Fallback

<div class="text-2xl mt-8">
Wenn die Live-Demo ausfällt: Screenshot + Trace + Command-ACK-Ablauf.
</div>

<div class="mt-10 opacity-80">
Die Präsentation muss auch ohne Live-System vollständig prüfbar bleiben.
</div>

<!--
Nicht als Hauptfolie verwenden, nur bei Bedarf.
Quellen:
- 02_Projektvortrag/02_Demo_Ablauf.md
- 02_Projektvortrag/02_Dokumentationsgrafiken/Command-ACK-Ablauf der Aktorsteuerung.png
- 03_Wichtige_Nachweise/01_Testnachweise/01_Anhaenge/08_Dashboard_Aktorsteuerung_MQTT_Trace_bereinigt.png

Notes:
Nur verwenden, wenn Demo unsicher ist oder Prüfer nach Demoabsicherung fragen. Keine Entschuldigung daraus machen, sondern als vorbereitete Nachweiskette darstellen.
-->

---
layout: center
---

# Backup · Prüferfragen

## Bereithalten, aber nicht im Hauptvortrag ausbreiten

- Schaltplan- und PCB-Details
- Firmware-Codepfade
- Node-RED-Flow-Logik
- SQLite-Schema
- Datenblätter und Grenzwerte
- Testprotokolle im Detail

<!--
Notes:
Diese Folie ist für Fragen da. Im Hauptvortrag wäre sie Ballast. Nur öffnen, wenn gezielt nach Tiefe gefragt wird.
-->
