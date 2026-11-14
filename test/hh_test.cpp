// GENRA CHOP without Rack: the generator for every style, and the analysis of a made-up sample (four chords
// played as stabs) - the chords and the key have to come out right. Build: g++ -std=c++11 -O2 -Isrc test/hh_test.cpp
// src/hh/Core.cpp src/hh/Styles.cpp src/hh/Chop.cpp src/hh/Song.cpp src/hh/Break.cpp -o hh_test
#include "hh/Chop.hpp"
#include "hh/Song.hpp"
#include "hh/Break.hpp"
#include <cstring>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <algorithm>

using namespace hh;

static int fails = 0;
#define CHECK(c, ...)                                                                                                  \
	do {                                                                                                               \
		if (!(c)) {                                                                                                    \
			printf("FAIL: " __VA_ARGS__);                                                                              \
			printf("\n");                                                                                              \
			fails++;                                                                                                   \
		}                                                                                                              \
	} while (0)

static void chordTone(SampleData& s, size_t at, size_t len, const int* notes, int n) {
	for (size_t i = 0; i < len && at + i < s.L.size(); i++) {
		float t = i / s.sampleRate;
		float env = std::exp(-t * 2.5f) * std::min(1.f, i / 40.f);
		float v = 0.f;
		for (int k = 0; k < n; k++) {
			float f = 440.f * std::pow(2.f, (notes[k] - 69) / 12.f);
			v += std::sin(2.f * (float) M_PI * f * t) + 0.3f * std::sin(4.f * (float) M_PI * f * t);
		}
		s.L[at + i] += 0.15f * env * v;
		s.R[at + i] += 0.15f * env * v;
	}
}

