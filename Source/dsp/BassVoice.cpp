#include "BassVoice.h"

namespace rb
{
namespace
{
    // Constants from the web app's 303 model.
    constexpr double kGateLen = 0.5;     // re
    constexpr double kCloseTau = 0.022;  // ie
    constexpr double kSlideTau = 0.017;  // ae
    constexpr double kLevelGain = 1.244; // oe
    constexpr double kCutExp = 3.093;    // se
    constexpr double kQLimit = 1.25;     // ue
    constexpr double kEnvModOct = -1.51; // pe
    constexpr double kAccentEnv = 3.3;   // ge
    constexpr double kAccentEnvTau = 0.065; // _e
    constexpr double kAccentVca = 2.85;  // ve
    constexpr double kAccentVcaTau = 0.092; // ye
    constexpr double kPulseWidth = 0.4767;  // xe
    constexpr double kPulseAmp = 0.55;      // Se
    constexpr double kBaseHz = 65.40639132514966; // MIDI 36

    double tuningCents (double e)
    {
        const double t = e * 127.0 - 64.0;
        return t >= 0 ? std::round ((t * 12.0) / 63.0) * 100.0 : 1200.0 * std::log2 (1.0 + t / 128.0);
    }

    double decayTau (double e)
    {
        static const double me[] = { 0.011, 0.124, 0.261, 0.763, 2.089 };
        const double t = std::clamp (e, 0.0, 1.0) * 4.0;
        const int n = std::min (3, (int) std::floor (t));
        return me[n] * std::pow (me[n + 1] / me[n], t - n);
    }

    double envDepth (double envmod) { return 0.81 + 3.91 * envmod; } // fe

    // Band-limited copies of the two PeriodicWaves (saw / pulse), one per octave.
    struct Tables
    {
        static constexpr int kSize = 4096;
        static constexpr int kHarm = 2047;
        static constexpr int kLevels = 12;
        int maxHarm[kLevels] {};
        std::vector<float> saw[kLevels], pulse[kLevels];

        Tables()
        {
            std::vector<double> accSaw (kSize, 0.0), accPulse (kSize, 0.0);
            int built = 0;
            for (int lvl = 0; lvl < kLevels; ++lvl)
            {
                const int limit = lvl == kLevels - 1 ? kHarm : (1 << lvl);
                for (int k = built + 1; k <= limit; ++k)
                {
                    const double c = ((k % 2) ? 1.0 : -1.0) * 2.0 / (kPi * k);
                    const double ang = 2.0 * kPi * k * kPulseWidth;
                    const double pImag = kPulseAmp * c * (1.0 - std::cos (ang));
                    const double pReal = kPulseAmp * c * std::sin (ang);
                    const double w = 2.0 * kPi * k / kSize;
                    const double cw = std::cos (w), sw = std::sin (w);
                    double cs = 1.0, sn = 0.0;
                    for (int i = 0; i < kSize; ++i)
                    {
                        accSaw[(size_t) i] += c * sn;
                        accPulse[(size_t) i] += pReal * cs + pImag * sn;
                        const double ncs = cs * cw - sn * sw;
                        sn = sn * cw + cs * sw;
                        cs = ncs;
                    }
                }
                built = limit;
                maxHarm[lvl] = limit;
                for (double v : accSaw) saw[lvl].push_back ((float) v);
                for (double v : accPulse) pulse[lvl].push_back ((float) v);
                saw[lvl].push_back (saw[lvl][0]);
                pulse[lvl].push_back (pulse[lvl][0]);
            }
        }

