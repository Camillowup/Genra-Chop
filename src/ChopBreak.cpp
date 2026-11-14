// SPDX-License-Identifier: GPL-3.0-only
// GENRA CHOP BREAK (2.7.0): a drum break, cut into its hits (at most 16), every hit a drum voice; played on the drums of
// GENRA CHOP next to it (or on the eight trigger jacks), as a loop in time, or as its own pattern - and mangled:
// SHUFFLE, STUTTER, PITCH, REVERSE by the pattern's dice (NEW rolls them), DECAY, ROLL in a rate, TUNE and DRIVE.
// Without a break of your own it plays its own (hh/Break: made here, free of rights).
//
// The link to GENRA CHOP (no cables): it reads the hits GENRA CHOP fires (counters per voice), its step and tempo, NEW
// ALL and ABLAUF's parts (NEW, unless LOCK); it can silence the kit's triggers for the voices it plays (MIX: replace)
// and hands its mix over for CHOP FX ("CHOP + BREAK"). Its heartbeat tells GENRA CHOP it is still there.
#include "GenraChopModule.hpp"
#include "ChopUI.hpp"
#include "ChopBreakLayout.hpp"
#include "hh/Break.hpp"

// the voices on the panel: GENRA CHOP's order of the drum jacks (DRUM_AT), their colours
static const char* const VNAME[DRUMS] = {"KICK", "SNR", "CH", "OH", "CLAP", "RIM", "TOM", "PERC"};
static const NVGcolor VCOL[DRUMS] = {nvgRGB(0xc8, 0x31, 0x2b), nvgRGB(0xd6, 0xa6, 0x40), nvgRGB(0x9f, 0xd3, 0xff),
                                     nvgRGB(0x7f, 0xb3, 0xd8), nvgRGB(0xe0, 0x8a, 0x5a), nvgRGB(0xb4, 0x8a, 0xd8),
                                     nvgRGB(0x8f, 0xbf, 0x6a), nvgRGB(0xd8, 0x8a, 0xb4)};
/** The panel's column of a voice (DrumVoice). */
static int colOf(int voice) { return drumPos(voice); }

// COPY / PASTE: one clipboard for every GENRA CHOP BREAK in the patch
struct BrkClip {
	std::mutex m;
	BrkPat pat;
	bool has = false;
	static BrkClip& get() {
		static BrkClip c;
		return c;
	}
};

/** Its own break, cut and analysed (worker thread), WARP if asked. */
static void startOwn(std::shared_ptr<Loader> L, int mode, int count, WarpCfg w, bool keepPattern) {
	L->busy = true;
	L->keepPattern = keepPattern;
	L->keepMove = false;
	L->editKind = Loader::EDIT_NONE;
	std::thread([L, mode, count, w]() {
		ChopSet* cs = new ChopSet();
		synthBreak(cs->data, 44100.f);
		analyse(*cs, mode, count);
		finishSet(L, cs, w);
		L->busy = false;
	}).detach();
}

struct ChopBreak : Module {
	enum ParamId {
		TUNE_PARAM,
		DRIVE_PARAM,
		LEVEL_PARAM,
		CHOPS_PARAM,
		ENUMS(VLVL_PARAM, 8), // by the panel's column
		PITCH_PARAM,
		DECAY_PARAM,
		STUTTER_PARAM,
		SHUFFLE_PARAM,
		RATE_PARAM,
		NUM_PARAMS
	};
	enum InputId { CLK_INPUT, RST_INPUT, ENUMS(TRIG_INPUT, 8), PITCH_INPUT, STUT_INPUT, ROLL_INPUT, NEW_INPUT, NUM_INPUTS };
	enum OutputId { ENUMS(VOICE_OUTPUT, 8), MIXL_OUTPUT, MIXR_OUTPUT, NUM_OUTPUTS };

	// ---- saved
	std::string path;                 // the break ("": its own)
	int playMode = BP_HITS, mixMode = BM_LAYER, chopMode = CM_TRANSIENT;
	bool warpOn = false;
	float warpMul = 1.f;
	int voicePad[DRUMS] = {-1, -1, -1, -1, -1, -1, -1, -1}; // by DrumVoice
	uint32_t muteMask = 0;            // bit per DrumVoice
	std::atomic<bool> lock{false}, reverseOn{false};
	int64_t linkedId = -1;            // the GENRA CHOP (-1: automatic)

	// ---- the break (as in GENRA CHOP: a worker thread cuts it, the audio thread swaps it in)
	std::shared_ptr<Loader> loader = std::make_shared<Loader>();
	ChopSet* chops = nullptr;
	std::atomic<ChopSet*> incoming{nullptr};
	std::atomic<ChopSet*> trash[4];
	std::atomic<const ChopSet*> uiChops{nullptr};
	int cutCount = BRK_PADS;
	float warpFor = 0.f;
	bool warpPending = false;
	std::atomic<uint32_t> wantPath{0}; // the break wanted (a hash of its path; its own: ""): an older one that comes late goes
	bool started = false;              // a break is on its way (the patch's, or its own when the module is new)
	static uint32_t pathHash(const std::string& p) {
		uint32_t x = 2166136261u;
		for (unsigned char ch : p)
			x = (x ^ ch) * 16777619u;
		return x;
	}
	void want(const std::string& p) {
		wantPath = pathHash(p);
		started = true;
	}

	// ---- the pattern
	BrkPat pat;
	std::vector<BrkPat> undo;
	int undoN = 0, undoTop = 0;
	std::atomic<int> undoCount{0};
	std::mutex shareM;
	BrkPat shared, pasteBuf;          // under shareM: a copy for COPY; what PASTE brings
	std::atomic<bool> pasteIn{false};
	bool patDirty = true;
	std::atomic<bool> reqNew{false}, reqUndo{false}, reqClear{false};

	// ---- GENRA CHOP
	std::atomic<GenraChop*> host{nullptr};
	std::atomic<bool> hostChanged{true};
	GenraChop* h = nullptr;
	uint32_t hostEpoch = 0;
	uint32_t seenHit[DRUMS] = {};
	uint32_t seenNewAll = 0;
	int seenPart = -1;
	int lastHostStep = -1;
	std::atomic<bool> linkPub{false};

	// ---- the clock (own jack, else GENRA CHOP's)
	dsp::SchmittTrigger clkTrig, rstTrig, newTrig, trig[DRUMS];
	float clockGap = 0.f, stepPeriod = 5512.5f, sinceTick = 0.f;
	int step = -1;
	long lstep = -1;
	std::atomic<int> stepPub{-1};
	std::atomic<float> bpmPub{0.f};

	// ---- the voices: two per drum voice (a new hit cuts its voice's last), four without a voice, two for LOOP
	struct Shot {
		const ChopSet* cs = nullptr;
		double from = 0.0, to = 0.0, inc = 1.0;
		int voice = -1, pad = -1;
		float vel = 1.f;
		bool rev = false;
		int len = -1;
	};
	SVoice vv[DRUMS][2];
	int vvNext[DRUMS] = {};
	SVoice fv[4];
	int fvNext = 0;
	SVoice lv[2];
	int lvVoice[2] = {-1, -1};
	int lvNext = 0;
	int prevSrc = -1;
	bool prevPlain = false;
	struct Later {
		int delay;
		Shot s;
		bool loop;
	};
	Later later[64];
	int nLater = 0;
	Shot last;            // ROLL repeats it
	bool haveLast = false;
	std::atomic<bool> rollHeld{false};
	bool rolling = false;
	float rollPhase = 0.f;
	int playSeen = BP_HITS;

	// ---- for the panel
	std::atomic<int> padQ[32];
	std::atomic<int> padQw{0};
	int padQr = 0;
	std::atomic<int> lastPad{-1};
	std::atomic<uint32_t> lastPadTick{0};
	std::atomic<float> posPub{-1.f};
	const SVoice* posVoice = nullptr;
	uint32_t tick = 0;

