// Insert effects (distortion, compressor, pattern controlled filter), the
// tempo-synced delay and the master limiter of the web app.
#pragma once

#include "Core.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>

namespace rb
{
//==============================================================================
// Distortion: hard clip or wave-folder depending on SHAPE, 4x oversampled.
class Distortion
{
public:
    struct Settings { float driveClip, driveFold, mixClip, mixFold, shape; };

    void prepare (double sr, int maxBlock)
    {
        os = std::make_unique<juce::dsp::Oversampling<float>> (2, kFactorLog2,
                juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        os->initProcessing ((size_t) maxBlock);
        const double osr = sr * (1 << kFactorLog2);
        for (auto& lp : foldLp) lp.setup (4881.0, osr);
        for (auto* s : { &driveClip, &driveFold, &mixClip, &mixFold })
            s->prepare (osr);
    }

    void reset()
    {
        if (os) os->reset();
        for (auto& lp : foldLp) lp.reset();
    }

    void set (const Settings& s, bool snap)
    {
        driveClip.setTarget (s.driveClip); driveFold.setTarget (s.driveFold);
        mixClip.setTarget (s.mixClip); mixFold.setTarget (s.mixFold);
        if (snap)
            for (auto* p : { &driveClip, &driveFold, &mixClip, &mixFold })
                p->snap (p->targetValue());
        if (s.shape != shape)
        {
            shape = s.shape;
            foldR = (float) (6.267 * std::pow ((double) shape, 1.661));
            foldDry = (float) (0.691 * std::pow (1.0 - shape, 0.82));
        }
    }

    // Processes a stereo block in place (returns the wet signal).
    void process (float* l, float* r, int n)
    {
        float* ch[2] = { l, r };
        juce::dsp::AudioBlock<float> block (ch, 2, (size_t) n);
        auto up = os->processSamplesUp (block);
        const int m = (int) up.getNumSamples();
        float* ul = up.getChannelPointer (0);
        float* ur = up.getChannelPointer (1);
        for (int i = 0; i < m; ++i)
        {
            const float dc = driveClip.next(), df = driveFold.next(), mc = mixClip.next(), mf = mixFold.next();
            ul[i] = shapeSample (ul[i], dc, df, mc, mf, foldLp[0]);
            ur[i] = shapeSample (ur[i], dc, df, mc, mf, foldLp[1]);
        }
        os->processSamplesDown (block);
    }

private:
    float shapeSample (float x, float dc, float df, float mc, float mf, OnePoleLP& lp) const
    {
        const float clip = clampf (x * dc, -1.0f, 1.0f) * mc;
        const float t = clampf (x * df, -1.0f, 1.0f) * 3.446f;
        const float fold = 2.505f * std::sin (foldR * t) + foldDry * t;
        return clip + lp.process (fold) * mf;
    }

    static constexpr int kFactorLog2 = 2;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    OnePoleLP foldLp[2];
    Smoothed driveClip, driveFold, mixClip, mixFold;
    float shape = -1.0f, foldR = 0.0f, foldDry = 0.0f;
};

//==============================================================================
// Port of the "rebirth-compressor" AudioWorklet (RMS detector, soft knee).
class Compressor
{
public:
    struct Settings { float thresholdDb, slope, makeupDb, offsetDb; };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        auto coef = [&] (double s) { return (float) (1.0 - std::exp (-1.0 / (std::max (s, 1.0e-4) * sr))); };
        cDet = coef (0.015);
        cAtt = coef (0.03);
        cRel = coef (0.037);
        for (auto* s : { &thr, &slope, &makeup, &offset })
            s->prepare (sr);
    }

    void reset() { ms = 0.0; g = 0.0f; }

    void set (const Settings& s, bool snap)
    {
        thr.setTarget (s.thresholdDb); slope.setTarget (s.slope);
        makeup.setTarget (s.makeupDb); offset.setTarget (s.offsetDb);
        if (snap)
            for (auto* p : { &thr, &slope, &makeup, &offset })
                p->snap (p->targetValue());
    }

    void process (float& l, float& r)
    {
        const float d = thr.next(), p = slope.next(), m = makeup.next(), h = offset.next();
        constexpr float knee = 7.1f;
        ms += ((l * l + r * r) * 0.5 - ms) * cDet;
        const float n = (float) (10.0 * std::log10 (ms + 1e-20)) + h - d;
        const float o = m - p * (n > knee / 2 ? n : n < -knee / 2 ? 0.0f : (n + knee / 2) * (n + knee / 2) / (2 * knee));
        g += (o - g) * (o < g ? cAtt : cRel);
        const float gain = std::pow (10.0f, g / 20.0f);
        l *= gain;
        r *= gain;
        reduction = std::min (0.0f, g - m);
    }

