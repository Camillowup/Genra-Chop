# Merkliste GENRA CHOP (offen)

Nur Offenes. Gebaut wird erst, wenn ausdrücklich „bauen“ kommt; vorher wird jedes Panel als Entwurf gezeigt.
Kein Release ohne ausdrückliches Okay.

2.2.0 (12.11.2026): Umbenennung, MOVE, CLEAR, LOAD SAMPLE, MIDI learn, GENRA CHOP ABLAUF – in Rack testen.
2.3.0 (12.11.2026): GENRA CHOP FX (neu), CHOPS statt DUST, Pad-Banks A–D, STOP/PLAY grösser, Taster-Schrift passt
sich an, ABLAUF ohne RENEWS mit CLEAR / CLEAR ALL, GRID schwarze Tasten – in Rack testen.
2.4.0 (12.11.2026): WARP (Rubber Band / Signalsmith), SAMPLE, CLEAR pro Spur, MPC-Pads, Drum-Reihenfolge und
DENS-Extras, NEW ALL schneidet frisch, GRID nimmt GENRA CHOPs MIDI, FX auf 36 HP mit Einzel-I/O, DJ-Pads und OLEDs,
ABLAUF Drag & Drop und Scrollbalken – in Rack testen (WARP-Qualität und Rechenzeit, FX an WING-LINK).
Offen (Details in `README.md`):
- Wellenform-Fenster: Klick lädt ein Sample, sonst Chops mit der Maus verschieben (erst nach dem Test von GENRA CHOP).
- Follow GENRA für die GENRA-Instrumente (DRUM-X, SCHLAG, FUNKE …): GENRA CHOP ist dort noch keine Quelle
  (`Follow.cpp` im GENRA-Repo kennt nur GENRA und GENRA BOX).
- GENRA CHOP FX: kabellos verbinden (Sound und Clock von GENRA CHOP ohne Kabel) – im Code vorbereitet (Quelle als
  eigene Wahl im Patch, Platz für die Leuchtsegmente über IN L/R und CLK), noch nicht gewünscht.
- GENRA CHOP FX: Effekte und Presets beim Anhören nachstimmen.