	ChopBreak() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, 0);
		configParam(TUNE_PARAM, -12.f, 12.f, 0.f, "Tune", " semitones")->snapEnabled = true;
		configParam(DRIVE_PARAM, 0.f, 1.f, 0.f, "Drive", "%", 0.f, 100.f);
		configParam(LEVEL_PARAM, 0.f, 1.f, 0.8f, "Level", "%", 0.f, 100.f);
		configParam(CHOPS_PARAM, (float) MIN_CHOPS, (float) BRK_PADS, (float) BRK_PADS, "Chops (how many the break is cut into)")
		    ->snapEnabled = true;
		for (int c = 0; c < DRUMS; c++)
			configParam(VLVL_PARAM + c, 0.f, 1.f, 0.8f, string::f("%s level", VNAME[c]), "%", 0.f, 100.f);
		configParam(PITCH_PARAM, 0.f, 1.f, 0.f, "Pitch (how many hits are pitched)", "%", 0.f, 100.f);
		configParam(DECAY_PARAM, 0.f, 1.f, 1.f, "Decay (how long a chop rings)", "%", 0.f, 100.f);
		configParam(STUTTER_PARAM, 0.f, 1.f, 0.f, "Stutter (how many hits repeat in their step)", "%", 0.f, 100.f);
		configParam(SHUFFLE_PARAM, 0.f, 1.f, 0.f, "Shuffle (how many hits swap for another)", "%", 0.f, 100.f);
		configSwitch(RATE_PARAM, 0.f, (float) (NUM_RATES - 1), 2.f, "Roll rate",
		             {RATE_NAMES[0], RATE_NAMES[1], RATE_NAMES[2], RATE_NAMES[3], RATE_NAMES[4], RATE_NAMES[5], RATE_NAMES[6]});
		configInput(CLK_INPUT, "Clock (16ths; without a cable: GENRA CHOP's)");
		configInput(RST_INPUT, "Reset");
		for (int c = 0; c < DRUMS; c++) {
			configInput(TRIG_INPUT + c, string::f("%s trigger (voltage = velocity; without a cable: GENRA CHOP's)", VNAME[c]));
			configOutput(VOICE_OUTPUT + c, string::f("%s (mono)", VNAME[c]));
		}
		configInput(PITCH_INPUT, "Pitch (V/OCT, added to TUNE)");
		configInput(STUT_INPUT, "Stutter (gate: every hit stutters)");
		configInput(ROLL_INPUT, "Roll (gate, as the button)");
		configInput(NEW_INPUT, "New pattern (trigger)");
		configOutput(MIXL_OUTPUT, "Mix left");
		configOutput(MIXR_OUTPUT, "Mix right");
		undo.resize(16);
		for (auto& t : trash)
			t = nullptr;
		for (auto& q : padQ)
			q = -1;
		wantPath = pathHash("");
	}
	/** A new module (not from a patch): its own break. */
	void onAdd(const AddEvent& e) override {
		if (!started) {
			want("");
			startOwn(loader, chopMode, cutCount, WarpCfg(), false);
		}
	}
	~ChopBreak() {
		unlinkHost();
		delete chops;
		for (auto& t : trash)
			delete t.exchange(nullptr);
		delete incoming.exchange(nullptr);
	}
	void onRemove(const RemoveEvent& e) override { unlinkHost(); }
	/** The kit plays again and gets no mix from here (the engine is locked: the host cannot go meanwhile). */
	void unlinkHost() {
		GenraChop* c = host.load();
		if (c && ChopRegistry::get().alive(c)) {
			c->brkReplace = 0;
			c->brkMixL = 0.f;
			c->brkMixR = 0.f;
		}
	}

	void toTrash(ChopSet* c) {
		if (!c)
			return;
		for (auto& t : trash) {
			ChopSet* none = nullptr;
			if (t.compare_exchange_strong(none, c))
				return;
		}
	}
	int countNow() const { return chops ? std::min(chops->info.count, BRK_PADS) : 0; }
	int voiceOf(int pad) const { return brkVoiceOf(voicePad, pad); }
	bool muted(int voice) const { return voice >= 0 && (muteMask & (1u << voice)); }

	void pushUndo() {
		undo[undoTop] = pat;
		undoTop = (undoTop + 1) % (int) undo.size();
		undoN = std::min(undoN + 1, (int) undo.size());
		undoCount = undoN;
	}
	void newPattern() {
		if (!chops)
			return;
		bool anyMap = false;
		for (int v = 0; v < DRUMS; v++)
			anyMap = anyMap || voicePad[v] >= 0;
		if (!anyMap)
			brkAssign(*chops, voicePad); // after a reset: the voices from the sound again
		pushUndo();
		brkGenerate(pat, *chops, brkBars(*chops), voicePad, random::u32());
		patDirty = true;
	}

	// ---------------------------------------------------------------- playing
	/** The voices a new shot of `voice` cuts: its own (and the other hat: closed and open choke each other). */
	void choke(int voice, float sr) {
		auto rel = [&](int v) {
			for (SVoice& x : vv[v])
				if (x.active)
					x.release(sr);
		};
		rel(voice);
		if (voice == CH)
			rel(OH);
		if (voice == OH)
			rel(CH);
	}
	void fire(const Shot& s, float sr) {
		if (!s.cs)
			return;
		SVoice* v;
		if (s.voice >= 0) {
			choke(s.voice, sr);
			v = &vv[s.voice][vvNext[s.voice]];
			vvNext[s.voice] = (vvNext[s.voice] + 1) % 2;
		}
		else {
			for (SVoice& x : fv)
				if (x.active)
					x.release(sr);
			v = &fv[fvNext];
			fvNext = (fvNext + 1) % 4;
		}
		v->startRange(s.cs, s.from, s.to, s.vel, 0, s.rev, sr, s.len);
		v->inc = s.inc;
		posVoice = v;
		if (s.pad >= 0) {
			lastPad = s.pad;
			lastPadTick = tick;
		}
	}
	void fireLoop(const Shot& s, float sr) {
		for (SVoice& x : lv)
			if (x.active)
				x.release(sr);
		SVoice& v = lv[lvNext];
		lvVoice[lvNext] = s.voice;
		lvNext = (lvNext + 1) % 2;
		v.startRange(s.cs, s.from, s.to, s.vel, 0, s.rev, sr, s.len);
		v.inc = s.inc;
		posVoice = &v;
		if (s.pad >= 0) {
			lastPad = s.pad;
			lastPadTick = tick;
		}
	}
	/** A shot now, and its stutter: `rat` hits spread over the step. */
	void shoot(const Shot& s, int rat, bool loop, float sr) {
		if (loop)
			fireLoop(s, sr);
		else
			fire(s, sr);
		last = s;
		haveLast = true;
		for (int k = 1; k < rat && nLater < 64; k++) {
			Later& l = later[nLater++];
			l.delay = (int) (k * stepPeriod / rat);
			l.s = s;
			l.s.vel = s.vel * (1.f - 0.12f * k);
			l.loop = loop;
		}
	}

	float semisNow() {
		return params[TUNE_PARAM].getValue() + 12.f * inputs[PITCH_INPUT].getVoltage();
	}
	double incFor(float semis, float sr) const {
		return std::pow(2.0, semis / 12.0) * (chops ? chops->data.sampleRate : 44100.f) / sr;
	}
	/** The mangle of step s: what fires (the knobs over the dice). */
	struct Mang {
		bool shuffle = false, rev = false;
		int semis = 0, rat = 1;
		float decay = 1.f;
	};
	Mang mangleAt(int s) {
		Mang m;
		s = ((s % LEN) + LEN) % LEN;
		float sh = params[SHUFFLE_PARAM].getValue();
		float st = inputs[STUT_INPUT].getVoltage() >= 1.f ? 1.f : params[STUTTER_PARAM].getValue();
		float pi = params[PITCH_PARAM].getValue();
		m.shuffle = pat.dice[s][D_SHUFFLE] < sh * 0.6f;
		m.rat = pat.dice[s][D_STUTTER] < st * 0.5f ? pat.rat[s] : 1;
		m.semis = pat.dice[s][D_PITCH] < pi * 0.5f ? pat.semis[s] : 0;
		m.rev = reverseOn.load() && pat.dice[s][D_REVERSE] < 0.3f;
		m.decay = params[DECAY_PARAM].getValue();
		return m;
	}
	/** A chop as a shot (DECAY: 10..100 % of it). */
	Shot padShot(int pad, float vel, float semis, bool rev, float decay, float sr) {
		Shot s;
		s.cs = chops;
		s.pad = pad;
		s.voice = voiceOf(pad);
		s.from = (double) chops->start[pad];
		s.to = (double) chops->end[pad];
		s.vel = vel;
		s.rev = rev;
		s.inc = incFor(semis, sr);
		if (decay < 0.995f) {
			double frames = (s.to - s.from) * (0.1 + 0.9 * decay);
			s.len = std::max(1, (int) (frames / s.inc));
		}
		return s;
	}

	/** HITS: drum voice `voice` hit with `vel` on step s. */
	void hit(int voice, float vel, int s, float sr) {
		int pad = voicePad[voice];
		if (!chops || pad < 0 || pad >= countNow() || muted(voice) || rolling)
			return;
		Mang m = mangleAt(s);
		if (m.shuffle)
			pad = brkSwap(*chops, voicePad, pad, pat.alt[((s % LEN) + LEN) % LEN]);
		Shot sh = padShot(pad, vel, semisNow() + m.semis, m.rev, m.decay, sr);
		sh.voice = voice; // it plays for the voice that was hit (its jack, its level)
		shoot(sh, m.rat, false, sr);
	}

	/** CHOP: the pattern's event on step s. */
	void chopStep(int s, float sr) {
		const BrkEv& e = pat.ev[s];
		if (!chops || e.pad < 0 || e.pad >= countNow())
			return;
		int pad = e.pad;
		Mang m = mangleAt(s);
		if (m.shuffle)
			pad = brkSwap(*chops, voicePad, pad, pat.alt[s]);
		if (muted(voiceOf(pad)))
			return;
		Shot sh = padShot(pad, e.vel, semisNow() + m.semis, m.rev, 1.f, sr);
		// as long as the event (and DECAY), but never past the chop
		int stepsLen = (int) (e.len * stepPeriod * (0.1f + 0.9f * m.decay));
		sh.len = sh.len > 0 ? std::min(sh.len, stepsLen) : stepsLen;
		shoot(sh, m.rat, false, sr);
	}

	/** LOOP: the slice of step s (the break runs on, sixteenth by sixteenth; mangled steps start anew). */
	void loopStep(int s, float sr) {
		if (!chops || !chops->data.frames())
			return;
		int bars = brkBars(*chops);
		int total = bars * STEPS;
		double fps = (double) chops->data.frames() / total; // frames per step
		int src = (int) (((lstep % total) + total) % total);
		Mang m = mangleAt(s);
		bool plain = !m.shuffle && !m.rev && m.semis == 0 && m.rat == 1 && m.decay >= 0.995f;
		if (m.shuffle) {
			int beats = std::max(2, total / 4);
			src = (src + 4 * (1 + ((pat.alt[s] % (beats - 1)) + (beats - 1)) % (beats - 1))) % total;
		}
		double inc = fps / std::max(1.f, stepPeriod) * std::pow(2.0, (semisNow() + m.semis) / 12.0);
		int pad = 0;
		for (int i = 0; i < countNow(); i++)
			if ((double) chops->cutStart[i] <= src * fps + 1.0)
				pad = i;
		int voice = voiceOf(pad);
		SVoice& cur = lv[(lvNext + 1) % 2];
		// runs on only while it is where it should be (a quarter step off: it starts again, on the spot)
		bool onTime = cur.active && std::fabs(cur.pos - src * fps) < 0.25 * fps;
		if (plain && prevPlain && onTime && src == (prevSrc + 1) % total && !muted(voice)) {
			cur.inc = inc; // runs on (no click, no restart); only the tempo
			lvVoice[(lvNext + 1) % 2] = voice;
			prevSrc = src;
			return;
		}
		prevSrc = src;
		prevPlain = plain;
		if (muted(voice)) {
			for (SVoice& x : lv)
				if (x.active)
					x.release(sr);
			return;
		}
		Shot sh;
		sh.cs = chops;
		sh.pad = pad;
		sh.voice = voice;
		sh.from = src * fps;
		sh.to = plain ? (double) chops->data.frames() : std::min((double) chops->data.frames(), (src + 1) * fps + 0.004 * chops->data.sampleRate);
		sh.inc = inc;
		sh.rev = m.rev;
		sh.vel = 1.f;
		sh.len = plain ? -1 : std::max(1, (int) (stepPeriod / m.rat * (0.2f + 0.8f * m.decay)));
		shoot(sh, m.rat, true, sr);
	}

	void onTick(int s, float sr) {
		step = s;
		stepPub = s;
		sinceTick = 0.f;
		if (rolling)
			return;
		if (playMode == BP_LOOP)
			loopStep(s, sr);
		else if (playMode == BP_CHOP)
			chopStep(s, sr);
	}

	void stopAll(float sr) {
		for (SVoice& x : lv)
			if (x.active)
				x.release(sr);
		prevSrc = -1;
		prevPlain = false;
		nLater = 0;
	}

	void process(const ProcessArgs& args) override {
		float sr = args.sampleRate;
		tick++;
		// ---- a new break from the worker thread
		ChopSet* c = incoming.exchange(nullptr);
		if (c && pathHash(c->data.path) != wantPath.load()) {
			toTrash(c); // a break asked for before the one wanted now (e.g. its own, then the patch's)
			c = nullptr;
		}
		if (c) {
			for (auto& p : vv)
				for (SVoice& x : p)
					x.active = false;
			for (SVoice& x : fv)
				x.active = false;
			for (SVoice& x : lv)
				x.active = false;
			nLater = 0;
			haveLast = false;
			posVoice = nullptr;
			toTrash(chops);
			chops = c;
			uiChops = c;
			// the voices: kept through a patch's load and WARP, found from the sound for a new break or a new cut
			bool anyMap = false;
			for (int v = 0; v < DRUMS; v++)
				anyMap = anyMap || (voicePad[v] >= 0 && voicePad[v] < countNow());
			if (!(loader->keepPattern && anyMap))
				brkAssign(*c, voicePad);
			if (!loader->keepPattern) {
				pushUndo();
				brkGenerate(pat, *c, brkBars(*c), voicePad, random::u32());
			}
			patDirty = true;
		}
		// ---- GENRA CHOP still there?
		uint32_t ep = ChopRegistry::get().epoch.load();
		if (hostChanged.exchange(false) || ep != hostEpoch) {
			hostEpoch = ep;
			GenraChop* c = host.load();
			GenraChop* before = h;
			h = (c && ChopRegistry::get().alive(c)) ? c : nullptr;
			if (before && before != h && ChopRegistry::get().alive(before)) {
				before->brkReplace = 0;
				before->brkMixL = 0.f;
				before->brkMixR = 0.f;
			}
			if (h) {
				// start from what it did before we came
				for (int v = 0; v < DRUMS; v++)
					seenHit[v] = h->hitCount[v].load();
				seenNewAll = h->newAllCount.load();
				seenPart = h->songPart.load();
				lastHostStep = -1;
			}
		}
		linkPub = h != nullptr;
		if (playMode != playSeen) {
			playSeen = playMode; // another way to play: the loop still running stops
			stopAll(sr);
		}
		// ---- requests
		if (newTrig.process(inputs[NEW_INPUT].getVoltage(), 0.1f, 1.f))
			reqNew = true;
		if (h) {
			uint32_t na = h->newAllCount.load();
			int part = h->songPart.load();
			if (na != seenNewAll) {
				seenNewAll = na;
				if (!lock)
					reqNew = true;
			}
			if (part != seenPart) {
				seenPart = part;
				if (!lock && part >= 0)
					reqNew = true;
			}
		}
		if (reqNew.exchange(false))
			newPattern();
		if (reqClear.exchange(false)) {
			pushUndo();
			pat.clear();
			patDirty = true;
		}
		if (reqUndo.exchange(false) && undoN > 0) {
			undoTop = (undoTop + (int) undo.size() - 1) % (int) undo.size();
			undoN--;
			undoCount = undoN;
			pat = undo[undoTop];
			patDirty = true;
		}
		if ((tick & 255) == 0) {
			if (pasteIn.load()) {
				if (shareM.try_lock()) {
					pushUndo();
					pat = pasteBuf;
					pasteIn = false;
					patDirty = true;
					shareM.unlock();
				}
			}
			else if (patDirty && shareM.try_lock()) {
				shared = pat;
				patDirty = false;
				shareM.unlock();
			}
		}
		// ---- the clock: the jack, else GENRA CHOP's steps
		if (rstTrig.process(inputs[RST_INPUT].getVoltage(), 0.1f, 1.f)) {
			step = -1;
			lstep = -1;
			stopAll(sr);
		}
		sinceTick += 1.f;
		if (inputs[CLK_INPUT].isConnected()) {
			clockGap += 1.f;
			if (clkTrig.process(inputs[CLK_INPUT].getVoltage(), 0.1f, 1.f)) {
				if (clockGap > 1.f && clockGap < 4.f * sr)
					stepPeriod = clockGap;
				clockGap = 0.f;
				lstep++;
				onTick((step + 1) % LEN, sr);
			}
		}
		else if (h) {
			int hs = h->step.load();
			if (hs != lastHostStep) {
				if (hs >= 0) {
					stepPeriod = std::max(1.f, h->stepPeriod);
					bool jumped = lastHostStep < 0 || !(hs == lastHostStep + 1 || (hs == 0 && lastHostStep == LEN - 1));
					lstep = jumped ? hs : lstep + 1;
					onTick(hs, sr);
				}
				else {
					step = -1;
					stepPub = -1;
					stopAll(sr);
				}
				lastHostStep = hs;
			}
		}
		else if (step >= 0 && sinceTick > 4.f * sr) {
			step = -1; // no clock any more
			stepPub = -1;
			stopAll(sr);
		}
		if ((tick & 1023) == 0)
			bpmPub = (step >= 0 || h) ? 60.f * sr / (std::max(1.f, stepPeriod) * 4.f) : 0.f;
		// ---- the hits: a cable at a trigger jack, else GENRA CHOP's drums (HITS only)
		int sNow = step >= 0 ? step : 0;
		for (int c = 0; c < DRUMS; c++) {
			int v = DRUM_AT[c];
			if (inputs[TRIG_INPUT + c].isConnected()) {
				float x = inputs[TRIG_INPUT + c].getVoltage();
				if (trig[c].process(x, 0.1f, 1.f) && playMode == BP_HITS)
					hit(v, clamp(x / 10.f, 0.1f, 1.f), sNow, sr);
			}
			else if (h) {
				uint32_t n = h->hitCount[v].load();
				if (n != seenHit[v]) {
					seenHit[v] = n;
					if (playMode == BP_HITS)
						hit(v, h->hitVel[v].load(), h->hitStep[v].load(), sr);
				}
			}
		}
		// ---- the pads by hand
		while (true) {
			int w = padQw.load();
			if (padQr == w)
				break;
			int q = padQ[padQr % 32].load();
			padQr++;
			if (q >= 0 && chops && q < countNow()) {
				Shot sh = padShot(q, 0.9f, semisNow(), false, params[DECAY_PARAM].getValue(), sr);
				shoot(sh, 1, false, sr);
			}
		}
		// ---- ROLL: held, it repeats the last shot in the rate (the pattern and the hits rest meanwhile)
		bool rh = rollHeld.load() || inputs[ROLL_INPUT].getVoltage() >= 1.f;
		if (rh && !rolling) {
			rolling = true;
			rollPhase = 0.f;
			// LOOP: the slice of now, as a slice of one step
			if (playMode == BP_LOOP && haveLast) {
				last.to = std::min(last.to, last.from + std::max(1.0, (double) stepPeriod * last.inc));
				last.len = -1;
			}
		}
		else if (!rh && rolling)
			rolling = false;
		if (rolling && haveLast) {
			float every = std::max(16.f, stepPeriod * RATE_STEPS[clamp((int) std::round(params[RATE_PARAM].getValue()), 0, NUM_RATES - 1)]);
			if (rollPhase <= 0.f) {
				Shot s = last;
				s.len = std::max(1, (int) (every * 0.9f));
				s.vel = 0.85f * last.vel;
				if (playMode == BP_LOOP)
					fireLoop(s, sr);
				else
					fire(s, sr);
				rollPhase += every;
			}
			rollPhase -= 1.f;
		}
		// ---- stutters due
		for (int i = 0; i < nLater;) {
			if (--later[i].delay <= 0) {
				Later l = later[i];
				later[i] = later[--nLater];
				if (!rolling) {
					if (l.loop)
						fireLoop(l.s, sr);
					else
						fire(l.s, sr);
				}
			}
			else
				i++;
		}
		// ---- the sound: every voice to its jack (mono) and the mix
		float vo[DRUMS] = {};
		float L = 0.f, R = 0.f;
		for (int v = 0; v < DRUMS; v++) {
			float l = 0.f, r = 0.f;
			for (SVoice& x : vv[v])
				x.next(sr, l, r);
			float g = params[VLVL_PARAM + colOf(v)].getValue();
			vo[v] += 0.5f * (l + r) * g;
			L += l * g;
			R += r * g;
		}
		for (int k = 0; k < 2; k++) {
			float l = 0.f, r = 0.f;
			lv[k].next(sr, l, r);
			int v = lvVoice[k];
			float g = v >= 0 ? params[VLVL_PARAM + colOf(v)].getValue() : 0.8f;
			if (v >= 0)
				vo[v] += 0.5f * (l + r) * g;
			L += l * g;
			R += r * g;
		}
		for (SVoice& x : fv)
			x.next(sr, L, R);
		// DRIVE: a soft clip that pushes the break
		float d = params[DRIVE_PARAM].getValue();
		if (d > 0.001f) {
			float gn = 1.f + 7.f * d;
			float norm = 1.f / std::sqrt(gn);
			L = std::tanh(L * gn) * norm * 1.4f;
			R = std::tanh(R * gn) * norm * 1.4f;
		}
		float lvl = 5.f * params[LEVEL_PARAM].getValue();
		for (int c = 0; c < DRUMS; c++)
			outputs[VOICE_OUTPUT + c].setVoltage(clamp(vo[DRUM_AT[c]] * lvl, -12.f, 12.f));
		float ml = clamp(L * lvl, -12.f, 12.f), mr = clamp(R * lvl, -12.f, 12.f);
		outputs[MIXL_OUTPUT].setVoltage(ml);
		outputs[MIXR_OUTPUT].setVoltage(mr);
		// ---- to GENRA CHOP: the mix (for CHOP FX), which voices the kit leaves to the break, the heartbeat
		if (h) {
			h->brkMixL = ml;
			h->brkMixR = mr;
			if ((tick & 255) == 0) {
				uint32_t mask = 0;
				if (mixMode == BM_REPLACE) {
					if (playMode == BP_HITS) {
						for (int v = 0; v < DRUMS; v++)
							if (voicePad[v] >= 0 && voicePad[v] < countNow() && !muted(v))
								mask |= 1u << v;
					}
					else if (step >= 0)
						mask = 0xff; // the break plays the drums
				}
				h->brkReplace = mask;
				h->brkBeat++;
			}
		}
		if ((tick & 63) == 0) {
			const SVoice* pv = posVoice;
			posPub = (pv && pv->active && chops && chops->data.frames()) ? (float) (pv->pos / (double) chops->data.frames()) : -1.f;
		}
	}

	// ---------------------------------------------------------------- the panel's actions (UI thread)
	void collectTrash() {
		if (!incoming.load())
			if (ChopSet* c = loader->done.exchange(nullptr))
				incoming = c;
		const ChopSet* ui = uiChops.load();
		for (auto& t : trash) {
			ChopSet* c = t.load();
			if (c && c != ui)
				delete t.exchange(nullptr);
		}
	}
	float bpmNow() {
		float b = bpmPub.load();
		if (b > 1.f)
			return b;
		const ChopSet* c = uiChops.load();
		return c && c->bpm > 0.f ? (c->source ? c->source->bpm : c->bpm) : 96.f;
	}
	WarpCfg warpCfg() {
		WarpCfg w;
		w.on = warpOn;
		w.mode = WARP_BEATS; // a break: its hits on the grid
		w.engine = hhw::ENGINE_RUBBER;
		w.dst = bpmNow();
		w.mul = warpMul;
		warpFor = w.dst;
		return w;
	}
	int chopsWanted() { return clamp((int) std::round(params[CHOPS_PARAM].getValue()), MIN_CHOPS, BRK_PADS); }
	void loadBreak() {
		std::string dir = path.empty() ? hhx::sampleFolder() : system::getDirectory(path);
		osdialog_filters* f = osdialog_filters_parse("WAV:wav,WAV");
		char* p = osdialog_file(OSDIALOG_OPEN, dir.empty() ? nullptr : dir.c_str(), nullptr, f);
		osdialog_filters_free(f);
		if (!p)
			return;
		path = p;
		std::free(p);
		cutCount = chopsWanted();
		want(path);
		startLoad(loader, path, chopMode, false, cutCount, warpCfg());
	}
	void loadPath(const std::string& p) {
		if (string::lowercase(system::getExtension(p)) != ".wav") {
			loader->say("only WAV files", true);
			return;
		}
		path = p;
		cutCount = chopsWanted();
		want(path);
		startLoad(loader, path, chopMode, false, cutCount, warpCfg());
	}
	void ownBreak() {
		path.clear();
		cutCount = chopsWanted();
		want("");
		startOwn(loader, chopMode, cutCount, warpCfg(), false);
	}
	void recut() {
		cutCount = chopsWanted();
		if (const ChopSet* c = uiChops.load())
			startRecut(loader, c, chopMode, cutCount, warpCfg());
	}
	void rewarp() {
		if (loader->busy) {
			warpPending = true;
			return;
		}
		warpPending = false;
		if (const ChopSet* c = uiChops.load())
			startWarp(loader, c, warpCfg());
	}
	void queuePad(int pad) {
		int w = padQw.load();
		padQ[w % 32] = pad;
		padQw = w + 1;
	}
	void copyPattern() {
		BrkPat p;
		{
			std::lock_guard<std::mutex> lk(shareM);
			p = shared;
		}
		BrkClip& c = BrkClip::get();
		std::lock_guard<std::mutex> lk(c.m);
		c.pat = p;
		c.has = true;
	}
	bool pastePattern() {
		BrkClip& c = BrkClip::get();
		BrkPat p;
		{
			std::lock_guard<std::mutex> lk(c.m);
			if (!c.has)
				return false;
			p = c.pat;
		}
		std::lock_guard<std::mutex> lk(shareM);
		pasteBuf = p;
		pasteIn = true;
		return true;
	}
	std::string saveChops() {
		const ChopSet* c = uiChops.load();
		if (!c)
			return "No break loaded";
		std::string base = hhx::chopFolder();
		std::string name = c->data.name.empty() ? "break" : c->data.name;
		std::string dir = system::join(base, hhx::freeName(base, name, ""));
		try {
			system::createDirectories(dir);
			for (int i = 0; i < std::min(c->info.count, BRK_PADS); i++) {
				std::vector<uint8_t> w = writeWav(c->data, c->start[i], c->end[i], false, 0);
				int v = brkVoiceOf(voicePad, i);
				std::string tag = v >= 0 ? std::string("_") + string::lowercase(VNAME[colOf(v)]) : "";
				system::writeFile(system::join(dir, string::f("%s_chop%02d%s.wav", hhx::cleanName(name).c_str(), i + 1, tag.c_str())), w);
			}
		}
		catch (Exception& e) {
			return e.what();
		}
		return "Saved to " + dir;
	}

	// ---------------------------------------------------------------- saving
	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "version", json_integer(1));
		json_object_set_new(r, "path", json_string(path.c_str()));
		json_object_set_new(r, "play", json_integer(playMode));
		json_object_set_new(r, "mix", json_integer(mixMode));
		json_object_set_new(r, "chopMode", json_integer(chopMode));
		json_object_set_new(r, "warpOn", json_boolean(warpOn));
		json_object_set_new(r, "warpMul", json_real(warpMul));
		json_object_set_new(r, "mute", json_integer(muteMask));
		json_object_set_new(r, "lock", json_boolean(lock));
		json_object_set_new(r, "reverse", json_boolean(reverseOn));
		json_object_set_new(r, "linkedId", json_integer(linkedId));
		json_t* jv = json_array();
		for (int v = 0; v < DRUMS; v++)
			json_array_append_new(jv, json_integer(voicePad[v]));
		json_object_set_new(r, "voices", jv);
		json_t* je = json_array();
		json_t* jd = json_array();
		for (int s = 0; s < LEN; s++) {
			const BrkEv& e = pat.ev[s];
			if (e.pad >= 0)
				json_array_append_new(je, json_pack("[iiff]", s, (int) e.pad, e.len, e.vel));
			json_array_append_new(jd, json_pack("[ffffiii]", pat.dice[s][0], pat.dice[s][1], pat.dice[s][2], pat.dice[s][3],
			                                    (int) pat.alt[s], (int) pat.semis[s], (int) pat.rat[s]));
		}
		json_object_set_new(r, "events", je);
		json_object_set_new(r, "dice", jd);
		return r;
	}
	void dataFromJson(json_t* r) override {
		auto geti = [&](const char* k, int d) {
			json_t* j = json_object_get(r, k);
			return j ? (int) json_integer_value(j) : d;
		};
		auto num = [](json_t* a, size_t i) { return (float) json_number_value(json_array_get(a, i)); };
		json_t* jp = json_object_get(r, "path");
		path = jp ? json_string_value(jp) : "";
		playMode = clamp(geti("play", BP_HITS), 0, NUM_BRK_PLAY - 1);
		mixMode = clamp(geti("mix", BM_LAYER), 0, NUM_BRK_MIX - 1);
		chopMode = clamp(geti("chopMode", CM_TRANSIENT), 0, NUM_CHOP_MODES - 1);
		warpOn = json_is_true(json_object_get(r, "warpOn"));
		if (json_t* j = json_object_get(r, "warpMul"))
			warpMul = clamp((float) json_number_value(j), 0.25f, 4.f);
		muteMask = (uint32_t) geti("mute", 0) & 0xff;
		lock = json_is_true(json_object_get(r, "lock"));
		reverseOn = json_is_true(json_object_get(r, "reverse"));
		linkedId = geti("linkedId", -1);
		if (json_t* jv = json_object_get(r, "voices")) {
			for (size_t v = 0; v < DRUMS && v < json_array_size(jv); v++)
				voicePad[v] = clamp((int) json_integer_value(json_array_get(jv, v)), -1, BRK_PADS - 1);
		}
		pat.clear();
		json_t* je = json_object_get(r, "events");
		for (size_t i = 0; i < json_array_size(je); i++) {
			json_t* o = json_array_get(je, i);
			int s = (int) num(o, 0);
			if (s < 0 || s >= LEN)
				continue;
			pat.ev[s].pad = (int8_t) clamp((int) num(o, 1), -1, BRK_PADS - 1);
			pat.ev[s].len = std::max(0.25f, num(o, 2));
			pat.ev[s].vel = clamp(num(o, 3), 0.f, 1.f);
		}
		json_t* jd = json_object_get(r, "dice");
		for (size_t s = 0; s < LEN && s < json_array_size(jd); s++) {
			json_t* o = json_array_get(jd, s);
			for (int k = 0; k < NUM_DICE; k++)
				pat.dice[s][k] = clamp(num(o, k), 0.f, 1.f);
			pat.alt[s] = (int8_t) clamp((int) num(o, 4), 0, 127);
			pat.semis[s] = (int8_t) clamp((int) num(o, 5), -24, 24);
			pat.rat[s] = (uint8_t) clamp((int) num(o, 6), 1, 4);
		}
		patDirty = true;
		// the break: the file (if it is still there), else its own; the pattern and the voices stay
		cutCount = chopsWanted();
		if (!path.empty() && system::isFile(path)) {
			want(path);
			startLoad(loader, path, chopMode, true, cutCount, warpCfg());
		}
		else {
			want("");
			startOwn(loader, chopMode, cutCount, warpCfg(), true);
		}
	}
	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		playMode = BP_HITS;
		mixMode = BM_LAYER;
		muteMask = 0;
		lock = false;
		reverseOn = false;
		for (int v = 0; v < DRUMS; v++)
			voicePad[v] = -1; // found again from the break
		reqNew = true;
	}
};