    float reduction = 0.0f; // for metering

private:
    double sr = 44100.0, ms = 0.0;
    float g = 0.0f, cDet = 0, cAtt = 0, cRel = 0;
    Smoothed thr, slope, makeup, offset;
};

//==============================================================================
// Pattern controlled filter: a biquad whose detune is kicked per half-step.
class PatternFilter
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        detune.prepare (sr);
        for (auto* s : { &freq, &q, &makeup })
            s->prepare (sr);
        reset();
    }

    void reset()
    {
        detune.reset (0.0f);
        for (int c = 0; c < 2; ++c)
        {
            f[c].setup (lowpass ? Biquad::Lowpass : Biquad::Bandpass, 1000.0, 1.0, sr);
            f[c].reset();
        }
    }

    void set (bool lp, float hz, float qVal, float makeupGain, bool snap)
    {
        if (lp != lowpass)
        {
            lowpass = lp;
            for (auto& b : f)
                b.setup (lowpass ? Biquad::Lowpass : Biquad::Bandpass, freq.value(), q.value(), sr);
        }
        freq.setTarget (hz); q.setTarget (qVal); makeup.setTarget (makeupGain);
        if (snap)
            for (auto* p : { &freq, &q, &makeup })
                p->snap (p->targetValue());
    }

    void process (float& l, float& r, double t)
    {
        const double hz = freq.next() * std::pow (2.0, detune.tick (t) / 1200.0);
        const float qv = q.next(), g = makeup.next();
        if (++counter >= 4) // coefficient refresh every 4 samples is inaudible and cheap
        {
            counter = 0;
            for (auto& b : f)
                b.update (hz, qv);
        }
        l = f[0].process (l) * g;
        r = f[1].process (r) * g;
        currentHz = (float) hz;
    }

    AutoParam detune;
    float currentHz = 0.0f;

private:
    double sr = 44100.0;
    bool lowpass = true;
    Biquad f[2];
    Smoothed freq, q, makeup;
    int counter = 0;
};

//==============================================================================
// Stereo feedback delay with a smoothed, fractionally read delay time.
class Delay
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        size = (int) (sr * 12.0) + 4;
        buf[0].assign ((size_t) size, 0.0f);
        buf[1].assign ((size_t) size, 0.0f);
        time.prepare (sr, 0.05);
        for (auto* s : { &inGain, &feedback, &pan })
            s->prepare (sr);
        inGain.prepare (sr, 0.01);
        write = 0;
    }

    void reset()
    {
        std::fill (buf[0].begin(), buf[0].end(), 0.0f);
        std::fill (buf[1].begin(), buf[1].end(), 0.0f);
    }

    void set (float seconds, float on, float fb, float panPos, bool snap)
    {
        time.setTarget (seconds); inGain.setTarget (on); feedback.setTarget (fb); pan.setTarget (panPos);
        if (snap)
            for (auto* p : { &time, &inGain, &feedback, &pan })
                p->snap (p->targetValue());
    }

    void process (float inL, float inR, float& outL, float& outR)
    {
        const float g = inGain.next(), fb = feedback.next(), pn = pan.next();
        const double d = std::clamp ((double) time.next() * sr, 128.0, (double) size - 2.0);
        double rp = write - d;
        if (rp < 0) rp += size;
        const int i0 = (int) rp;
        const int i1 = (i0 + 1) % size;
        const float fr = (float) (rp - i0);
        const float yl = buf[0][(size_t) i0] + (buf[0][(size_t) i1] - buf[0][(size_t) i0]) * fr;
        const float yr = buf[1][(size_t) i0] + (buf[1][(size_t) i1] - buf[1][(size_t) i0]) * fr;
        buf[0][(size_t) write] = inL * g + yl * fb;
        buf[1][(size_t) write] = inR * g + yr * fb;
        if (++write >= size) write = 0;
        outL = yl;
        outR = yr;
        stereoPan (pn, outL, outR);
    }

private:
    double sr = 44100.0;
    int size = 1, write = 0;
    std::vector<float> buf[2];
    Smoothed time, inGain, feedback, pan;
};

//==============================================================================
// Approximation of the DynamicsCompressorNode used as master limiter
// (threshold -3 dB, ratio 20, hard knee, 2 ms / 120 ms) incl. its auto makeup.
class MasterLimiter
{
public:
    void prepare (double sr)
    {
        att = (float) (1.0 - std::exp (-1.0 / (0.002 * sr)));
        rel = (float) (1.0 - std::exp (-1.0 / (0.12 * sr)));
        env = 0.0f;
    }

    void process (float& l, float& r)
    {
        const float peak = std::max (std::abs (l), std::abs (r));
        const float db = 20.0f * std::log10 (std::max (peak, 1.0e-9f));
        const float over = std::max (0.0f, db - kThreshold);
        const float red = over - over / kRatio; // dB of gain reduction wanted
        env += (red - env) * (red > env ? att : rel);
        const float gain = std::pow (10.0f, (-env) / 20.0f) * kMakeup;
        l *= gain;
        r *= gain;
    }

private:
    static constexpr float kThreshold = -3.0f, kRatio = 20.0f;
    static constexpr float kMakeup = 1.218f; // (1 / curve(0 dBFS))^0.6 like Chromium
    float att = 0, rel = 0, env = 0;
};

// Soft clipper curve nt(0.6) at the very end of the chain.
inline float softClip (float x)
{
    constexpr float k = 0.6f;
    const float a = std::min (1.0f, std::abs (x));
    const float o = a < k ? a : k + (1 - k) * std::tanh ((a - k) / (1 - k));
    return std::copysign (o, x);
}

} // namespace rb
