# GENRA CHOP – Versionsgeschichte

Was in welcher Version dazukam (neueste zuerst). Bis 2.4.0 lag GENRA CHOP im Repo von GENRA Test
(`Camillowup/testmodulesvcv`, Ordner `genra-chop/`, davor als GENRA HipHop).

### GENRA CHOP 2.5.0
- **GENRA CHOP:**
  - **LOOP** (rechts in der WARP-Leiste): das ganze Sample läuft als Loop im Takt (Start bei Takt 1, neu nach jeder
    LOOP-Länge, Tempo genau auf die Länge, schaltet WARP mit ein); daneben die Länge AUTO / 1 / 2 / 4 / 8 / 16 Takte.
    Das Chop-Pattern pausiert, die Pads bleiben spielbar; „LOOP n BARS“ und Laufzeile im Wellenform-Fenster. LOOP wird
    mit dem Pattern gespeichert. WARP-Leiste dafür schmaler eingeteilt.
  - **Drums / DENS:** Hi-Hats früher und mehr (Ghost-Hats ab etwa 30–40 %, Trap-16tel und Rolls früher, offene Hat vor
    Takt 1 öfter); Clap-Layer nur noch ab und zu (Takt 2 / 4, ab etwa 80 %); Tamburin seltener und leiser (ab etwa
    75 %). Kick, Snare, Rim-Ghosts und Tom-Fill unverändert.
  - **Break (BREAK / BOTH):** Clap, Rim und PERC bekommen keine snareähnlichen Stücke mehr, sondern hellere oder keine.
- **GENRA CHOP ABLAUF:** jedes Pad merkt sich LOOP (rotes **L** auf dem Pad), Rechtsklick › LOOP schaltet es um.
- **GENRA CHOP FX:** die Effekt-Felder von BUS 1 / 2 haben wieder einen dunklen Grund (Schrift lesbar).

### GENRA CHOP 2.4.0
- **GENRA CHOP:**
  - **WARP** (Leiste unter dem Wellenform-Fenster): das Sample ans Tempo des Beats anpassen, Modus AUTO / BEATS /
    TONE, Tempo-Anzeige „87 > 92 BPM“ mit x2 und /2, oben rechts im Wellenform-Fenster RUBBER | SIGNAL (Rubber Band R3
    oder Signalsmith Stretch). Nur das Sample, nicht der Break; ein neues BPM warpt nach kurzer Pause neu.
  - **SAMPLE** spielt das ganze Sample (nochmal = Stopp), mit Laufzeile im Wellenform-Fenster.
  - **CLEAR pro Spur** neben NEW (Akkorde bleiben).
  - **Pads in MPC-Reihenfolge** (1–4 unten); lange Akkordnamen werden kleiner.
  - **Drums:** Reihenfolge Kick, Snare, CH, OH, Clap, Rim, Tom, Perc (Ausgänge und GRID); DENS bringt stiltypische
    Extras (Rim-Ghosts, Tamburin/Shaker, offene Hats, Clap-Layer, Tom-Fill in Takt 4).
  - **NEW ALL** schneidet frisch: CHOPS auf 16, MOVE zurück, neuer Schnitt im eingestellten MODE.
- **GENRA CHOP GRID:** ohne eigenen MIDI-Eingang nimmt REC den von GENRA CHOP.
- **GENRA CHOP FX** neu auf **36 HP**: acht Quellen einzeln rein (IN L/R, KICK, SNR, CH, OH, BASS, MEL L/R, PADS L/R)
  mit Wähler DRY / 1 / 2 / 1+2, jede einzeln wieder raus, dazu RET 1 L/R, RET 2 L/R und MIX L/R (für WING-LINK am X).
  OLED-Displays über den Reglern, CV darunter, Buchsen in zwei Reihen; acht DJ-Pads (halten = wirkt auf alle
  Ausgänge, Rechtsklick: jeder Effekt oder *Copy settings from BUS*), DJ FILTER, acht Schnelltasten; neue Effekte
  LOW CUT und HIGH CUT (39). 2.3.0-Patches mit GENRA CHOP FX laden die Kabel nicht mehr richtig.
- **GENRA CHOP ABLAUF:** die Hinweiszeile unter den Pads passt in ihr Feld; Pads per Drag & Drop in die Song-Liste
  (rote Linie zeigt die Stelle), Zeilen per Ziehen verschieben, Scrollbalken und Auto-Scroll am Rand.
- **Mac:** Rubber Band baut auch auf macOS (dort ohne `sincos`).
- **Fremder Code:** Rubber Band Library 4.0.0 (GPL-2.0-or-later) und Signalsmith Stretch / Linear (MIT) in `dep/`,
  Herkunft und Lizenzen in `licenses/`.

### GENRA CHOP 2.3.0
- **GENRA CHOP FX** (neu, 22 HP): zwei Effekt-Busse zum Live-Spielen nach Vorbild SP-404, 37 Effekte in sechs Gruppen
  (TEXTURE, FILTER, DRIVE, SPACE, MOD, PERFORM), CTRL 1–3 pro Bus, FX ON (antippen = an/aus, halten = solange
  gehalten), sechs frei belegbare Schnelltasten, 20 Presets, ROUTE 1>2 / 2>1 / parallel, Tempo von einer 16tel-Clock,
  CV, MIDI learn. Kabelloses Verbinden ist vorbereitet (noch nicht drin). Engine-Test `test/fx_test.cpp` im Build.
- **GENRA CHOP:** DUST ist weg, an seiner Stelle **CHOPS** (4 bis 64 Chops, ruht der Regler, wird neu geschnitten);
  Pads in **Banks A–D** (64 Chops, MIDI C1 = Pad 1 bis Pad 64); MODE nur noch Transient / Equal; **STOP und PLAY
  grösser** (14,6 × 8,4 mm). Alte 2.2.0-Patches laden (CHOPS startet dann bei 16).
- **Taster-Schrift** passt sich in allen drei bisherigen Modulen automatisch an (LOAD SAMPLE ragte hinaus).
- **GENRA CHOP ABLAUF:** VARIATION RENEWS weg (jeder Druck auf einen Teil = neues Pattern; Bass, Melodie, Pads neu,
  Drums und Chop vom Original); darunter **CLEAR** (dann ein Pad) und **CLEAR ALL** (zweimal), UNDO holt zurück.
- **GENRA CHOP GRID:** schwarze Tasten links schwarz; das Chop-Raster hat so viele Zeilen wie Chops (mind. 16).

### GENRA CHOP 2.2.0
- **Umbenannt:** GENRA HipHop heisst **GENRA CHOP** (Slug `GenraChop`, Ordner `genra-chop/`), der Grid-Editor
  **GENRA CHOP GRID**. Alte Test-Patches laden nicht mehr.
- **MOVE** (Startpunkt des zuletzt angetippten Chops), **CLEAR** (ganzes Chop-Pattern; halten + Pad = nur dieses Pad),
  **LOAD SAMPLE** gross und gelb neben dem Wellenform-Fenster, der NEW der Chop-Spur ist weg.
- **MIDI learn** für alle Regler, Taster und Felder; pro Controller und Computer gespeichert, bis zu drei Controller,
  Encoder-Erkennung, Export/Import.
- **GENRA CHOP ABLAUF** (neu, 26 HP): 64 Pattern-Pads, Varianten INTRO bis OUTRO, Song-Liste, MAKE SONG in sieben
  Hip-Hop-Formen, Übergänge nach Stil, Song-MIDI-Export.
- Fehler: KEY von Hand würfelte nur den Bass neu statt Bass, Melodie und Pads.