// ================================================================ the panel
struct BrkLcd : widget::Widget {
	ChopBreak* m = nullptr;
	void draw(const DrawArgs& args) override {
		NVGcontext* vg = args.vg;
		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 1.5f);
		nvgFillColor(vg, C_LCD);
		nvgFill(vg);
		auto f = lcdFont();
		std::string l1 = "BRK GENRA FUNK BREAK  16 CHOPS", l2 = "HITS  LAYER  KICK 1  SNR 3", l3 = "LINK: GENRA CHOP";
		if (m) {
			const ChopSet* c = m->uiChops.load();
			if (m->loader->busy)
				l1 = "CUTTING THE BREAK...";
			else if (c) {
				float src = (c->source ? c->source->bpm : c->bpm) * m->warpMul;
				l1 = string::f("BRK %s  %d CHOPS  %s", string::uppercase(c->data.name).substr(0, 18).c_str(), std::min(c->info.count, BRK_PADS),
				               m->warpOn ? string::f("%.0f>%.0f BPM", src, m->bpmNow()).c_str() : string::f("%.0f BPM", src).c_str());
			}
			else {
				std::lock_guard<std::mutex> lk(m->loader->m);
				l1 = string::uppercase(m->loader->err.empty() ? "NO BREAK" : "ERR: " + m->loader->err);
			}
			l2 = string::uppercase(std::string(BRK_PLAY_NAMES[m->playMode])) + (m->mixMode == BM_REPLACE ? "  REPLACE" : "  LAYER");
			for (int col : {0, 1, 2, 3}) {
				int v = DRUM_AT[col];
				int p = m->voicePad[v];
				l2 += string::f("  %s %s", VNAME[col], p >= 0 ? std::to_string(p + 1).c_str() : "-");
			}
			if (m->linkPub.load())
				l3 = "LINK: GENRA CHOP";
			else if (m->inputs[ChopBreak::CLK_INPUT].isConnected())
				l3 = "OWN CLOCK";
			else
				l3 = "NO LINK - TRIGGER JACKS";
			int s = m->stepPub.load();
			if (s >= 0)
				l3 += string::f("  BAR %d.%d", s / STEPS + 1, (s % STEPS) / 4 + 1);
			if (m->lock)
				l3 += "  LOCK";
			if (!note.empty())
				l3 = note;
		}
		label(vg, f, 6.f, 9.5f, l1, 12.5f, C_LCDINK, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		label(vg, f, 6.f, 21.5f, l2, 10.5f, C_LCDINK, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		label(vg, f, 6.f, 32.f, l3, 9.5f, nvgRGB(0x3a, 0x4a, 0x2a), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
	}
	std::string note;
};

struct BrkWave : widget::OpaqueWidget {
	ChopBreak* m = nullptr;
	void onButton(const ButtonEvent& e) override {
		if (m && e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS && !m->uiChops.load() && !m->loader->busy) {
			m->loadBreak();
			e.consume(this);
		}
	}
	void draw(const DrawArgs& args) override {
		NVGcontext* vg = args.vg;
		const ChopSet* c = m ? m->uiChops.load() : nullptr;
		float w = box.size.x, h = box.size.y;
		if (!c || c->peaks.empty()) {
			label(vg, uiBold(), w / 2, h / 2, m && m->loader->busy ? "CUTTING..." : "CLICK HERE OR PRESS LOAD BREAK", 9.f,
			      nvgRGB(0x8f, 0x8a, 0x7c));
			return;
		}
		size_t total = c->data.frames();
		int n = std::min(c->info.count, BRK_PADS);
		int lp = m->lastPad.load();
		if (lp >= 0 && lp < n && total) {
			float x0 = w * c->start[lp] / total, x1 = w * c->end[lp] / total;
			nvgBeginPath(vg);
			nvgRect(vg, x0, 0, x1 - x0, h);
			nvgFillColor(vg, nvgRGBA(0xa9, 0xb9, 0x8c, 0x30));
			nvgFill(vg);
		}
		nvgBeginPath(vg);
		size_t np = c->peaks.size();
		for (size_t i = 0; i < np; i++) {
			float x = w * i / (np - 1), a = c->peaks[i] * h * 0.46f;
			nvgRect(vg, x, h / 2 - a, std::max(0.6f, w / np), 2 * a);
		}
		nvgFillColor(vg, C_LCD);
		nvgFill(vg);
		float pp = m->posPub.load();
		if (pp >= 0.f && pp <= 1.f) {
			nvgBeginPath(vg);
			nvgRect(vg, w * pp - 0.6f, 0, 1.2f, h);
			nvgFillColor(vg, C_YELLOW);
			nvgFill(vg);
		}
		for (int p = 0; p < n; p++) {
			float x = total ? w * c->start[p] / total : 0.f;
			int v = brkVoiceOf(m->voicePad, p);
			NVGcolor col = v >= 0 ? VCOL[colOf(v)] : nvgRGB(0x77, 0x73, 0x6a);
			nvgBeginPath(vg);
			nvgRect(vg, x, 0, v >= 0 ? 1.2f : 0.8f, h);
			nvgFillColor(vg, col);
			nvgFill(vg);
			label(vg, uiBold(), x + 2.f, 5.f, std::to_string(p + 1), 6.5f, col, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		}
	}
};

struct BrkPad : widget::OpaqueWidget {
	ChopBreak* m = nullptr;
	int idx = 0;
	float flash = 0.f;
	uint32_t seenTick = 0;
	std::function<void(int)> clicked;
	void step() override {
		if (m && m->lastPad.load() == idx && m->lastPadTick.load() != seenTick) {
			seenTick = m->lastPadTick.load();
			flash = 1.f;
		}
		flash = std::max(0.f, flash - 0.06f);
		OpaqueWidget::step();
	}
	void draw(const DrawArgs& args) override {
		NVGcontext* vg = args.vg;
		const ChopSet* c = m ? m->uiChops.load() : nullptr;
		bool has = !m || (c && idx < std::min(c->info.count, BRK_PADS));
		NVGcolor fill = has ? nvgLerpRGBA(C_PAD, nvgRGB(0xf2, 0xc9, 0x6a), flash * 0.85f) : nvgRGB(0xa8, 0xa3, 0x96);
		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0.6f, 0.6f, box.size.x - 1.2f, box.size.y - 1.2f, 3.f);
		nvgFillColor(vg, fill);
		nvgFill(vg);
		nvgStrokeColor(vg, C_PADEDGE);
		nvgStrokeWidth(vg, 1.2f);
		nvgStroke(vg);
		label(vg, uiBold(), box.size.x / 2, 7.5f, std::to_string(idx + 1), 7.f, C_INKDIM);
		if (!m || !has)
			return;
		// the voices it plays for: a tag in the first one's colour (+ when more)
		int first = -1, more = 0;
		for (int col = 0; col < DRUMS; col++)
			if (m->voicePad[DRUM_AT[col]] == idx) {
				if (first < 0)
					first = col;
				else
					more++;
			}
		if (first >= 0) {
			float tw = box.size.x - 7.f, th = 7.f, ty = box.size.y - th - 3.5f;
			nvgBeginPath(vg);
			nvgRoundedRect(vg, 3.5f, ty, tw, th, 1.2f);
			nvgFillColor(vg, m->muted(DRUM_AT[first]) ? nvgRGB(0x7a, 0x2e, 0x22) : VCOL[first]);
			nvgFill(vg);
			labelFit(vg, uiBold(), box.size.x / 2, ty + th / 2 + 0.3f, std::string(VNAME[first]) + (more ? "+" : ""), 6.f, tw - 2.f,
			         nvgRGB(0x1e, 0x1d, 0x1c));
		}
	}
	void onButton(const ButtonEvent& e) override {
		if (m && e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
			if (clicked)
				clicked(idx);
			e.consume(this);
		}
	}
};

/** A voice's button: its name, a strip in its colour; lit while it waits for a pad, dark red when muted. */
struct VoiceButton : HHButton {
	int col = 0;
	void draw(const DrawArgs& args) override {
		HHButton::draw(args);
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 1.f, box.size.y - 2.4f, box.size.x - 2.f, 1.8f, 0.8f);
		nvgFillColor(args.vg, VCOL[col]);
		nvgFill(args.vg);
	}
};

/** The rate of ROLL in a little screen. */
struct RateDisp : widget::Widget {
	ChopBreak* m = nullptr;
	void draw(const DrawArgs& args) override {
		NVGcontext* vg = args.vg;
		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 1.5f);
		nvgFillColor(vg, nvgRGB(0x14, 0x14, 0x13));
		nvgFill(vg);
		nvgStrokeColor(vg, nvgRGB(0x55, 0x53, 0x4f));
		nvgStrokeWidth(vg, 0.8f);
		nvgStroke(vg);
		int r = m ? clamp((int) std::round(m->params[ChopBreak::RATE_PARAM].getValue()), 0, NUM_RATES - 1) : 3;
		label(vg, uiBold(), box.size.x / 2, box.size.y / 2 + 0.5f, RATE_NAMES[r], 9.5f, nvgRGB(0x9f, 0xd3, 0xff));
	}
};

