// SPDX-License-Identifier: GPL-3.0-only
// GENRA CHOP BREAK (2.7.0): the break's pattern and its own break (see Break.hpp).
#include "Break.hpp"
#include <algorithm>
#include <cmath>

namespace hh {

const char* const BRK_PLAY_NAMES[NUM_BRK_PLAY] = {"Hits", "Loop", "Chop"};
const char* const BRK_PLAY_TEXT[NUM_BRK_PLAY] = {
	"Hits (the drums of GENRA CHOP / the trigger jacks play the chops)",
	"Loop (the break runs in time, mangled by the knobs)",
	"Chop (its own pattern: the chops where they sit in the break)",
};
const char* const BRK_MIX_NAMES[NUM_BRK_MIX] = {"Layer with kit", "Replace kit"};
const char* const RATE_NAMES[NUM_RATES] = {"1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T", "1/64"};
const float RATE_STEPS[NUM_RATES] = {2.f, 4.f / 3.f, 1.f, 2.f / 3.f, 0.5f, 1.f / 3.f, 0.25f};

void BrkPat::clear() {
	for (int s = 0; s < LEN; s++) {
		ev[s] = BrkEv();
		for (int d = 0; d < NUM_DICE; d++)
			dice[s][d] = 1.f;
		alt[s] = 0;
		semis[s] = 0;
		rat[s] = 1;
	}
}

int brkVoiceOf(const int* voicePad, int pad) {
	if (!voicePad || pad < 0)
		return -1;
	for (int v = 0; v < DRUMS; v++)
		if (voicePad[v] == pad)
			return v;
	return -1;
}

/** low 0, middle 1, high 2: by the voice, else by the chop's brightness among the others */
static int groupOf(const ChopSet& cs, const int* voicePad, int pad) {
	int v = brkVoiceOf(voicePad, pad);
	if (v == KICK || v == TOM)
		return 0;
	if (v == SNARE || v == CLAP || v == RIM)
		return 1;
	if (v >= 0)
		return 2;
	int n = std::min(cs.info.count, BRK_PADS), below = 0;
	for (int i = 0; i < n; i++)
		below += cs.info.chop[i].centroid < cs.info.chop[pad].centroid;
	return n > 0 ? std::min(2, below * 3 / n) : 1;
}

int brkSwap(const ChopSet& cs, const int* voicePad, int pad, int r) {
	int n = std::min(cs.info.count, BRK_PADS);
	if (n < 2 || pad < 0 || pad >= n)
		return pad;
	int g = groupOf(cs, voicePad, pad);
	int c[BRK_PADS], k = 0;
	for (int i = 0; i < n; i++)
		if (i != pad && groupOf(cs, voicePad, i) == g)
			c[k++] = i;
	if (!k)
		for (int i = 0; i < n; i++)
			if (i != pad)
				c[k++] = i;
	return c[((r % k) + k) % k];
}

void brkAssign(const ChopSet& cs, int* vp) {
	for (int v = 0; v < DRUMS; v++)
		vp[v] = -1;
	int n = std::min(cs.info.count, BRK_PADS);
	if (n <= 0)
		return;
	const SampleData& d = cs.data;
	float sr = d.sampleRate > 0.f ? d.sampleRate : 44100.f;
	float a = 1.f - std::exp(-2.f * (float) M_PI * 150.f / sr);
	struct F {
		float low, high, peak, decay;
	} f[BRK_PADS];
	float maxPeak = 0.f;
	for (int i = 0; i < n; i++) {
		size_t s0 = cs.cutStart[i], s1 = std::min(cs.cutEnd[i], s0 + (size_t) (0.12f * sr));
		float lp = 0.f, prev = 0.f, eAll = 1e-9f, eLow = 0.f, eHigh = 0.f, pk = 0.f, e0 = 0.f, e1 = 0.f;
		size_t mid = s0 + (s1 - s0) / 2;
		for (size_t k = s0; k < s1; k++) {
			float x = d.R.empty() ? d.L[k] : 0.5f * (d.L[k] + d.R[k]);
			lp += a * (x - lp);
			float hp = x - prev;
			prev = x;
			eAll += x * x;
			eLow += lp * lp;
			eHigh += hp * hp;
			pk = std::max(pk, std::fabs(x));
			(k < mid ? e0 : e1) += x * x;
		}
		f[i] = {eLow / eAll, eHigh / eAll, pk, e1 / std::max(1e-9f, e0)};
		maxPeak = std::max(maxPeak, pk);
	}
	bool used[BRK_PADS] = {};
	auto pick = [&](float (*score)(const F&, float), float minScore) {
		int best = -1;
		float bs = minScore;
		for (int i = 0; i < n; i++)
			if (!used[i]) {
				float sc = score(f[i], maxPeak);
				if (sc > bs)
					bs = sc, best = i;
			}
		if (best >= 0)
			used[best] = true;
		return best;
	};
	// kick: the most low end of the loud ones; then the tom: the next low one
	vp[KICK] = pick([](const F& x, float mp) { return x.peak > 0.35f * mp ? x.low * (0.5f + x.peak / mp) : -1.f; }, 0.05f);
	vp[SNARE] = pick([](const F& x, float mp) { return x.peak > 0.45f * mp ? (x.peak / mp) * (1.f - x.low) * (0.3f + std::min(1.f, x.high * 2.f)) : -1.f; }, 0.f);
	vp[CH] = pick([](const F& x, float mp) { return x.high * (1.f - x.low) * (x.decay < 0.5f ? 1.f : 0.5f) * (x.peak < 0.6f * mp ? 1.f : 0.6f); }, 0.05f);
	vp[OH] = pick([](const F& x, float mp) { return x.high * (1.f - x.low) * (0.3f + std::min(1.f, x.decay)); }, 0.05f);
	vp[TOM] = pick([](const F& x, float mp) { return x.low > 0.25f ? x.low * x.peak / mp : -1.f; }, 0.f);
	vp[CLAP] = pick([](const F& x, float mp) { return x.peak > 0.4f * mp ? (x.peak / mp) * (1.f - x.low) : -1.f; }, 0.f);
	vp[RIM] = pick([](const F& x, float mp) { return (1.f - x.low) * (1.f - x.peak / mp) * (1.f - std::min(1.f, x.high)); }, 0.f);
	vp[PERC] = pick([](const F& x, float mp) { return x.high + 0.01f; }, 0.f);
}

int brkBars(const ChopSet& cs) {
	double sr = cs.data.sampleRate > 0.f ? cs.data.sampleRate : 44100.0;
	double secs = cs.data.frames() / sr / (cs.warpRatio > 0.0 ? cs.warpRatio : 1.0); // the original's length
	float bpm = cs.bpm > 0.f ? cs.bpm : 96.f;
	double bars = secs * bpm / 240.0;
	int b = 1;
	for (int c : {1, 2, 4, 8})
		if (std::fabs(c - bars) < std::fabs(b - bars))
			b = c;
	return b;
}

void brkGenerate(BrkPat& p, const ChopSet& cs, int bars, const int* voicePad, uint32_t seed) {
	p.clear();
	Rng rng;
	rng.seed(seed * 2246822519u + 77u);
	int n = std::min(cs.info.count, BRK_PADS);
	size_t frames = cs.data.frames();
	if (n > 0 && frames > 0) {
		int total = std::max(1, bars) * STEPS;
		// the chops where they sit in the break (a shorter break comes round again; a longer one: its first 4 bars)
		int at[BRK_PADS];
		for (int i = 0; i < n; i++)
			at[i] = (int) std::lround((double) cs.cutStart[i] / frames * total);
		for (int i = 0; i < n; i++) {
			int nxt = i + 1 < n ? at[i + 1] : total;
			float len = (float) std::max(1, nxt - at[i]);
			for (int s = at[i]; s < LEN; s += total) {
				if (s < 0 || p.ev[s].pad >= 0)
					continue;
				p.ev[s].pad = (int8_t) i;
				p.ev[s].len = len;
				p.ev[s].vel = rng.range(0.85f, 1.f);
			}
		}
		// bars 2 and 4 a little varied: a beat repeated, two beats swapped, the last beat a stutter of the one before
		for (int b = 1; b < BARS; b += 2) {
			if (!rng.chance(0.6f))
				continue;
			int b0 = b * STEPS;
			BrkEv bar[STEPS];
			for (int i = 0; i < STEPS; i++)
				bar[i] = p.ev[b0 + i];
			switch (rng.below(3)) {
				case 0: // beat 3 again on beat 4
					for (int i = 0; i < 4; i++)
						p.ev[b0 + 12 + i] = bar[8 + i];
					break;
				case 1: // beats 2 and 4 swapped
					for (int i = 0; i < 4; i++) {
						p.ev[b0 + 4 + i] = bar[12 + i];
						p.ev[b0 + 12 + i] = bar[4 + i];
					}
					break;
				default: // the first hit of beat 4 on all its 16ths
				{
					int f = -1;
					for (int i = 12; i < 16 && f < 0; i++)
						if (bar[i].pad >= 0)
							f = i;
					if (f >= 0)
						for (int i = 12; i < 16; i++) {
							p.ev[b0 + i] = bar[f];
							p.ev[b0 + i].len = 1.f;
							p.ev[b0 + i].vel = bar[f].vel * (0.7f + 0.1f * (i - 12));
						}
					break;
				}
			}
		}
	}
	// the dice (the bar's one is never shuffled: the groove keeps its feet)
	static const int8_t SEMIS[] = {-12, -7, -5, -3, 3, 5, 7, 12};
	for (int s = 0; s < LEN; s++) {
		for (int d = 0; d < NUM_DICE; d++)
			p.dice[s][d] = rng.uni();
		if (s % STEPS == 0)
			p.dice[s][D_SHUFFLE] = 1.f;
		p.alt[s] = (int8_t) rng.below(127);
		p.semis[s] = SEMIS[rng.below(8)];
		p.rat[s] = (uint8_t) (2 + rng.below(3));
	}
	(void) voicePad;
}

// ---------------------------------------------------------------- its own break
namespace {
struct Noise {
	uint32_t s = 22222u;
	float next() {
		s ^= s << 13;
		s ^= s >> 17;
		s ^= s << 5;
		return (s >> 8) * (2.f / 16777216.f) - 1.f;
	}
};
}

void synthBreak(SampleData& out, float sr) {
	const float bpm = 96.f;
	const int bars = 2;
	double stepSec = 60.0 / bpm / 4.0;
	size_t n = (size_t) (bars * STEPS * stepSec * sr);
	out = SampleData();
	out.sampleRate = sr;
	out.name = "GENRA FUNK BREAK";
	out.path = "";
	out.L.assign(n, 0.f);
	out.R.assign(n, 0.f);
	Noise nz;
	// two bars: K kick, S snare, g ghost snare, h closed hat, o open hat (a step each)
	static const char* const KICK_ = "x.........x.....x.x.......x..x..";
	static const char* const SNR_ = "....x..g.g..x..g....x..g.g..x.g.";
	static const char* const HAT_ = "h.h.h.h.h.h.h.o.h.h.h.h.h.h.h.o.";
	auto at = [&](int step) { return (size_t) (step * stepSec * sr); };
	auto kick = [&](size_t t0, float vel) {
		float ph = 0.f;
		size_t len = (size_t) (0.35f * sr);
		for (size_t i = 0; i < len && t0 + i < n; i++) {
			float t = i / sr;
			float f = 48.f + 110.f * std::exp(-t * 38.f);
			ph += f / sr;
			float y = std::sin(2.f * (float) M_PI * ph) * std::exp(-t * 7.5f);
			y += 0.35f * nz.next() * std::exp(-t * 400.f); // the beater
			y = std::tanh(1.6f * y);
			out.L[t0 + i] += 0.75f * vel * y;
			out.R[t0 + i] += 0.75f * vel * y;
		}
	};
	auto snare = [&](size_t t0, float vel) {
		size_t len = (size_t) (0.25f * sr);
		float lp = 0.f, prev = 0.f;
		for (size_t i = 0; i < len && t0 + i < n; i++) {
			float t = i / sr;
			float w = nz.next();
			lp += 0.45f * (w - lp);       // a little darker
			float hp = lp - prev;         // and without the boom
			prev = lp;
			float body = std::sin(2.f * (float) M_PI * 185.f * t) * std::exp(-t * 30.f) +
			             0.5f * std::sin(2.f * (float) M_PI * 330.f * t) * std::exp(-t * 45.f);
			float y = 0.55f * body + 1.4f * hp * std::exp(-t * 16.f) + 0.25f * w * std::exp(-t * 60.f);
			y = std::tanh(1.3f * y);
			out.L[t0 + i] += 0.6f * vel * y;
			out.R[t0 + i] += 0.6f * vel * y;
		}
	};
	auto hat = [&](size_t t0, float vel, bool open) {
		size_t len = (size_t) ((open ? 0.3f : 0.06f) * sr);
		float prev = 0.f, prev2 = 0.f;
		float dec = open ? 9.f : 70.f;
		for (size_t i = 0; i < len && t0 + i < n; i++) {
			float t = i / sr;
			float w = nz.next();
			float hp = w - 2.f * prev + prev2; // bright
			prev2 = prev;
			prev = w;
			float y = 0.22f * hp * std::exp(-t * dec);
			out.L[t0 + i] += vel * y * 0.85f; // a little to the left
			out.R[t0 + i] += vel * y * 1.1f;
		}
	};
	for (int s = 0; s < bars * STEPS; s++) {
		// a little swing on the 16ths in between, as played
		size_t t0 = at(s) + ((s % 2) ? (size_t) (0.12 * stepSec * sr) : 0);
		if (KICK_[s] == 'x')
			kick(t0, s % 8 == 0 ? 1.f : 0.85f);
		if (SNR_[s] == 'x')
			snare(t0, 1.f);
		else if (SNR_[s] == 'g')
			snare(t0, 0.28f);
		if (HAT_[s] == 'h')
			hat(t0, s % 4 == 0 ? 0.9f : 0.6f, false);
		else if (HAT_[s] == 'o')
			hat(t0, 0.75f, true);
	}
	// a short room, so it sounds played, not programmed
	{
		const int D[3] = {(int) (0.011f * sr), (int) (0.017f * sr), (int) (0.023f * sr)};
		std::vector<float> l = out.L, r = out.R;
		for (size_t i = 0; i < n; i++)
			for (int k = 0; k < 3; k++) {
				// the left and the right side get other delays (a little width)
				size_t dl = (size_t) D[(k + 1) % 3], dr = (size_t) D[k];
				if (i >= dl)
					out.L[i] += 0.12f * l[i - dl];
				if (i >= dr)
					out.R[i] += 0.12f * r[i - dr];
			}
	}
	float mx = 0.f;
	for (size_t i = 0; i < n; i++)
		mx = std::max(mx, std::max(std::fabs(out.L[i]), std::fabs(out.R[i])));
	if (mx > 0.f)
		for (size_t i = 0; i < n; i++) {
			out.L[i] *= 0.89f / mx;
			out.R[i] *= 0.89f / mx;
		}
}

} // namespace hh
