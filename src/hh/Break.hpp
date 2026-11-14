// SPDX-License-Identifier: GPL-3.0-only
// GENRA CHOP BREAK (2.7.0): the break's pattern and its own break (no Rack in here: test/hh_test.cpp).
//
// The break is cut like a sample (hh/Chop: by its attacks, at most 16 chops) and every chop gets a drum voice (by its
// brightness, or by hand). It plays three ways:
//   HITS  every drum hit (GENRA CHOP's, or a trigger at the jacks) plays its voice's chop
//   LOOP  the break runs in time, sixteenth by sixteenth (a slice per step)
//   CHOP  its own pattern: the chops on the steps where they sit in the break
// The pattern keeps, per step, dice: whether SHUFFLE (another chop / another step), STUTTER (2-4 hits in the step),
// PITCH and REVERSE come in. The knobs say how much, live; NEW rolls the dice (and CHOP's variations) again.
#pragma once
#include "Chop.hpp"

namespace hh {

static const int BRK_PADS = 16; // chops at most (two rows of eight pads)
enum BrkPlay { BP_HITS, BP_LOOP, BP_CHOP, NUM_BRK_PLAY };
extern const char* const BRK_PLAY_NAMES[NUM_BRK_PLAY];
extern const char* const BRK_PLAY_TEXT[NUM_BRK_PLAY];
enum BrkMix { BM_LAYER, BM_REPLACE, NUM_BRK_MIX };
extern const char* const BRK_MIX_NAMES[NUM_BRK_MIX];
// ROLL's rates, in steps (16ths): 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/32T, 1/64
static const int NUM_RATES = 7;
extern const char* const RATE_NAMES[NUM_RATES];
extern const float RATE_STEPS[NUM_RATES];

struct BrkEv {
	int8_t pad = -1; // -1: none
	float len = 1.f; // steps
	float vel = 0.9f;
};
enum Dice { D_SHUFFLE, D_STUTTER, D_PITCH, D_REVERSE, NUM_DICE };
struct BrkPat {
	BrkEv ev[LEN];          // CHOP: the break's own pattern
	float dice[LEN][NUM_DICE]; // 0..1: a knob above it brings the thing in on this step (1: never)
	int8_t alt[LEN];        // SHUFFLE: CHOP / HITS: the chop that comes instead; LOOP: how many beats away (1..)
	int8_t semis[LEN];      // PITCH: semitones
	uint8_t rat[LEN];       // STUTTER: hits in the step (2..4)
	void clear();           // CLEAR: no events, no dice (the break plays plain)
	BrkPat() { clear(); }
};

/** NEW: the pattern (CHOP: the chops where they sit in a break of `bars` bars, bars 2 and 4 a little varied) and the
 *  dice, from `seed`. The chops' voices (`voicePad`, -1 none) choose what SHUFFLE may swap. */
void brkGenerate(BrkPat& p, const ChopSet& cs, int bars, const int* voicePad, uint32_t seed);
/** Which chop plays which drum voice (-1: none), from the sound of each hit: the lows (kick, tom), the bright and
 *  short ones (closed hat), bright and long (open hat), loud with little low end (snare, clap), the quieter snare-like
 *  ones (rim), what is left (perc). Better on breaks than the brightness alone. */
void brkAssign(const ChopSet& cs, int* voicePad);
/** The break's length in whole bars (1, 2, 4, 8) from its tempo (found) and length. */
int brkBars(const ChopSet& cs);
/** The voice (DrumVoice) a chop plays for, or -1. */
int brkVoiceOf(const int* voicePad, int pad);
/** SHUFFLE: the chop that comes instead of `pad` (`r` any number): one of the same kind (low: kick / tom, middle:
 *  snare / clap / rim, high: the hats and percussion), else any other. */
int brkSwap(const ChopSet& cs, const int* voicePad, int pad, int r);

/** GENRA CHOP BREAK's own break (when none is loaded): two bars of funk at 96 BPM, made here (no sample from anywhere,
 *  free of rights): kick, snare with ghost notes, closed and open hat. */
void synthBreak(SampleData& out, float sampleRate = 44100.f);

} // namespace hh