/** The break's tempo: found (x2 / /2) > the beat's with WARP. */
struct BrkBpm : widget::Widget {
	ChopBreak* m = nullptr;
	void draw(const DrawArgs& args) override {
		NVGcontext* vg = args.vg;
		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 1.5f);
		nvgFillColor(vg, C_LCD);
		nvgFill(vg);
		std::string t = "-- BPM";
		if (m) {
			const ChopSet* c = m->uiChops.load();
			float src = c ? (c->source ? c->source->bpm : c->bpm) * m->warpMul : 0.f;
			if (m->loader->busy && m->warpOn)
				t = "WARPING...";
			else if (src > 0.f && m->warpOn)
				t = string::f("%.0f > %.0f BPM", src, m->bpmNow());
			else if (src > 0.f)
				t = string::f("%.1f BPM", src);
		}
		labelFit(vg, lcdFont(), box.size.x / 2, box.size.y / 2 + 0.5f, t, 11.f, box.size.x - 6.f, C_LCDINK);
	}
};

/** A field whose text shrinks to fit. */
struct FitField : HHField {
	void draw(const DrawArgs& args) override {
		labelFit(args.vg, uiBold(), 5.f, box.size.y / 2 + 0.5f, value ? value() : "", 9.5f, box.size.x - 14.f, C_CREAM,
		         NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		float x = box.size.x - 6.f, y = box.size.y / 2;
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, x - 2.6f, y - 1.3f);
		nvgLineTo(args.vg, x + 2.6f, y - 1.3f);
		nvgLineTo(args.vg, x, y + 1.7f);
		nvgClosePath(args.vg);
		nvgFillColor(args.vg, C_RED);
		nvgFill(args.vg);
	}
};

