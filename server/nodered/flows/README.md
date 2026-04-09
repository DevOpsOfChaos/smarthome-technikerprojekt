# Node-RED Flows

Diese Ebene bleibt fuer V1 bewusst klein.

- `active/` enthaelt die produktiven Flow-Fragmente.
- `../build-flows.js` liest genau diese Dateien ein und baut daraus `flows.json`.
- Die Reihenfolge ist absichtlich numerisch: Boot, Dashboard-Runtime, Ingest, Device-Store, Dashboard-Views, Master-Diagnostik.

Es gibt hier bewusst keine zweite Flow-Welt fuer Wetter, Charts, Logs, Commands oder Komfortpfade.
