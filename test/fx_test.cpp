// GENRA CHOP FX: every effect runs (on, off, ringing out, sleeping), stays finite and in range, and what it costs.
// g++ -std=c++11 -O2 -Isrc test/fx_test.cpp src/fx/FxEngine.cpp -o fxt && ./fxt
#include "fx/FxEngine.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

using namespace fx;
static int fails = 0;
#define CHECK(c, ...)                  \
	do {                                \
		if (!(c)) {                       \
			printf("FAIL: " __VA_ARGS__); \
			printf("\n");                 \
			fails++;                      \
		}                                 \
	} while (0)

int main() {
	const float sr = 48000.f;
	Tempo t;
	t.spb = sr * 60.f / 90.f;
	double total = 0.0;
	for (int f = 0; f < NUM_FX; f++) {
		Bus b;
		b.setSampleRate(sr);
		b.want = f;
		for (int k = 0; k < 3; k++)
			b.c[k] = FX[f].def[k];
		float peak = 0.f;
		bool finite = true;
		uint32_t r = 1;
		auto t1 = std::chrono::high_resolution_clock::now();
		int n = (int) (sr * 18);
		for (int i = 0; i < n; i++) {
			// a chord stab every half second, some noise
			float s = (i % 24000) < 12000 ? 0.4f * (std::sin(i * 0.02f) + 0.5f * std::sin(i * 0.031f)) : 0.f;
			r = r * 1664525u + 1013904223u;
			s += ((int32_t) r) * (0.02f / 2147483648.f);
			float L = s, R = s * 0.9f;
			b.on = i > (int) sr / 2 && i < (int) (sr * 3); // on for 2.5 s, then off
			if (i == (int) (sr * 2))
				b.c[0] = 1.f - b.c[0]; // a turn while it runs
			if (f == LOOPER && i == (int) (sr * 1.5))
				b.retrigger = true;
			b.process(L, R, t);
			t.barPos = std::fmod((double) i / (t.spb * 4.0), 1.0);
			if (!std::isfinite(L) || !std::isfinite(R))
				finite = false;
			peak = std::max(peak, std::max(std::fabs(L), std::fabs(R)));
		}
		auto t2 = std::chrono::high_resolution_clock::now();
		double us = std::chrono::duration<double, std::micro>(t2 - t1).count() / 18.0; // per second of audio
		total += us;
		CHECK(finite, "%s: not finite", FX[f].name);
		CHECK(peak < 4.f, "%s: peak %.2f", FX[f].name, peak);
		CHECK(!b.busy(), "%s: still awake 15 s after OFF", FX[f].name);
		printf("%-14s peak %.2f  %6.0f us per second (%.2f%% of a core)\n", FX[f].name, peak, us, us / 1e4);
	}
	// switching while it plays, every 50 ms through all effects
	{
		Bus b;
		b.setSampleRate(sr);
		b.on = true;
		bool finite = true;
		for (int i = 0; i < (int) (sr * 4); i++) {
			if (i % 2400 == 0)
				b.want = (i / 2400) % NUM_FX;
			float L = 0.3f * std::sin(i * 0.01f), R = L;
			b.process(L, R, t);
			if (!std::isfinite(L) || std::fabs(L) > 4.f)
				finite = false;
		}
		CHECK(finite, "switching: not finite / too loud");
	}
	// an idle bus costs (almost) nothing
	{
		Bus b;
		b.setSampleRate(sr);
		b.want = REVERB;
		auto t1 = std::chrono::high_resolution_clock::now();
		float L = 0.f, R = 0.f;
		for (int i = 0; i < (int) sr * 10; i++) {
			L = 0.1f, R = 0.1f;
			b.process(L, R, t);
		}
		auto t2 = std::chrono::high_resolution_clock::now();
		printf("idle bus: %.0f us per second\n", std::chrono::duration<double, std::micro>(t2 - t1).count() / 10.0);
	}
	// 2.6.0: how loud each effect is against the dry signal at its default settings (a beat with a chord); the
	// inserts within a few dB (the cuts and the stops are quieter on purpose)
	{
		auto sig = [&](int i, uint32_t& r, float& L, float& R) {
			float tt = i / sr, beat = 60.f / 90.f, tb = std::fmod(tt, beat);
			int bn = (int) (tt / beat) % 4;
			float k = std::exp(-tb * 18.f) * std::sin(2 * 3.14159f * 55.f * tb * (1.f + 2.f * std::exp(-tb * 40.f))) * 0.8f;
			r = r * 1664525u + 1013904223u;
			float nz = ((int32_t) r) / 2147483648.f;
			float sn = (bn == 1 || bn == 3) ? nz * std::exp(-tb * 25.f) * 0.5f : 0.f;
			float hh = nz * std::exp(-std::fmod(tt, beat / 2) * 120.f) * 0.15f;
			float pad = 0.12f * (std::sin(2 * 3.14159f * 220.f * tt) + std::sin(2 * 3.14159f * 261.6f * tt) +
			                     std::sin(2 * 3.14159f * 329.6f * tt));
			L = k + sn + hh + pad, R = k + 0.9f * sn + hh + pad;
		};
		int n = (int) (sr * 4);
		double dry = 0.0;
		{
			uint32_t r = 7;
			for (int i = (int) sr; i < n; i++) {
				float L, R;
				sig(i, r, L, R);
				dry += L * L + R * R;
			}
		}
		for (int f = 0; f < NUM_FX; f++) {
			Bus b;
			b.setSampleRate(sr);
			b.want = f;
			for (int k = 0; k < 3; k++)
				b.c[k] = FX[f].def[k];
			b.on = true;
			uint32_t r = 7;
			double e = 0.0;
			for (int i = 0; i < n; i++) {
				float L, R;
				sig(i, r, L, R);
				t.barPos = std::fmod(i / (t.spb * 4.0), 1.0);
				b.process(L, R, t);
				if (i >= (int) sr)
					e += L * L + R * R;
			}
			float db = 10.f * (float) std::log10(std::max(1e-12, e / dry));
			printf("%-14s %+5.1f dB against dry\n", FX[f].name, db);
			bool exempt = f == LOWCUT || f == TAPESTOP || f == BACKSPIN || FX[f].tail;
			if (!exempt)
				CHECK(std::fabs(db) < 6.5f, "%s: %+.1f dB against dry", FX[f].name, db);
		}
	}
	CHECK(NUM_PRESETS >= 16, "presets: %d", NUM_PRESETS);
	for (int p = 0; p < NUM_PRESETS; p++)
		CHECK(PRESETS[p].fx[0] >= 0 && PRESETS[p].fx[0] < NUM_FX && PRESETS[p].fx[1] >= 0 && PRESETS[p].fx[1] < NUM_FX,
		      "preset %d", p);
	printf(fails ? "%d FAILED\n" : "all good\n", fails);
	return fails ? 1 : 0;
}
