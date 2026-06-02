# Gemeinsame Bibliotheken

In diesem Ordner liegen nur wirklich gemeinsam genutzte technische Bausteine.

## Typische Inhalte

- Protokollstrukturen
- ESP-NOW-Helfer
- MQTT-Helfer
- Provisionierung
- Storage
- kleine Utilities
- Sensor-Readout-Helfer bei mehrfach gleicher Nutzung

## Nicht vorgesehen

- gerätespezifische Sonderlogik
- Komfortlogik einzelner Geräte
- allgemeine Profil- oder Regel-Engines
- lokale Zustandsmaschinen konkreter Geräte

## Grundsatz

Wiederverwendung ist nur sinnvoll, wenn sie den Code tatsächlich klarer und mehrfach nutzbar macht. Gerätespezifische Logik bleibt in der jeweiligen Geräteschicht.

## Leseguide fuer C/C++-Spezialfaelle

Diese Bibliothek arbeitet an mehreren Stellen bewusst mit C-nahen Konstrukten, weil ESP-NOW, ESP32-Flashspeicher und Arduino-Callbacks rohe Bytes und feste Speicherbereiche erwarten.

- `#define` und `constexpr`: feste Protokollwerte. Diese Zahlen duerfen nicht beliebig geaendert werden, weil Master und Nodes dieselben Werte verstehen muessen.
- Hexwerte wie `0xA5U`: kompakte Bytewerte. Das `U` bedeutet `unsigned` und verhindert Vorzeichenfehler bei Bitoperationen.
- Bitmasken wie `SH_CAP_RELAY`: mehrere Ja/Nein-Faehigkeiten werden platzsparend in einem Integer gespeichert. Geprueft wird mit `wert & MASKE`.
- `__attribute__((packed))`: verhindert Compiler-Padding in Protokollstrukturen. Dadurch entspricht `sizeof(...)` exakt der Funk-Nutzlast.
- `static_assert`: Compile-Zeit-Pruefung. Wenn sich ein Protokoll-Struct versehentlich vergroessert, bricht der Build sofort ab.
- Zeiger wie `uint8_t*` oder `char*`: Verweise auf externe Speicherbereiche. Der Controller besitzt diese Speicher nicht, sondern beschreibt Runtime-Variablen des jeweiligen Geraets.
- Referenzen wie `Settings&`: ein vorhandenes Objekt wird direkt veraendert, ohne eine Kopie anzulegen.
- Funktionszeiger wie `SetupLogFn`: optionaler Callback. Die Bibliothek ruft damit Logger-Code des konkreten Geraets auf, ohne dessen Implementierung zu kennen.
- `memcpy`, `memset`, `strncpy`: kontrollierte Arbeit mit Bytepuffern und C-Strings. Diese Funktionen sind hier absichtlich genutzt, weil die Daten binaer gespeichert oder gefunkt werden.
- `F("...")`: Arduino-Makro, das Stringliterale im Flash statt im RAM haelt. Das spart Arbeitsspeicher auf Mikrocontrollern.