int main() {
	// ---- every style generates, renders, and stays in range
	for (int st = 0; st < NUM_STYLES; st++) {
		Settings set;
		set.style = st;
		SampleInfo none;
		PadSet pads[PADS];
		Pattern p;
		generate(p, -1, set, none, pads, 1234 + st, false, STYLES[st].bpm);
		Knobs k;
		Render r;
		render(p, k, r);
		int drums = 0, bass = 0, mel = 0, pad = 0;
		for (int s = 0; s < LEN; s++) {
			for (int v = 0; v < DRUMS; v++)
				drums += r.drum[v][s].vel > 0.f;
			bass += r.bass[s].pitch >= 0;
			mel += r.mel[s].pitch >= 0;
			pad += r.pad[s].pitch[0] >= 0;
			if (r.bass[s].pitch >= 0)
				CHECK(r.bass[s].pitch >= 20 && r.bass[s].pitch <= 70, "%s: bass note %d", STYLES[st].name, r.bass[s].pitch);
			if (r.mel[s].pitch >= 0)
				CHECK(r.mel[s].pitch >= 45 && r.mel[s].pitch <= 100, "%s: melody note %d", STYLES[st].name, r.mel[s].pitch);
		}
		if (st != 25) // Drumless
			CHECK(drums > 8, "%s: only %d drum hits", STYLES[st].name, drums);
		CHECK(bass > 0 && mel > 0 && pad > 0, "%s: empty track (bass %d mel %d pad %d)", STYLES[st].name, bass, mel, pad);
		// MIDI chop: all the way up still leaves something
		for (int t = 0; t < NUM_TRACKS; t++)
			k.cut[t] = 1.f;
		render(p, k, r);
		printf("%-22s %-6s bpm %3.0f  drums %3d bass %2d mel %2d pad %2d\n", STYLES[st].name, ERA_NAMES[STYLES[st].era],
		       STYLES[st].bpm, drums, bass, mel, pad);
	}

	// ---- a sample: Fm7 | Bbm7 | Db | C, two stabs each, 0.45 s apart
	ChopSet cs;
	cs.data.sampleRate = 44100.f;
	size_t step = (size_t) (0.45f * 44100.f);
	cs.data.L.assign(step * 8, 0.f);
	cs.data.R.assign(step * 8, 0.f);
	const int CH[4][4] = {{53, 56, 60, 63}, {58, 61, 65, 68}, {61, 65, 68, 73}, {60, 64, 67, 72}};
	for (int i = 0; i < 8; i++)
		chordTone(cs.data, i * step, step, CH[i / 2], 4);
	analyse(cs, CM_TRANSIENT);
	printf("chops %d, key %s%s (%.2f)\n", cs.info.count, NOTE_NAMES[cs.info.key.root], cs.info.key.minor ? "m" : "",
	       cs.info.keyConf);
	for (int i = 0; i < cs.info.count; i++) {
		char name[16];
		chordName(cs.info.chop[i].chord, name, sizeof(name));
		printf("  pad %2d  %5.2fs  %-6s conf %.2f kind %d\n", i + 1, cs.info.chop[i].seconds, name, cs.info.chop[i].conf,
		       cs.info.chop[i].kind);
	}
	CHECK(cs.info.count >= 8, "found %d chops, want at least 8", cs.info.count);
	// the chop at the start of each stab must have the stab's chord root
	const int ROOT[4] = {5, 10, 1, 0};
	for (int i = 0; i < 8; i++) {
		size_t at = i * step;
		int best = -1;
		for (int p = 0; p < cs.info.count; p++)
			if (cs.start[p] <= at + 2048 && cs.start[p] + 2048 >= at)
				best = p;
		CHECK(best >= 0, "no chop at stab %d", i);
		if (best >= 0)
			CHECK(cs.info.chop[best].chord.valid() && cs.info.chop[best].chord.root == ROOT[i / 2],
			      "stab %d: chord root %d, want %d", i, cs.info.chop[best].chord.root, ROOT[i / 2]);
	}
	CHECK(cs.info.key.root == 5 && cs.info.key.minor, "key %d%s, want F minor", cs.info.key.root, cs.info.key.minor ? "m" : "");
	// moving, deleting and adding a cut by hand (2.6.0): the count follows, the chords are found again
	{
		ChopSet e = cs;
		int n0 = e.info.count;
		std::vector<size_t> st(e.cutStart, e.cutStart + n0);
		st.erase(st.begin() + 3);
		recut(e, st);
		CHECK(e.info.count == n0 - 1, "delete: %d chops, want %d", e.info.count, n0 - 1);
		st.push_back(e.data.frames() / 2 + 4000); // between two cuts (a cut sits on every stab)
		recut(e, st);
		CHECK(e.info.count == n0, "insert: %d chops, want %d", e.info.count, n0);
		for (int i = 1; i < e.info.count; i++)
			CHECK(e.cutStart[i] > e.cutStart[i - 1] && e.cutEnd[i - 1] == e.cutStart[i], "recut: chops not in a row at %d", i);
		CHECK(e.cutEnd[e.info.count - 1] == e.data.frames(), "recut: last chop does not end at the end");
		size_t z = nearZero(e.data, e.data.frames() / 3);
		CHECK(z + 1000 > e.data.frames() / 3 && z < e.data.frames() / 3 + 1000, "nearZero too far away");
		CHECK(e.info.key.root == cs.info.key.root, "recut: key changed");
	}

	// ---- with the sample: chops sound, the bass follows them
	Settings set;
	set.style = 5;
	set.key = cs.info.key;
	PadSet pads[PADS];
	Pattern p;
	generate(p, -1, set, cs.info, pads, 99, false, 92.f);
	int chops = 0;
	for (int s = 0; s < LEN; s++)
		chops += p.chop[s].pad >= 0;
	CHECK(chops > 4, "only %d chop events", chops);
	for (int st = 0; st < NUM_STYLES; st++) {
		set.style = st;
		generate(p, -1, set, cs.info, pads, 7 + st, false, STYLES[st].bpm);
		mutateChops(p, set, cs.info, pads, 3, 1.f, STYLES[st].bpm);
	}
	// 2.7.0: every kind of chop pattern from every style: events in range, never empty, HOLD long, SPARSE thin
	for (int st = 0; st < NUM_STYLES; st++)
		for (int k = 0; k < NUM_CHOP_PATS; k++) {
			Settings s2 = set;
			s2.style = st;
			s2.chopPat = k;
			Pattern q;
			generate(q, -1, s2, cs.info, pads, 300 + st * 11 + k, false, STYLES[st].bpm);
			int n = 0;
			float longest = 0.f;
			for (int s = 0; s < LEN; s++) {
				const ChopEv& e = q.chop[s];
				if (e.pad < 0)
					continue;
				n++;
				longest = std::max(longest, e.len);
				CHECK(e.pad < cs.info.count && e.len > 0.f && e.len <= 16.f && e.pitch > -24 && e.pitch < 24,
				      "%s %s: event at %d out of range (pad %d len %.1f pitch %d)", STYLES[st].name, CHOP_PAT_NAMES[k], s, e.pad,
				      e.len, e.pitch);
			}
			CHECK(n >= 2, "%s %s: only %d chop events", STYLES[st].name, CHOP_PAT_NAMES[k], n);
			if (k == CP_HOLD)
				CHECK(longest >= 8.f, "%s HOLD: longest chop %.1f steps", STYLES[st].name, longest);
			if (k == CP_SPARSE)
				CHECK(n <= 3 * BARS, "%s SPARSE: %d events", STYLES[st].name, n);
		}
	// AUTO picks every kind somewhere, and not always the same one for a style
	{
		int seen[NUM_CHOP_PATS] = {};
		for (int st = 0; st < NUM_STYLES; st++)
			for (int i = 0; i < 20; i++)
				seen[autoChopPat(st, (i + 0.5f) / 20.f)]++;
		for (int k = 1; k < NUM_CHOP_PATS; k++)
			CHECK(seen[k] > 0, "AUTO never picks %s", CHOP_PAT_NAMES[k]);
		CHECK(seen[0] == 0, "AUTO picks AUTO");
	}
	// 2.7.0: the grooves: half-time puts the snare on the 3, minimal leaves only kick, snare, hat
	for (int st = 0; st < NUM_STYLES; st++)
		for (int g = 0; g < NUM_GROOVES; g++) {
			Settings s2;
			s2.style = st;
			s2.drumMode = g;
			Pattern q;
			generate(q, T_DRUMS, s2, SampleInfo(), pads, 77 + st, false, STYLES[st].bpm);
			for (int v = 0; v < DRUMS; v++)
				for (int s = 0; s < LEN; s++)
					CHECK(q.drum[v][s].vel <= 1.f && q.drum[v][s].off >= -0.46f && q.drum[v][s].off <= 0.46f,
					      "%s %s: hit out of range", STYLES[st].name, GROOVE_NAMES[g]);
			if (g == G_HALF && st != 25)
				for (int b = 0; b < BARS; b++)
					CHECK(q.drum[SNARE][b * STEPS + 8].vel > 0.f, "%s HALF: no snare on the 3 in bar %d", STYLES[st].name, b + 1);
			if (g == G_MINIMAL)
				for (int v : {CLAP, RIM, OH, TOM, PERC})
					for (int s = 0; s < LEN; s++)
						CHECK(q.drum[v][s].vel == 0.f, "%s MINIMAL: voice %d plays", STYLES[st].name, v);
			if (g == G_DRUNK)
				for (int s = 0; s < LEN; s++)
					CHECK(q.drum[KICK][s].off == 0.f, "%s DRUNK: the kick moved", STYLES[st].name);
		}

	// a lock keeps a pad's events through NEW
	set.style = 5;
	generate(p, T_CHOP, set, cs.info, pads, 5, false, 92.f);
	int lockPad = -1, lockStep = -1;
	for (int s = 0; s < LEN && lockPad < 0; s++)
		if (p.chop[s].pad >= 0)
			lockPad = p.chop[s].pad, lockStep = s;
	pads[lockPad].lock = true;
	generate(p, T_CHOP, set, cs.info, pads, 6, false, 92.f);
	CHECK(p.chop[lockStep].pad == lockPad, "locked pad %d gone from step %d", lockPad, lockStep);

	// ---- GENRA CHOP ABLAUF: every part from every style, every form, the bars of a song with transitions
	for (int st = 0; st < NUM_STYLES; st++) {
		set.style = st;
		Pattern src;
		generate(src, -1, set, cs.info, pads, 11 + st, false, STYLES[st].bpm);
		const SongForm& f = FORMS[autoForm(st)];
		CHECK(f.n > 0 && f.n <= 12, "form %d of style %d", autoForm(st), st);
		for (int part = 0; part < NUM_PARTS; part++)
			for (int renew = 0; renew < 32; renew += 7) {
				Pattern v;
				makePart(v, src, part, renew, set, cs.info, pads, 21 + part, STYLES[st].bpm);
				for (int k = 0; k < NUM_FORMS; k++) {
					int bars = FORMS[k].bars[part % FORMS[k].n];
					for (int b = 0; b < bars; b++) {
						Pattern q = v;
						shapeBar(q, part, b, bars, true, (part + 1) % NUM_PARTS, st, 3);
						for (int s = 0; s < LEN; s++) {
							CHECK(q.chop[s].pad < PADS, "chop pad out of range");
							for (int d = 0; d < DRUMS; d++)
								CHECK(q.drum[d][s].vel <= 1.f && q.drum[d][s].ratchet >= 1 && q.drum[d][s].ratchet <= 8,
								      "hit out of range (style %d part %d)", st, part);
							CHECK(q.bass[s].pitch < 128 && q.bass[s].len > 0.f, "bass out of range");
						}
						Render r;
						Knobs kn;
						render(q, kn, r);
					}
				}
			}
		// a BREAK has no kick; an INTRO's first bar of a long row has no bass
		Pattern br;
		makePart(br, src, P_BREAK, 0, set, cs.info, pads, 4, STYLES[st].bpm);
		int kicks = 0;
		for (int s = 0; s < LEN; s++)
			kicks += br.drum[KICK][s].vel > 0.f;
		CHECK(kicks == 0, "break with %d kicks (style %d)", kicks, st);
		Pattern in;
		makePart(in, src, P_INTRO, 0, set, cs.info, pads, 4, STYLES[st].bpm);
		shapeBar(in, P_INTRO, 0, 8, false, P_VERSE, st, 1);
		int bass = 0;
		for (int s = 0; s < STEPS; s++)
			bass += in.bass[s].pitch >= 0;
		CHECK(bass == 0, "intro bar 1 of 8 has bass (style %d)", st);
	}

	// ---- CHOPS: how many the sample is cut into (4..64), both modes
	{
		ChopSet c2;
		c2.data = cs.data;
		analyse(c2, CM_EQUAL, 40);
		CHECK(c2.info.count == 40, "equal 40: got %d chops", c2.info.count);
		analyse(c2, CM_TRANSIENT, 64);
		CHECK(c2.info.count >= 16 && c2.info.count <= 64, "transient 64: got %d chops", c2.info.count);
		analyse(c2, CM_TRANSIENT, 4);
		CHECK(c2.info.count == 4, "transient 4: got %d chops", c2.info.count);
		for (int i = 0; i + 1 < c2.info.count; i++)
			CHECK(c2.end[i] <= c2.start[i + 1] && c2.start[i] < c2.end[i], "chop %d out of order", i);
		printf("CHOPS: equal 40, transient 64 -> %d, transient 4\n", c2.info.count);
	}
	// ---- WARP: the tempo found, BEATS keeps every chop at its new place
	{
		printf("tempo found: %.2f BPM (stabs every 0.45 s = 133.33)\n", cs.bpm);
		CHECK(std::fabs(cs.bpm - 133.33f) < 1.5f, "tempo %.2f, want 133.33", cs.bpm);
		double ratio = cs.bpm / 92.0;
		SampleData w;
		warpBeats(cs, ratio, w);
		CHECK(std::llabs((long long) w.frames() - (long long) std::llround(cs.data.frames() * ratio)) <= 1, "beats length");
		ChopSet ws;
		makeWarped(cs, std::move(w), ratio, WARP_BEATS, ws);
		CHECK(ws.info.count == cs.info.count, "warped count");
		for (int i = 0; i < ws.info.count; i++)
			CHECK(std::llabs((long long) ws.cutStart[i] - std::llround(cs.cutStart[i] * ratio)) <= 1, "warped start %d", i);
		printf("WARP: %d chops at %.3f, mode %s\n", ws.info.count, ratio, WARP_MODES[autoWarp(cs)]);
	}
	// ---- WAV out and back in
	std::vector<uint8_t> w = writeWav(cs.data, cs.start[0], cs.end[0], true, 3);
	SampleData back;
	std::string err;
	CHECK(readWav(w, back, err), "written chop does not read back: %s", err.c_str());
	// ---- GENRA CHOP BREAK (2.7.0): its own break is cut into hits, the kick is a low chop at the start, the pattern
	// puts the chops where they sit
	{
		ChopSet b;
		synthBreak(b.data, 44100.f);
		CHECK(b.data.frames() > 44100 * 4 && b.data.frames() < 44100 * 6, "own break: %d frames", (int) b.data.frames());
		float mx = 0.f;
		for (size_t i = 0; i < b.data.frames(); i++)
			mx = std::max(mx, std::fabs(b.data.L[i]));
		CHECK(mx > 0.5f && mx <= 0.9f, "own break: peak %.2f", mx);
		analyse(b, CM_TRANSIENT, BRK_PADS);
		brkAssign(b, b.voicePad); // by the sound of each hit (the module does so)
		printf("own break: %d chops, %.1f BPM, %d bars, kick on chop %d, snare on chop %d\n", b.info.count, b.bpm, brkBars(b),
		       b.voicePad[KICK] + 1, b.voicePad[SNARE] + 1);
		CHECK(b.info.count >= 10 && b.info.count <= BRK_PADS, "own break: %d chops", b.info.count);
		CHECK(std::fabs(b.bpm - 96.f) < 2.f, "own break: tempo %.1f, want 96", b.bpm);
		CHECK(brkBars(b) == 2, "own break: %d bars, want 2", brkBars(b));
		CHECK(b.voicePad[KICK] >= 0 && b.voicePad[SNARE] >= 0 && b.voicePad[hh::CH] >= 0, "own break: voices missing");
		for (int v = 0; v < DRUMS; v++)
			for (int w = v + 1; w < DRUMS; w++)
				CHECK(b.voicePad[v] < 0 || b.voicePad[v] != b.voicePad[w], "own break: voices %d and %d on one chop", v, w);
		{
			// the snare chop starts on a snare step of the made-up break (beats 2 and 4)
			double sps = 44100.0 * 60.0 / 96.0 / 4.0;
			int st = (int) std::lround(b.cutStart[b.voicePad[SNARE]] / sps);
			CHECK(st % 8 == 4, "own break: the snare chop sits on step %d", st + 1);
		}
		CHECK(b.info.chop[b.voicePad[KICK]].centroid < b.info.chop[b.voicePad[SNARE]].centroid, "own break: kick brighter than snare");
		BrkPat bp;
		brkGenerate(bp, b, brkBars(b), b.voicePad, 5);
		int ev = 0;
		for (int s = 0; s < LEN; s++)
			if (bp.ev[s].pad >= 0) {
				ev++;
				CHECK(bp.ev[s].pad < b.info.count && bp.ev[s].len >= 1.f, "break pattern: bad event at %d", s);
			}
		CHECK(ev >= 2 * b.info.count, "break pattern: %d events for %d chops over 4 bars", ev, b.info.count);
		CHECK(bp.ev[0].pad == 0, "break pattern: step 1 plays chop %d", bp.ev[0].pad + 1);
		for (int s = 0; s < LEN; s += STEPS)
			CHECK(bp.dice[s][D_SHUFFLE] >= 1.f, "break pattern: the one of bar %d may be shuffled", s / STEPS + 1);
		for (int p2 = 0; p2 < b.info.count; p2++)
			for (int r = 0; r < 20; r++) {
				int q = brkSwap(b, b.voicePad, p2, r);
				CHECK(q >= 0 && q < b.info.count && q != p2, "brkSwap(%d) = %d", p2, q);
			}
		bp.clear();
		for (int s = 0; s < LEN; s++)
			CHECK(bp.ev[s].pad < 0 && bp.dice[s][D_STUTTER] >= 1.f, "break pattern: CLEAR left something");
	}

	printf(fails ? "%d FAILED\n" : "all good\n", fails);
	return fails ? 1 : 0;
}
