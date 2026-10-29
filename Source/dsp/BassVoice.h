// TB-303 style bass voice, a port of the web app's oscillator / dual-biquad
// filter / VCA model including its slide, tie and accent behaviour.
#pragma once

#include "Core.h"
#include "../Patterns.h"
#include <vector>

namespace rb
{
struct BassParams
{
    float tuning = 0.5f, cutoff = 0.35f, resonance = 0.78f, envmod = 0.55f, decay = 0.4f,
          accent = 0.7f, volume = 0.8f, waveform = 0.0f;
};

class BassVoice
{
public:
    void prepare (double sampleRate);
    void reset();

    // Mirrors setParams(): recomputes the smoothed filter/level targets.
    void setParams (const BassParams& p);

    // Mirrors step(cur, next, time, stepDuration, params).
    void step (const BassStep& cur, const BassStep& next, double t, double stepDur, const BassParams& p);
    void release (double t);

    // Live notes (single mode): legato = another key is still held, so slide without retrigger.
    // stepDur: an accented note held longer than a step drops to normal level then (as in step()).
    void noteOn (double t, int semitones, bool accent, bool legato, const BassParams& p, double stepDur);
    void noteOff (double t);

    // Renders one mono sample at absolute time t.
    float render (double t);

private:
    void trigger (double t, bool accent, const BassParams& p);
    void close (double t);

    double sr = 44100.0;
    double phase = 0.0;
    bool pulse = false;
    bool sliding = false, holding = false, needsSnap = true;

    AutoParam pitch, env, vca;
    Smoothed f1Freq, f1Q, qIn, qOut, f2Freq, f2Q, detune, level;
    Biquad f1, f2;
    float hpB0 = 1, hpB1 = 0, hpA1 = 0, hpX = 0, hpY = 0; // 90 Hz DC blocker
};

} // namespace rb
