// SPDX-License-Identifier: GPL-2.0-or-later
// Rubber Band Library, built as one unit (its own FFT and resampler everywhere, no threads, no Accelerate on the Mac).
// The library: dep/rubberband (GPL-2.0-or-later, Copyright Particular Programs Ltd.), see dep/rubberband/COPYING.
#define ALREADY_CONFIGURED 1
#define USE_BQRESAMPLER 1
#define USE_BUILTIN_FFT 1
#define NO_TIMING 1
#define NO_THREADING 1
#define NO_THREAD_CHECKS 1
#if defined(__APPLE__)
// macOS has no sincos()/sincosf() (only __sincos), so use plain sin/cos there.
#define LACK_SINCOS 1
#endif
#include "../../dep/rubberband/single/RubberBandSingle.cpp"