struct ChopBreakWidget : ModuleWidget {
	BrkLcd* lcd = nullptr;
	int selVoice = -1; // the voice waiting for a pad (by DrumVoice)
	int chopsSeen = -1;
	float chopsRest = 0.f;
	float warpSeen = -1.f, warpRest = 0.f;
	float noteTime = 0.f;

	template <typename T>
	T* at(const float* r) {
		T* w = new T();
		w->box = rectOf(r);
		addChild(w);
		return w;
	}
	HHButton* button(const float* r, const char* text, NVGcolor col, std::function<void()> act) {
		HHButton* b = at<HHButton>(r);
		b->text = text;
		b->col = col;
		b->act = act;
		return b;
	}
	void say(const std::string& s) {
		if (lcd)
			lcd->note = string::uppercase(s);
		noteTime = 3.f;
	}

	ChopBreakWidget(ChopBreak* m) {
		setModule(m);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/ChopBreak.svg")));
		using M = ChopBreak;
		const NVGcolor GREY = nvgRGB(0x55, 0x53, 0x4f);
		auto knobS = [&](const float* p, int id) { addParam(createParamCentered<HHKnobSmall>(Vec(p[0], p[1]), m, id)); };
		auto knobB = [&](const float* p, int id) { addParam(createParamCentered<HHKnob>(Vec(p[0], p[1]), m, id)); };
		knobS(cbl::TUNE, M::TUNE_PARAM);
		knobS(cbl::DRIVE, M::DRIVE_PARAM);
		knobS(cbl::LEVEL, M::LEVEL_PARAM);
		lcd = at<BrkLcd>(cbl::LCD);
		lcd->m = m;
		// PLAY and MIX
		HHField* play = at<FitField>(cbl::MODE);
		play->value = [=]() { return std::string("PLAY  ") + BRK_PLAY_NAMES[m ? m->playMode : 0]; };
		play->count = []() { return (int) NUM_BRK_PLAY; };
		play->current = [=]() { return m ? m->playMode : 0; };
		play->item = [](int i) { return std::string(BRK_PLAY_TEXT[i]); };
		play->choose = [=](int i) {
			if (m)
				m->playMode = i;
		};
		HHField* mix = at<FitField>(cbl::MIX);
		mix->value = [=]() { return std::string("MIX  ") + BRK_MIX_NAMES[m ? m->mixMode : 0]; };
		mix->count = []() { return (int) NUM_BRK_MIX; };
		mix->current = [=]() { return m ? m->mixMode : 0; };
		mix->item = [](int i) {
			return std::string(i == 0 ? "Layer with kit (GENRA CHOP's triggers play on)" : "Replace kit (its triggers rest where the break plays)");
		};
		mix->choose = [=](int i) {
			if (m)
				m->mixMode = i;
		};
		// the waveform and its row
		BrkWave* wave = at<BrkWave>(cbl::WAVE);
		wave->m = m;
		HHButton* load = button(cbl::LOAD, "LOAD BREAK", C_YELLOW, [=]() {
			if (m)
				m->loadBreak();
		});
		load->textCol = C_YELLOWINK;
		HHField* cm = at<FitField>(cbl::CMODE);
		cm->value = [=]() { return std::string(CHOP_MODES[m ? m->chopMode : 0]); };
		cm->count = []() { return (int) NUM_CHOP_MODES; };
		cm->current = [=]() { return m ? m->chopMode : 0; };
		cm->item = [](int i) { return std::string(i == 0 ? "Transient (on the hits)" : "Equal (parts of the same length)"); };
		cm->choose = [=](int i) {
			if (!m || i == m->chopMode)
				return;
			m->chopMode = i;
			m->recut();
		};
		HHButton* warp = button(cbl::WARP, "WARP", GREY, [=]() {
			if (!m)
				return;
			m->warpOn = !m->warpOn;
			m->rewarp();
		});
		warp->colOf = [=]() { return m && m->warpOn ? C_RED : GREY; };
		BrkBpm* bb = at<BrkBpm>(cbl::WBPM);
		bb->m = m;
		button(cbl::WX2, "x2", GREY, [=]() {
			if (!m)
				return;
			m->warpMul = std::min(4.f, m->warpMul * 2.f);
			if (m->warpOn)
				m->rewarp();
		});
		button(cbl::WD2, "/2", GREY, [=]() {
			if (!m)
				return;
			m->warpMul = std::max(0.25f, m->warpMul * 0.5f);
			if (m->warpOn)
				m->rewarp();
		});
		knobS(cbl::CHOPSK, M::CHOPS_PARAM);
		// the pads: tap = play; with a voice waiting = that voice plays this chop
		const float* PD[16] = {cbl::PAD0, cbl::PAD1, cbl::PAD2, cbl::PAD3, cbl::PAD4, cbl::PAD5, cbl::PAD6, cbl::PAD7,
		                       cbl::PAD8, cbl::PAD9, cbl::PAD10, cbl::PAD11, cbl::PAD12, cbl::PAD13, cbl::PAD14, cbl::PAD15};
		for (int i = 0; i < 16; i++) {
			BrkPad* p = at<BrkPad>(PD[i]);
			p->m = m;
			p->idx = i < 8 ? i + 8 : i - 8; // the top row 9-16, the bottom row 1-8 (pad 1 bottom left, as on an MPC)
			p->clicked = [=](int pad) {
				if (!m)
					return;
				if (selVoice >= 0) {
					m->voicePad[selVoice] = pad;
					selVoice = -1;
				}
				m->queuePad(pad);
			};
		}
		const float* VS[8] = {cbl::VSEL0, cbl::VSEL1, cbl::VSEL2, cbl::VSEL3, cbl::VSEL4, cbl::VSEL5, cbl::VSEL6, cbl::VSEL7};
		const float* VL[8] = {cbl::VLVL0, cbl::VLVL1, cbl::VLVL2, cbl::VLVL3, cbl::VLVL4, cbl::VLVL5, cbl::VLVL6, cbl::VLVL7};
		for (int c = 0; c < DRUMS; c++) {
			int v = DRUM_AT[c];
			VoiceButton* b = at<VoiceButton>(VS[c]);
			b->col = c;
			b->text = VNAME[c];
			b->fontSize = 7.5f;
			b->act = [=]() { selVoice = selVoice == v ? -1 : v; };
			b->colOf = [=]() {
				if (!m)
					return GREY;
				if (selVoice == v)
					return VCOL[c];
				return m->muted(v) ? nvgRGB(0x7a, 0x2e, 0x22) : GREY;
			};
			b->textColOf = [=]() { return selVoice == v ? nvgRGB(0x1e, 0x1d, 0x1c) : C_CREAM; };
			b->lit = [=]() { return selVoice == v; };
			b->rightMenu = [=](ui::Menu* mn) {
				if (!m)
					return;
				mn->addChild(createMenuLabel(string::f("%s: tap it, then a pad (that chop plays it)", VNAME[c])));
				mn->addChild(createCheckMenuItem("Mute", "", [=]() { return m->muted(v); }, [=]() { m->muteMask ^= 1u << v; }));
				mn->addChild(createSubmenuItem("Chop", m->voicePad[v] >= 0 ? std::to_string(m->voicePad[v] + 1) : "none", [=](ui::Menu* sub) {
					sub->addChild(createCheckMenuItem("None", "", [=]() { return m->voicePad[v] < 0; }, [=]() { m->voicePad[v] = -1; }));
					const ChopSet* cs = m->uiChops.load();
					int n = cs ? std::min(cs->info.count, BRK_PADS) : 0;
					for (int p = 0; p < n; p++)
						sub->addChild(createCheckMenuItem(string::f("Chop %d", p + 1), "", [=]() { return m->voicePad[v] == p; },
						                                  [=]() { m->voicePad[v] = p; }));
				}));
				mn->addChild(createMenuItem("All voices found again (from the sound)", "", [=]() {
					if (const ChopSet* cs = m->uiChops.load())
						brkAssign(*cs, m->voicePad);
				}));
			};
			knobS(VL[c], M::VLVL_PARAM + c);
		}
		// PATTERN
		HHButton* lk = button(cbl::LOCK, "LOCK", C_GRAPH, [=]() {
			if (m)
				m->lock = !m->lock;
		});
		lk->lit = [=]() { return m && m->lock; };
		button(cbl::UNDO, "UNDO", C_GRAPH, [=]() {
			if (m)
				m->reqUndo = true;
		});
		button(cbl::CLEAR, "CLEAR", C_GRAPH, [=]() {
			if (m)
				m->reqClear = true;
		});
		button(cbl::COPY, "COPY", C_GRAPH, [=]() {
			if (!m)
				return;
			m->copyPattern();
			say("pattern copied");
		});
		button(cbl::PASTE, "PASTE", C_GRAPH, [=]() {
			if (!m)
				return;
			say(m->pastePattern() ? "pattern pasted" : "nothing copied yet");
		});
		button(cbl::SAVE, "SAVE", C_GRAPH, [=]() {
			if (m)
				say(m->saveChops());
		});
		HHButton* roll = button(cbl::ROLL, "ROLL", C_RED, [=]() {
			if (m)
				m->rollHeld = true;
		});
		roll->release = [=]() {
			if (m)
				m->rollHeld = false;
		};
		roll->fontSize = 11.f;
		roll->lit = [=]() { return m && m->rollHeld; };
		RateDisp* rd = at<RateDisp>(cbl::RDISP);
		rd->m = m;
		knobB(cbl::MRATE, M::RATE_PARAM);
		// MANGLE
		knobB(cbl::M0, M::PITCH_PARAM);
		knobB(cbl::M1, M::DECAY_PARAM);
		knobB(cbl::M2, M::STUTTER_PARAM);
		knobB(cbl::M3, M::SHUFFLE_PARAM);
		button(cbl::MNEW, "NEW", C_RED, [=]() {
			if (m)
				m->reqNew = true; // the button rolls even with LOCK (LOCK holds against NEW ALL and the parts)
		});
		HHButton* rev = button(cbl::MREV, "REVERSE", GREY, [=]() {
			if (m)
				m->reverseOn = !m->reverseOn;
		});
		rev->colOf = [=]() { return m && m->reverseOn ? C_RED : GREY; };
		// jacks
		auto in = [&](const float* p, int id) { addInput(createInputCentered<PJ301MPort>(Vec(p[0], p[1]), m, id)); };
		auto out = [&](const float* p, int id) { addOutput(createOutputCentered<PJ301MPort>(Vec(p[0], p[1]), m, id)); };
		in(cbl::CLK_IN, M::CLK_INPUT);
		in(cbl::RST_IN, M::RST_INPUT);
		const float* TI[8] = {cbl::TRIG0_IN, cbl::TRIG1_IN, cbl::TRIG2_IN, cbl::TRIG3_IN, cbl::TRIG4_IN, cbl::TRIG5_IN, cbl::TRIG6_IN, cbl::TRIG7_IN};
		const float* VO[8] = {cbl::V0_OUT, cbl::V1_OUT, cbl::V2_OUT, cbl::V3_OUT, cbl::V4_OUT, cbl::V5_OUT, cbl::V6_OUT, cbl::V7_OUT};
		for (int c = 0; c < DRUMS; c++) {
			in(TI[c], M::TRIG_INPUT + c);
			out(VO[c], M::VOICE_OUTPUT + c);
		}
		in(cbl::PITCH_IN, M::PITCH_INPUT);
		in(cbl::STUT_IN, M::STUT_INPUT);
		in(cbl::ROLL_IN, M::ROLL_INPUT);
		in(cbl::NEW_IN, M::NEW_INPUT);
		out(cbl::OUTL_OUT, M::MIXL_OUTPUT);
		out(cbl::OUTR_OUT, M::MIXR_OUTPUT);
	}