        static const Tables& get()
        {
            static const Tables t;
            return t;
        }
    };
}

void BassVoice::prepare (double sampleRate)
{
    sr = sampleRate;
    Tables::get();
    for (auto* p : { &pitch, &env, &vca })
        p->prepare (sr);
    for (auto* s : { &f1Freq, &f1Q, &qIn, &qOut, &f2Freq, &f2Q, &detune, &level })
        s->prepare (sr);

    const double r = std::tan (kPi * 90.0 / sr);
    const double i = (1.0 - r) / (1.0 + r);
    hpB0 = (float) ((1.0 + i) / 2.0);
    hpB1 = (float) (-(1.0 + i) / 2.0);
    hpA1 = (float) i;
    reset();
}

void BassVoice::reset()
{
    pitch.reset (0.0f);
    env.reset (0.0f);
    vca.reset (0.0f);
    f1.setup (Biquad::Lowpass, 350.0, 1.0, sr);
    f2.setup (Biquad::Lowpass, 350.0, 1.0, sr);
    f1.reset();
    f2.reset();
    hpX = hpY = 0.0f;
    phase = 0.0;
    sliding = holding = false;
    needsSnap = true;
}

void BassVoice::setParams (const BassParams& e)
{
    const double cut = e.cutoff, res = e.resonance;
    // ce(): filter model fitted to the hardware
    const double fa = 204.6 * std::pow (2.0, kCutExp * cut) * (1.0 + 0.2565 * res * res * res);
    const double pole = fa * (0.1582 + 1.062 * res);
    const double gainDb = 2.691 + res * (-5.016 - 20.27 * cut) + res * res * (-3.414 + 22.4 * cut);
    const double q0 = (20.0 / std::log (10.0)) * (-0.4045 + 1.339 * res + 0.5493 * res * res);
    const double q1 = (20.0 / std::log (10.0)) * (0.9314 * res + 0.8111 * res * res);

    const double s = kEnvModOct * e.envmod;
    const double c = std::pow (2.0, s);
    const double l = 0.1 + 0.15 * res;
    const double u = std::min (kQLimit, cut + s / kCutExp);
    const double d = (kQLimit - u) * kCutExp * 1200.0;
    const double de = (1.0 / l - std::sqrt (1.0 / (l * l) - 4.0)) / 2.0;

    f1Freq.setTarget ((float) (fa * c));
    f1Q.setTarget ((float) (q0 + q1 * u));
    qIn.setTarget (d > 1.0 ? (float) (1.0 / d) : 0.0f);
    qOut.setTarget ((float) ((q1 * d) / (kCutExp * 1200.0)));
    f2Freq.setTarget ((float) ((pole * c) / de));
    f2Q.setTarget ((float) (20.0 * std::log10 (l)));
    detune.setTarget ((float) tuningCents (e.tuning));
    level.setTarget ((float) (e.volume * e.volume * kLevelGain * std::pow (10.0, gainDb / 20.0)));
    pulse = e.waveform >= 0.5f;

    if (needsSnap)
    {
        for (auto* sm : { &f1Freq, &f1Q, &qIn, &qOut, &f2Freq, &f2Q, &detune, &level })
            sm->snap (sm->targetValue());
        needsSnap = false;
    }
}

void BassVoice::trigger (double t, bool accent, const BassParams& p)
{
    const double r = accent ? p.accent : 0.0;
    const double a = envDepth (p.envmod) + r * kAccentEnv;
    env.cancelScheduledValues (t);
    env.setValueAtTime ((float) (a * 1200.0), t);
    env.setTargetAtTime (0.0f, t, accent ? kAccentEnvTau : decayTau (p.decay));

    vca.cancelScheduledValues (t);
    vca.setValueAtTime (0.0f, t);
    vca.linearRampToValueAtTime ((float) (1.0 + r * kAccentVca), t + 0.001);
    if (r != 0.0)
        vca.setTargetAtTime (1.0f, t + 0.001, kAccentVcaTau);
}

void BassVoice::close (double t)
{
    vca.setTargetAtTime (0.0f, t, kCloseTau);
}

void BassVoice::release (double t)
{
    sliding = false;
    vca.cancelAndHoldAtTime (t);
    vca.setTargetAtTime (0.0f, t, kCloseTau);
}

void BassVoice::step (const BassStep& cur, const BassStep& next, double n, double r, const BassParams& p)
{
    if (! cur.gate)
    {
        if (sliding)
            release (n);
        return;
    }
    if (cur.tie && holding)
    {
        holding = next.tie;
        sliding = cur.slide && next.gate && ! next.tie;
        if (! holding && ! sliding)
            close (n + r * kGateLen);
        return;
    }

    const float a = (float) ((cur.note + cur.octave * 12) * 100);
    const bool wasNotSliding = ! sliding;
    if (sliding)
        pitch.setTargetAtTime (a, n, kSlideTau);
    else
    {
        pitch.cancelScheduledValues (n);
        pitch.setValueAtTime (a, n);
        trigger (n, cur.accent, p);
    }
    sliding = cur.slide && next.gate && ! next.tie;
    holding = next.tie;
    if (! sliding && ! holding)
        close (n + r * kGateLen);
    else if (wasNotSliding && cur.accent)
        vca.setTargetAtTime (1.0f, n + r, 0.004);
}

void BassVoice::noteOn (double t, int semitones, bool accent, bool legato, const BassParams& p, double stepDur)
{
    const float a = (float) (semitones * 100);
    if (legato)
        pitch.setTargetAtTime (a, t, kSlideTau);
    else
    {
        pitch.cancelScheduledValues (t);
        pitch.setValueAtTime (a, t);
        trigger (t, accent, p);
        if (accent)
            vca.setTargetAtTime (1.0f, t + stepDur, 0.004); // cancelled by a note-off before that
    }
    sliding = holding = false;
}

void BassVoice::noteOff (double t)
{
    vca.cancelAndHoldAtTime (t);
    close (t);
}

float BassVoice::render (double t)
{
    const auto& tables = Tables::get();

    // Oscillator
    const double cents = detune.next() + pitch.tick (t);
    const double hz = kBaseHz * std::pow (2.0, cents / 1200.0);
    int lvl = 0;
    while (lvl < Tables::kLevels - 1 && tables.maxHarm[lvl + 1] * hz < sr * 0.5)
        ++lvl;
    const auto& tab = pulse ? tables.pulse[lvl] : tables.saw[lvl];
    const double pos = phase * Tables::kSize;
    const int i0 = (int) pos;
    const float frac = (float) (pos - i0);
    const float osc = tab[(size_t) i0] + (tab[(size_t) i0 + 1] - tab[(size_t) i0]) * frac;
    phase += hz / sr;
    phase -= std::floor (phase);

    // Filter envelope (in cents) drives both biquads' detune and f1's Q.
    const float envCents = env.tick (t);
    const float fq1 = f1Freq.next(), q1 = f1Q.next(), qi = qIn.next(), qo = qOut.next();
    const float fq2 = f2Freq.next(), q2 = f2Q.next();
    const double mul = std::pow (2.0, envCents / 1200.0);
    const float qMod = qo * clampf (envCents * qi, -1.0f, 1.0f);
    f1.update (fq1 * mul, q1 + qMod);
    f2.update (fq2 * mul, q2);

    float y = f2.process (f1.process (osc));
    y *= vca.tick (t) * level.next();

    // 90 Hz one-pole highpass
    const float out = hpB0 * y + hpB1 * hpX + hpA1 * hpY;
    hpX = y;
    hpY = out;
    return out;
}

} // namespace rb
