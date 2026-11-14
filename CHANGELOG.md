# GENRA CHOP – Versionsgeschichte

Was in welcher Version dazukam (neueste zuerst). Bis 2.4.0 lag GENRA CHOP im Repo von GENRA Test
(`Camillowup/testmodulesvcv`, Ordner `genra-chop/`, davor als GENRA HipHop).

### GENRA CHOP 2.7.0
- **GENRA CHOP BREAK (neu, 36 HP):** ein Drum-Break zum Reinladen und Verwursten. Schnitt an den Schlägen (bis 16
  Chops), jede Drum-Stimme aus dem Klang erkannt (oder von Hand: Stimme antippen, dann Pad); spielt auf die Drums von
  GENRA CHOP (ohne Kabel) oder die acht Trigger-Eingänge (**Hits**), als Loop im Takt (**Loop**) oder als eigenes
  Pattern (**Chop**); MIX: zum Kit dazu oder statt des Kits. MANGLE: PITCH, DECAY, STUTTER, SHUFFLE über die Würfel
  des Patterns, NEW, REVERSE. PATTERN: LOCK, UNDO, CLEAR, COPY, PASTE, SAVE, ROLL mit RATE (1/8 bis 1/64, Triolen).
  WARP (BEATS), TUNE, DRIVE, LEVEL, acht Einzelausgänge, MIX L/R, CV für Pitch, Stutter, Roll, NEW. NEW ALL und die
  Teile im ABLAUF würfeln mit (ausser mit LOCK). Eigener Break dabei (GENRA FUNK BREAK, im Modul erzeugt).
- **GENRA CHOP:**
  - **PAT** neben MODE: acht Arten von Chop-Pattern (AUTO, ORDER, STABS, CALL+RESP, HOLD, MELODY, HALF ORIG, SPARSE);
    eigene Chop-Rhythmen pro Stil-Familie, AUTO wählt nach Stil. ORDER spielt jeden Chop so lang, wie er ist (kein
    Zerhacken mehr, ausser bei den losen Stilen wie Dilla).
  - Die Chops folgen der Akkordfolge des Stils (pro Takt der passende Chop, bei pitchenden Stilen passend verstimmt);
    Stabs nehmen Chops mit klarem Akkord vor blossen Kurz-Schnipseln.
  - Schnitt genau vor dem Anschlag (Transient), 5 ms Fade am Chop-Ende (auch beim SAVE).
  - **DRUMS:** Groove statt KIT / BREAK / BOTH: STYLE, HALF, DOUBLE, LAID BACK, DRUNK, MINIMAL.
  - **Break entfernt** (BREAK-Regler, Buchsen BRK L/R, Laden im Menü): dafür GENRA CHOP BREAK. Alte Patches laden
    (der Break wird ignoriert, Kabel an BRK L/R fallen weg).
  - **LOOP:** bei laufendem Beat erst am Ende des Patterns an oder aus (der Taster blinkt bis dahin); die Chops
    verstummen sauber (auch schon geplante oder noch klingende).
- **GENRA CHOP ABLAUF:** die grüne LED überschneidet sich nicht mehr mit „LINK“.
- **GENRA CHOP FX:** „CHOP + BREAK“ nimmt den Break von GENRA CHOP BREAK.

### GENRA CHOP 2.6.0
- **GENRA CHOP:** Schneiden von Hand im Wellenform-Fenster: Chop-Linie ziehen (rastet an der leisesten Stelle ein),
  Doppelklick auf eine Linie = Chop weg, Doppelklick daneben = neuer Chop; Akkorde und Tonart neu, Pad-Nummern im
  Pattern rücken mit, UNDO nimmt es zurück, mit WARP wird das Original mitgeschnitten. Klick lädt nur noch ohne Sample.
- **Follow GENRA:** GENRA CHOP meldet sich am Link-Hub der GENRA-Plugins an (Kopie von `link/` aus dem GENRA-Repo) und
  legt seine Spuren im Ausgangs-Layout von GENRA ab; die GENRA-Instrumente (ab GENRA 2.39.0) können ihm folgen.
- **GENRA CHOP FX:** ohne Kabel von GENRA CHOP (Sound an IN: CHOP L/R oder CHOP + BREAK; Clock und Reset), Rechtsklick;
  gelbe Segmente über IN L/R, CLK, RST; Kabel gewinnt. Pegelausgleich pro Effekt (bei Grundeinstellung etwa so laut
  wie trocken, im FX-Test gemessen und geprüft); Presets „Spaced Out“ und „Robot Talk“ leiser.
- **Mac:** Build für Intel und Apple Silicon.

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