	/** GENRA CHOP: chosen in the menu, else next to it (or one GENRA CHOP module further: GRID, ABLAUF, FX), else the
	 *  only one in the patch. */
	void findHost(ChopBreak* m) {
		GenraChop* found = nullptr;
		if (m->linkedId >= 0) {
			Module* x = APP->engine->getModule(m->linkedId);
			if (x && x->model == modelGenraChop)
				found = static_cast<GenraChop*>(x);
		}
		if (!found) {
			Module* side[2] = {m->leftExpander.module, m->rightExpander.module};
			for (int k = 0; k < 2 && !found; k++) {
				Module* x = side[k];
				if (!x)
					continue;
				if (x->model == modelGenraChop)
					found = static_cast<GenraChop*>(x);
				else if (x->model == modelChopGrid || x->model == modelChopAblauf || x->model == modelChopFx) {
					Module* y = k == 0 ? x->leftExpander.module : x->rightExpander.module;
					if (y && y->model == modelGenraChop)
						found = static_cast<GenraChop*>(y);
				}
			}
		}
		if (!found && m->linkedId < 0) {
			ChopRegistry& reg = ChopRegistry::get();
			std::lock_guard<std::mutex> lk(reg.m);
			if (reg.all.size() == 1)
				found = reg.all[0];
		}
		if (found != m->host.load()) {
			m->host = found;
			m->hostChanged = true;
		}
	}

