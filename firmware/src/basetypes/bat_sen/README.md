# bat_sen

Neutraler Basistyp fuer batteriebetriebene Sensor- und Event-Nodes.

Enthalten:
- Wake/Sleep-Grundmechanik (Discovery- und RX-Fenster)
- ESP-NOW-Basisfluss (HELLO, STATE, EVENT, ACK/CFG)
- neutrale Batteriebasis (mV, Prozent, Fault)
- Device-Hooks fuer I/O, State-Kanaele, Event-Mapping und Wake-Kandidaten

Nicht enthalten:
- konkrete Fensterkontakt-Semantik
- konkrete Regensensor-Semantik
- konkrete Tasterregeln
- geraetespezifische Pin-/Default-Profile
