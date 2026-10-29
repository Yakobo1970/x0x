#include "DrumKit.h"

namespace rb
{
namespace
{
    constexpr double Ee = 1.0 / 6.9; // decay times are "time to -60 dB"
    constexpr double kMetal[6] = { 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 };

    inline double lvl (double x) { return x * x * 1.3; } // b()
    inline double we (double e, double t, double n, double r) { return t + e * n + std::pow (e, 8.0) * r; }

    inline float polyBlep (double t, double dt)
    {
        if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    inline float square (double& phase, double hz, double sr)
    {
        const double dt = std::min (0.5, hz / sr);
        float v = phase < 0.5 ? 1.0f : -1.0f;
        v += polyBlep (phase, dt);
        v -= polyBlep (std::fmod (phase + 0.5, 1.0), dt);
        phase += dt;
        phase -= std::floor (phase);
        return v;
    }
}

float DrumLine::source (double t, double sr)
{
    switch (src)
    {
        case Sine:
        {
            const float v = (float) std::sin (2.0 * kPi * phase[0]);
            phase[0] += freq.tick (t) / sr;
            phase[0] -= std::floor (phase[0]);
            return v;
        }
        case Square:
            return square (phase[0], sqFreq, sr);
        case Metal:
        {
            float s = 0.0f;
            for (int i = 0; i < 6; ++i)
                s += square (phase[i], kMetal[i] * metalScale, sr);
            return s / 6.0f;
        }
        case NoiseSrc:
        default:
            return noise.next();
    }
}

DrumKit::DrumKit (int dev) : device (dev)
{
    const juce::String prefix = juce::String (kDeviceIds[dev]) + ".";
    for (int i = 0; i < (int) paramDefs().size(); ++i)
    {
        const auto& id = paramDefs()[(size_t) i].id;
        if (id.startsWith (prefix))
            knobIndex[id.substring (prefix.length()).toStdString()] = i;
    }
}

void DrumKit::prepare (double sampleRate)
{
    sr = sampleRate;
    for (auto& h : hits)
    {
        h.bus.prepare (sr);
        h.choke.prepare (sr);
        for (auto& l : h.lines)
        {
            l.freq.prepare (sr);
            l.env.prepare (sr);
        }
    }
    reset();
}

void DrumKit::reset()
{
    for (auto& h : hits)
        h.active = false;
    openHat = -1;
}

float DrumKit::p (const char* key, const ParamView& pv) const
{
    auto it = knobIndex.find (key);
    jassert (it != knobIndex.end());
    return it == knobIndex.end() ? 0.0f : pv[it->second];
}

DrumHit* DrumKit::allocHit (double t)
{
    DrumHit* best = nullptr;
    for (auto& h : hits)
        if (! h.active) { best = &h; break; }
    if (best == nullptr)
    {
        best = &hits[0];
        for (auto& h : hits)
            if (h.gen < best->gen)
                best = &h;
    }
    best->active = true;
    best->gen = genCounter++;
    best->numLines = 0;
    best->hasBus = false;
    best->bus.reset (1.0f);
    best->choke.reset (1.0f);
    best->end = t;
    return best;
}

DrumLine& DrumKit::addLine (Ctx& c, DrumLine::Src src)
{
    auto& h = *c.hit;
    jassert (h.numLines < DrumHit::kMaxLines);
    auto& l = h.lines[std::min (h.numLines, DrumHit::kMaxLines - 1)];
    h.numLines = std::min (h.numLines + 1, DrumHit::kMaxLines);
    l.src = src;
    l.numFilters = 0;
    l.gain = 1.0f;
    l.hasEnv = false;
    l.viaBus = false;
    l.env.reset (1.0f);
    l.freq.reset (0.0f);
    for (auto& ph : l.phase)
        ph = 0.0;
    l.noise.seed (noiseSeed = noiseSeed * 1664525u + 1013904223u);
    return l;
}

void DrumKit::addFilter (DrumLine& l, Biquad::Type type, double hz, double q)
{
    if (l.numFilters >= 3)
        return;
    auto& f = l.filt[l.numFilters++];
    f.setup (type, hz, q, sr);
    f.reset();
}

void DrumKit::run (DrumLine& l, double t, double dur)
{
    l.start = t;
    l.stop = t + dur;
}

void DrumKit::oe (AutoParam& g, double level, double decay, double attack, double t)
{
    g.setValueAtTime (0.0f, t);
    g.linearRampToValueAtTime ((float) level, t + attack);
    g.setTargetAtTime (0.0f, t + attack, decay * Ee);
}

void DrumKit::ke (AutoParam& g, double level, double n, double ratio, double decay, double attack, double t)
{
    g.setValueAtTime (0.0f, t);
    g.linearRampToValueAtTime ((float) level, t + attack);
    g.setTargetAtTime ((float) (level * ratio), t + attack, n * Ee);
    g.setTargetAtTime (0.0f, t + attack + n * 0.5, decay * Ee);
}

//==============================================================================
void DrumKit::kick (Ctx& c, double base, double sweep, double sweepTime, double settle, double decay, double level,
                    double click, double clickHz, double harm)
{
    const std::pair<double, double> harmonics[] = { { 1.0, 1.0 }, { 2.0, harm } };
    for (int k = 0; k < (harm > 0.0 ? 2 : 1); ++k)
    {
        const auto [mult, amt] = harmonics[k];
        auto& l = addLine (c, DrumLine::Sine);
        l.freq.setValueAtTime ((float) (base * sweep * mult), c.t);
        if (settle > 0.0)
            l.freq.setTargetAtTime ((float) (base * mult), c.t, settle);
        else
            l.freq.exponentialRampToValueAtTime ((float) (base * mult), c.t + sweepTime);
        l.hasEnv = true;
        oe (l.env, level * amt, decay, 0.001, c.t);
        run (l, c.t, decay + 0.05);
    }
    if (click > 0.0)
    {
        auto& l = addLine (c, DrumLine::NoiseSrc);
        addFilter (l, Biquad::Lowpass, clickHz, 0.7);
        l.hasEnv = true;
        oe (l.env, level * click, 0.012, 5e-4, c.t);
        run (l, c.t, 0.03);
    }
}

void DrumKit::snare (Ctx& c, std::initializer_list<Tone> tones, double sweep, double noiseHp, double noiseLp,
                     double noiseDecay, double noiseLevel)
{
    for (auto& tone : tones)
    {
        auto& l = addLine (c, DrumLine::Sine);
        l.freq.setValueAtTime ((float) (tone.hz * sweep), c.t);
        l.freq.exponentialRampToValueAtTime ((float) tone.hz, c.t + 0.02);
        l.hasEnv = true;
        if (tone.fast > 0.0)
            ke (l.env, tone.level, tone.fast, 0.2, tone.decay, 0.0015, c.t);
        else
            oe (l.env, tone.level, tone.decay, 0.0015, c.t);
        run (l, c.t, tone.decay + 0.05);
    }
    auto& n = addLine (c, DrumLine::NoiseSrc);
    addFilter (n, Biquad::Highpass, noiseHp, 0.7);
    addFilter (n, Biquad::Lowpass, noiseLp, 0.7);
    n.hasEnv = true;
    oe (n.env, noiseLevel, noiseDecay, 0.0015, c.t);
    run (n, c.t, noiseDecay + 0.05);
}

void DrumKit::tom (Ctx& c, double hz, double decay, double level, double noiseAmt, double sweep, double sweepTime,
                   double click, double attack)
{
    auto& l = addLine (c, DrumLine::Sine);
    l.freq.setValueAtTime ((float) (hz * sweep), c.t);
    l.freq.exponentialRampToValueAtTime ((float) hz, c.t + sweepTime);
    l.hasEnv = true;
    oe (l.env, level, decay, attack, c.t);
    run (l, c.t, decay + 0.05);
    if (click > 0.0)
    {
        auto& n = addLine (c, DrumLine::NoiseSrc);
        addFilter (n, Biquad::Lowpass, 6000.0, 0.7);
        n.hasEnv = true;
        oe (n.env, level * click, 0.04, 5e-4, c.t);
        run (n, c.t, 0.06);
    }
    if (noiseAmt > 0.0)
    {
        auto& n = addLine (c, DrumLine::NoiseSrc);
        addFilter (n, Biquad::Bandpass, hz * 4.0, 1.0);
        n.hasEnv = true;
        oe (n.env, level * noiseAmt, decay * 0.3, 0.0015, c.t);
        run (n, c.t, decay * 0.3 + 0.05);
    }
}

void DrumKit::partials (Ctx& c, std::initializer_list<std::pair<double, double>> parts, double decay, double level,
                        double tick, double tickHz)
{
    for (auto& [hz, amt] : parts)
    {
        auto& l = addLine (c, DrumLine::Sine);
        l.freq.setValueAtTime ((float) hz, c.t);
        l.hasEnv = true;
        oe (l.env, level * amt, decay, 5e-4, c.t);
        run (l, c.t, decay + 0.05);
    }
    if (tick > 0.0)
    {
        auto& n = addLine (c, DrumLine::NoiseSrc);
        addFilter (n, Biquad::Bandpass, tickHz, 0.8);
        n.hasEnv = true;
        oe (n.env, level * tick, 0.01, 5e-4, c.t);
        run (n, c.t, 0.03);
    }
}

void DrumKit::clap (Ctx& c, double level, double bp, double decay, double q, double tail, double hp)
{
    auto& n = addLine (c, DrumLine::NoiseSrc);
    addFilter (n, Biquad::Bandpass, bp, q);
    addFilter (n, Biquad::Highpass, hp, 0.7);
    n.hasEnv = true;
    auto& g = n.env;
    g.setValueAtTime (0.0f, c.t);
    for (int k = 0; k < 3; ++k)
    {
        const double r = c.t + k * 0.011;
        g.setValueAtTime ((float) level, r);
        g.setTargetAtTime (0.0f, r + 0.001, 0.0035);
    }
    g.setValueAtTime ((float) (level * tail), c.t + 0.033);
    g.setTargetAtTime (0.0f, c.t + 0.034, decay * Ee);
    run (n, c.t, 0.04 + decay);
}

void DrumKit::hat (Ctx& c, double level, double decay, double scale, double bp, double hp, double lp,
                   double noiseMix, double metalMix, bool choke, bool chokeable)
{
    auto& h = *c.hit;
    h.hasBus = true;
    oe (h.bus, level, decay, 8e-4, c.t);

    auto& m = addLine (c, DrumLine::Metal);
    m.metalScale = (float) scale;
    addFilter (m, Biquad::Bandpass, bp, 1.0);
    addFilter (m, Biquad::Highpass, hp, 0.7);
    addFilter (m, Biquad::Lowpass, lp, 0.7);
    m.gain = (float) metalMix;
    m.viaBus = true;
    run (m, c.t, decay + 0.05);

    if (noiseMix > 0.0)
    {
        auto& n = addLine (c, DrumLine::NoiseSrc);
        addFilter (n, Biquad::Highpass, hp, 0.7);
        addFilter (n, Biquad::Lowpass, lp, 0.7);
        n.gain = (float) noiseMix;
        n.viaBus = true;
        run (n, c.t, decay + 0.05);
    }

    if (choke && openHat >= 0 && hits[openHat].active && hits[openHat].gen == openHatGen)
    {
        hits[openHat].choke.setValueAtTime (1.0f, c.t);
        hits[openHat].choke.setTargetAtTime (0.0f, c.t, 0.004);
    }
    if (chokeable)
    {
        openHat = (int) (c.hit - hits);
        openHatGen = c.hit->gen;
    }
}

void DrumKit::cymbal (Ctx& c, double level, double decay, double scale, double hp, double bp, double noiseMix,
                      double fast, double tail)
{
    auto& h = *c.hit;
    h.hasBus = true;
    if (fast > 0.0)
        ke (h.bus, level, fast, tail, decay, 0.001, c.t);
    else
        oe (h.bus, level, decay, 0.001, c.t);

    auto& m = addLine (c, DrumLine::Metal);
    m.metalScale = (float) scale;
    addFilter (m, Biquad::Bandpass, bp, 0.6);
    addFilter (m, Biquad::Highpass, hp, 0.7);
    m.viaBus = true;
    run (m, c.t, decay + 0.05);

    if (noiseMix > 0.0)
    {
        auto& n = addLine (c, DrumLine::NoiseSrc);
        addFilter (n, Biquad::Highpass, hp, 0.7);
        n.gain = (float) noiseMix;
        n.viaBus = true;
        run (n, c.t, decay + 0.05);
    }
}

void DrumKit::noiseHit (Ctx& c, double level, double decay, double hp)
{
    auto& n = addLine (c, DrumLine::NoiseSrc);
    addFilter (n, Biquad::Highpass, hp, 0.7);
    n.hasEnv = true;
    oe (n.env, level, decay, 0.002, c.t);
    run (n, c.t, decay + 0.05);
}

void DrumKit::cowbell (Ctx& c, double level)
{
    auto& h = *c.hit;
    h.hasBus = true;
    ke (h.bus, level, 0.06, 0.18, 0.9, 0.001, c.t);
    for (double hz : { 540.0, 800.0 })
    {
        auto& l = addLine (c, DrumLine::Square);
        l.sqFreq = (float) hz;
        addFilter (l, Biquad::Bandpass, 1000.0, 1.0);
        l.viaBus = true;
        run (l, c.t, 1.0);
    }
}

//==============================================================================
void DrumKit::trigger (int lane, double t, float acc, const ParamView& pv)
{
    const std::string id = drumLanes (device)[lane];
    if (id == "ac")
        return;

    Ctx c { allocHit (t), t, acc };
    auto P = [&] (const char* k) { return (double) p (k, pv); };
    const double a = acc;

    if (device == R808)
    {
        auto alt = [&] (const char* k) { return P (k) >= 0.5; };
        auto tom808 = [&] (const char* lvlKey, const char* tuneKey, double hz, double decay, double mult) {
            tom (c, hz * (0.8 + P (tuneKey) * 0.5), decay, lvl (P (lvlKey)) * a * mult, 0.0, 1.04, 0.02, 0.0, 0.005);
        };

        if (id == "bd")
            kick (c, 50 + P ("bd.tone") * 6, 1.5, 0.03, 0.0, 0.12 * std::pow (4.25, 2 * P ("bd.decay")),
                  lvl (P ("bd.level")) * a * 1.4, P ("bd.tone") * 0.1, 3500, 0.0);
        else if (id == "sd")
        {
            const double n = lvl (P ("sd.level")) * a;
            snare (c, { { 179, n * 0.8, 0.2, 0 }, { 332, n * 0.8 * std::pow (10.0, (-15 + 22 * P ("sd.tone")) / 20), 0.07, 0 } },
                   1.0, 1800, 6000 + P ("sd.tone") * 6000, 0.18, n * P ("sd.snappy"));
        }
        else if (id == "lt")
            alt ("lt.alt") ? tom808 ("lt.level", "lt.tuning", 174, 0.45, 1.1) : tom808 ("lt.level", "lt.tuning", 80, 0.57, 0.7);
        else if (id == "mt")
            alt ("mt.alt") ? tom808 ("mt.level", "mt.tuning", 256, 0.38, 0.7) : tom808 ("mt.level", "mt.tuning", 123, 0.4, 0.68);
        else if (id == "ht")
            alt ("ht.alt") ? tom808 ("ht.level", "ht.tuning", 363, 0.2, 1.1) : tom808 ("ht.level", "ht.tuning", 166, 0.3, 0.77);
        else if (id == "rs")
        {
            if (alt ("rs.alt"))
                partials (c, { { 2517, 1 } }, 0.06, lvl (P ("rs.level")) * a * 0.5, 0, 2500);
            else
                partials (c, { { 495, 1 }, { 1700, 0.35 } }, 0.035, lvl (P ("rs.level")) * a * 0.9, 0.6, 3000);
        }
        else if (id == "cp")
        {
            if (alt ("cp.alt"))
                noiseHit (c, lvl (P ("cp.level")) * a, 0.07, 8000);
            else
                clap (c, lvl (P ("cp.level")) * a * 0.9, 1400, 0.3, 0.5, 0.5, 200);
        }
        else if (id == "cb")
            cowbell (c, lvl (P ("cb.level")) * a * 0.6);
        else if (id == "cy")
        {
            const double n = we (P ("cy.decay"), 0.27, 0, 0.78);
            cymbal (c, lvl (P ("cy.level")) * a, n * 6.2, 1.0, 2500 + P ("cy.tone") * 3000, 7000 + P ("cy.tone") * 2000, 0.2, n, 0.06);
        }
        else if (id == "oh")
            hat (c, lvl (P ("oh.level")) * a * 1.1, we (P ("oh.decay"), 0.15, 0.6, 3.6), 1.0, 8000, 6000, 20000, 0, 1, false, true);
        else if (id == "ch")
            hat (c, lvl (P ("ch.level")) * a * 4.4, 0.1, 1.0, 11000, 8000, 20000, 0, 1, true, false);
    }
    else
    {
        auto tom909 = [&] (const char* g, double hz, double decay, double mult, double sweep, double sweepTime, double click) {
            const std::string k (g);
            tom (c, hz * (0.8 + P ((k + ".tune").c_str()) * 0.5), decay * (0.3 + P ((k + ".decay").c_str()) * 0.7),
                 lvl (P ((k + ".level").c_str())) * a * mult, 0.0, sweep, sweepTime, click, 0.0015);
        };

        if (id == "bd")
            kick (c, 56, 4.9, 0.0, 0.005 + 0.025 * std::pow (std::min (1.0, P ("bd.tune") / 0.78), 2.5),
                  0.25 + P ("bd.decay") * 0.85, lvl (P ("bd.level")) * a * 0.75, 0.06 * std::pow (P ("bd.attack"), 0.45), 7000, 0.07);
        else if (id == "sd")
        {
            const double n = lvl (P ("sd.level")) * a;
            const double r = 122 * (1 + 0.975 * std::pow (P ("sd.tune"), 1.1));
            snare (c, { { r, n * 1.05, 0.45, 0.06 }, { r * 1.35, n * 0.37, 0.3, 0.06 } }, 1.3, 1200,
                   3000 + P ("sd.tone") * 9000, 0.18, n * (0.02 + P ("sd.snappy") * 0.45));
        }
        else if (id == "lt")
            tom909 ("lt", 90, 0.75, 1.6, 1.2, 0.04, 0.1);
        else if (id == "mt")
            tom909 ("mt", 108, 0.55, 1.5, 1.2, 0.04, 0.12);
        else if (id == "ht")
            tom909 ("ht", 122, 0.5, 1.9, 1.3, 0.05, 0.12);
        else if (id == "rs")
            partials (c, { { 221, 1 }, { 480, 0.45 }, { 1000, 0.2 }, { 1700, 0.1 } }, 0.07, lvl (P ("rs.level")) * a * 1.1, 0.25, 2500);
        else if (id == "cp")
            clap (c, lvl (P ("cp.level")) * a * 6.5, 1100, 0.75, 1.3, 0.12, 600);
        else if (id == "ch")
            hat (c, lvl (P ("hh.level")) * a * 1.05, 0.03 + P ("hh.chDecay") * 0.25, 1.3, 10000, 2400, 11000, 1, 0.35, true, false);
        else if (id == "oh")
            hat (c, lvl (P ("hh.level")) * a * 0.85, 0.25 + P ("hh.ohDecay") * 0.6, 1.3, 10000, 2600, 12000, 1, 0.35, false, true);
        else if (id == "cr")
            cymbal (c, lvl (P ("cy.crLevel")) * a * 0.7, 2.1, 1.7 * (0.7 + P ("cy.crTune") * 0.6), 1500, 5000, 0.5, 0, 1);
        else if (id == "rd")
            cymbal (c, lvl (P ("cy.rdLevel")) * a * 1.2, 2.0, 2.3 * (0.7 + P ("cy.rdTune") * 0.6), 2500, 8000, 0.15, 0, 1);
    }

    for (int i = 0; i < c.hit->numLines; ++i)
        c.hit->end = std::max (c.hit->end, c.hit->lines[i].stop);
    if (c.hit->numLines == 0)
        c.hit->active = false;
}

float DrumKit::render (double t)
{
    float out = 0.0f;
    for (auto& h : hits)
    {
        if (! h.active)
            continue;
        if (t >= h.end)
        {
            h.active = false;
            continue;
        }
        const float busGain = h.hasBus ? h.bus.tick (t) : 1.0f;
        const float chokeGain = h.choke.tick (t);
        float sum = 0.0f;
        for (int i = 0; i < h.numLines; ++i)
        {
            auto& l = h.lines[i];
            if (t < l.start || t >= l.stop)
                continue;
            float x = l.source (t, sr);
            for (int f = 0; f < l.numFilters; ++f)
                x = l.filt[f].process (x);
            x *= l.gain;
            if (l.hasEnv)
                x *= l.env.tick (t);
            sum += l.viaBus ? x * busGain : x;
        }
        out += sum * chokeGain;
    }
    return out;
}

} // namespace rb
