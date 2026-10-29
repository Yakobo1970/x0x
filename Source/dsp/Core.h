// Small DSP building blocks that mirror the Web Audio primitives used by the
// original Re-Rebirth web app (AudioParam automation, BiquadFilterNode,
// StereoPannerNode, WaveShaper curves).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace rb
{
constexpr double kPi = 3.14159265358979323846;

inline float clampf (float v, float lo, float hi) { return std::min (hi, std::max (lo, v)); }

// Exponential approach coefficient for AudioParam.setTargetAtTime().
inline float targetCoef (double tau, double sr)
{
    return tau <= 0.0 ? 1.0f : (float) (1.0 - std::exp (-1.0 / (tau * sr)));
}

//==============================================================================
// Sample-accurate emulation of a Web Audio AudioParam timeline.
class AutoParam
{
public:
    enum Type : uint8_t { SetValue, LinearRamp, ExpRamp, SetTarget, Hold };

    void prepare (double sampleRate) { sr = sampleRate; }
    void reset (float v)
    {
        n = 0; cur = v; lastT = 0.0; lastV = v; mode = Type::Hold;
    }

    void setValueAtTime (float v, double t)                 { insert ({ SetValue, t, v, 0.0f }); }
    void linearRampToValueAtTime (float v, double t)        { insert ({ LinearRamp, t, v, 0.0f }); }
    void exponentialRampToValueAtTime (float v, double t)   { insert ({ ExpRamp, t, v, 0.0f }); }
    void setTargetAtTime (float v, double t, double tau)    { insert ({ SetTarget, t, v, (float) tau }); }

    void cancelScheduledValues (double t)
    {
        int k = 0;
        for (int i = 0; i < n; ++i)
            if (ev[i].t < t)
                ev[k++] = ev[i];
        n = k;
    }

    void cancelAndHoldAtTime (double t)
    {
        cancelScheduledValues (t);
        insert ({ Hold, t, 0.0f, 0.0f });
    }

    float value() const { return cur; }

    // Advance to absolute time t (seconds) and return the param value there.
    float tick (double t)
    {
        while (n > 0)
        {
            const Ev& e = ev[0];
            if (e.type == LinearRamp || e.type == ExpRamp)
            {
                if (t >= e.t)
                {
                    cur = e.v; lastT = e.t; lastV = cur; mode = Hold;
                    pop();
                    continue;
                }
                const double span = e.t - lastT;
                const double f = span <= 0.0 ? 1.0 : std::clamp ((t - lastT) / span, 0.0, 1.0);
                if (e.type == LinearRamp || lastV * e.v <= 0.0f)
                    cur = (float) (lastV + (e.v - lastV) * f);
                else
                    cur = (float) (lastV * std::pow ((double) e.v / lastV, f));
                return cur;
            }

            if (t < e.t)
                break;

            switch (e.type)
            {
                case SetValue:  cur = e.v; mode = Hold; break;
                case SetTarget: mode = SetTarget; target = e.v; coef = targetCoef (e.tau, sr); break;
                case Hold:      mode = Hold; break;
                default: break;
            }
            lastT = e.t; lastV = cur;
            pop();
        }

        if (mode == SetTarget)
            cur += (target - cur) * coef;
        return cur;
    }

private:
    struct Ev { Type type; double t; float v; float tau; };
    static constexpr int kMax = 24;

    void insert (Ev e)
    {
        if (n == kMax) // drop the oldest event rather than allocate
        {
            pop();
        }
        int i = n;
        while (i > 0 && ev[i - 1].t > e.t)
        {
            ev[i] = ev[i - 1];
            --i;
        }
        ev[i] = e;
        ++n;
    }

    void pop()
    {
        for (int i = 1; i < n; ++i)
            ev[i - 1] = ev[i];
        --n;
    }

    Ev ev[kMax] {};
    int n = 0;
    double sr = 44100.0;
    float cur = 0.0f, lastV = 0.0f, target = 0.0f, coef = 1.0f;
    double lastT = 0.0;
    Type mode = Hold;
};

//==============================================================================
// Knob smoothing: the web app writes every parameter via setTargetAtTime(v, now, 0.012).
class Smoothed
{
public:
    void prepare (double sr, double tau = 0.012) { coef = targetCoef (tau, sr); }
    void setTarget (float v) { target = v; }
    void snap (float v) { target = cur = v; }
    float next() { cur += (target - cur) * coef; return cur; }
    float value() const { return cur; }
    float targetValue() const { return target; }
    bool settled() const { return std::abs (target - cur) < 1.0e-6f; }

private:
    float cur = 0.0f, target = 0.0f, coef = 1.0f;
};

//==============================================================================
// BiquadFilterNode with the exact coefficient formulas of the Web Audio spec.
class Biquad
{
public:
    enum Type : uint8_t { Lowpass, Highpass, Bandpass };

    void setup (Type t, double freq, double q, double sampleRate)
    {
        type = t;
        sr = sampleRate;
        update (freq, q);
    }

    void update (double freq, double q)
    {
        const double nyq = sr * 0.5;
        const double f0 = std::clamp (freq, 0.0, nyq) / nyq; // normalised 0..1
        if (f0 >= 1.0)
        {
            if (type == Lowpass) { setCoefs (1, 0, 0, 1, 0, 0); return; }
            if (type == Highpass || type == Bandpass) { setCoefs (0, 0, 0, 1, 0, 0); return; }
        }
        if (f0 <= 0.0)
        {
            if (type == Lowpass || type == Bandpass) { setCoefs (0, 0, 0, 1, 0, 0); return; }
            setCoefs (1, 0, 0, 1, 0, 0); return;
        }

        const double w0 = kPi * f0;
        const double cw = std::cos (w0), sw = std::sin (w0);

        if (type == Bandpass)
        {
            const double Q = std::max (q, 1.0e-4);
            const double alpha = sw / (2.0 * Q);
            setCoefs (alpha, 0.0, -alpha, 1.0 + alpha, -2.0 * cw, 1.0 - alpha);
            return;
        }

        const double alpha = sw / (2.0 * std::pow (10.0, q / 20.0));
        if (type == Lowpass)
            setCoefs ((1 - cw) * 0.5, 1 - cw, (1 - cw) * 0.5, 1 + alpha, -2 * cw, 1 - alpha);
        else
            setCoefs ((1 + cw) * 0.5, -(1 + cw), (1 + cw) * 0.5, 1 + alpha, -2 * cw, 1 - alpha);
    }

    void reset() { z1 = z2 = 0.0; }

    float process (float x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }

private:
    void setCoefs (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    {
        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0;
        a1 = na1 / na0; a2 = na2 / na0;
    }

    Type type = Lowpass;
    double sr = 44100.0;
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;
};

//==============================================================================
// StereoPannerNode for a stereo input (all channel buses in the app are stereo).
inline void stereoPan (float pan, float& l, float& r)
{
    pan = clampf (pan, -1.0f, 1.0f);
    if (pan <= 0.0f)
    {
        const float x = (float) ((pan + 1.0f) * kPi * 0.5);
        const float gl = std::cos (x), gr = std::sin (x);
        const float il = l, ir = r;
        l = il + ir * gl;
        r = ir * gr;
    }
    else
    {
        const float x = (float) (pan * kPi * 0.5);
        const float gl = std::cos (x), gr = std::sin (x);
        const float il = l, ir = r;
        l = il * gl;
        r = ir + il * gr;
    }
}

// Fast xorshift white noise in [-1, 1).
struct Noise
{
    uint32_t s = 0x12345678u;
    void seed (uint32_t v) { s = v ? v : 0x9e3779b9u; }
    float next()
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) ((double) s / 2147483648.0 - 1.0);
    }
};

// One-pole lowpass as created by the IIRFilterNode helper in the web app.
struct OnePoleLP
{
    float a = 0.0f, z = 0.0f;
    void setup (double hz, double sr) { a = (float) std::exp (-2.0 * kPi * hz / sr); }
    float process (float x) { z = (1.0f - a) * x + a * z; return z; }
    void reset() { z = 0.0f; }
};

} // namespace rb