	void onPathDrop(const PathDropEvent& e) override {
		ChopBreak* m = dynamic_cast<ChopBreak*>(module);
		if (m && !e.paths.empty()) {
			m->loadPath(e.paths[0]);
			e.consume(this);
		}
	}

	void step() override {
		if (ChopBreak* m = dynamic_cast<ChopBreak*>(module)) {
			m->collectTrash();
			findHost(m);
			if (m->warpPending && !m->loader->busy)
				m->rewarp();
			// WARP follows the tempo: when it rests a moment on a new one, the break is warped again
			if (m->warpOn && m->uiChops.load()) {
				float now = m->bpmNow();
				if (std::fabs(now - m->warpFor) > 0.05f) {
					if (std::fabs(now - warpSeen) > 0.01f) {
						warpSeen = now;
						warpRest = 0.4f;
					}
					else if (warpRest > 0.f) {
						warpRest -= APP->window->getLastFrameDuration();
						if (warpRest <= 0.f && !m->loader->busy)
							m->rewarp();
						else if (warpRest <= 0.f)
							warpRest = 0.1f;
					}
				}
			}
			// CHOPS: when it rests a moment on a new number, the break is cut anew
			int want = m->chopsWanted();
			if (want != chopsSeen) {
				if (chopsSeen >= 0)
					chopsRest = 0.35f;
				chopsSeen = want;
			}
			if (chopsRest > 0.f) {
				chopsRest -= APP->window->getLastFrameDuration();
				if (chopsRest <= 0.f && m->loader->busy)
					chopsRest = 0.1f;
				else if (chopsRest <= 0.f && want != m->cutCount)
					m->recut();
			}
		}
		if (noteTime > 0.f) {
			noteTime -= APP->window->getLastFrameDuration();
			if (noteTime <= 0.f && lcd)
				lcd->note.clear();
		}
		else if (selVoice >= 0 && lcd)
			lcd->note = string::f("%s: TAP THE PAD IT PLAYS", VNAME[colOf(selVoice)]);
		else if (lcd)
			lcd->note.clear();
		ModuleWidget::step();
	}

	void appendContextMenu(Menu* menu) override {
		ChopBreak* m = dynamic_cast<ChopBreak*>(module);
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Break"));
		menu->addChild(createMenuItem("Load break…", "", [=]() { m->loadBreak(); }));
		menu->addChild(createMenuItem("Its own break (GENRA FUNK BREAK)", "", [=]() { m->ownBreak(); }));
		menu->addChild(createMenuItem("Chop folder…", "", []() { hhx::pickChopFolder(); }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Connection"));
		menu->addChild(createCheckMenuItem("Automatic (GENRA CHOP next to it, or the only one)", "", [=]() { return m->linkedId < 0; },
		                                   [=]() { m->linkedId = -1; }));
		menu->addChild(createSubmenuItem("Connect to…", "", [=](Menu* sub) {
			int n = 0;
			for (int64_t id : APP->engine->getModuleIds()) {
				Module* x = APP->engine->getModule(id);
				if (!x || x->model != modelGenraChop)
					continue;
				n++;
				GenraChop* c = static_cast<GenraChop*>(x);
				sub->addChild(createCheckMenuItem(string::f("GENRA CHOP #%d (%s)", n, c->style().name), "",
				                                  [=]() { return m->linkedId == id; }, [=]() { m->linkedId = id; }));
			}
			if (!n)
				sub->addChild(createMenuLabel("No GENRA CHOP in the patch"));
		}));
		menu->addChild(createMenuItem("Undo steps", string::f("%d", m->undoCount.load()), []() {}));
	}
};

Model* modelChopBreak = createModel<ChopBreak, ChopBreakWidget>("GenraChopBreak");
