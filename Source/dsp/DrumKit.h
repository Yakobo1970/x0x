// Analog-style drum synthesis for DRUM 08 / DRUM 09, ported from the web
// app's per-hit Web Audio node graphs (oscillators, noise, biquads, envelopes).
#pragma once

#include "Core.h"
#include "../Params.h"
#include "../Patterns.h"
#include <string>
#include <unordered_map>

namespace rb
{
struct DrumLine
{
    enum Src : uint8_t { Sine, Square, Metal, NoiseSrc };

    Src src = Sine;
    AutoParam freq;          // sine frequency automation
    float sqFreq = 0.0f;     // square frequency
    float metalScale = 1.0f; // metal: 6 squares scaled
    double phase[6] {};
    Biquad filt[3];
    int numFilters = 0;
    float gain = 1.0f;       // static gain node
    bool hasEnv = false;
    AutoParam env;
    bool viaBus = false;
    double start = 0.0, stop = 0.0;
    Noise noise;

    float source (double t, double sr);
};

struct DrumHit
{
    static constexpr int kMaxLines = 6;
    bool active = false;
    uint32_t gen = 0;
    int numLines = 0;
    DrumLine lines[kMaxLines];
    bool hasBus = false;
    AutoParam bus;
    AutoParam choke;
    double end = 0.0;
};

class DrumKit
{
public:
    explicit DrumKit (int device); // R808 or R909

    void prepare (double sampleRate);
    void reset();

    // Triggers lane `lane` (index into drumLanes()) at absolute time t.
    void trigger (int lane, double t, float acc, const ParamView& pv);

    float render (double t);

private:
    struct Ctx
    {
        DrumHit* hit;
        double t;
        float acc;
    };

    DrumHit* allocHit (double t);
    DrumLine& addLine (Ctx& c, DrumLine::Src src);
    void addFilter (DrumLine& l, Biquad::Type type, double hz, double q);
    void run (DrumLine& l, double t, double dur);

    void oe (AutoParam& p, double level, double decay, double attack, double t);
    void ke (AutoParam& p, double level, double n, double ratio, double decay, double attack, double t);

    // Voice recipes (Pe, Fe, Ie, Le, Re, ze, Be, Ve and the cowbell)
    void kick (Ctx& c, double base, double sweep, double sweepTime, double settle, double decay, double level,
               double click, double clickHz, double harm);
    struct Tone { double hz, level, decay, fast; };
    void snare (Ctx& c, std::initializer_list<Tone> tones, double sweep, double noiseHp, double noiseLp,
                double noiseDecay, double noiseLevel);
    void tom (Ctx& c, double hz, double decay, double level, double noiseAmt, double sweep, double sweepTime,
              double click, double attack);
    void partials (Ctx& c, std::initializer_list<std::pair<double, double>> parts, double decay, double level,
                   double tick, double tickHz);
    void clap (Ctx& c, double level, double bp, double decay, double q, double tail, double hp);
    void hat (Ctx& c, double level, double decay, double scale, double bp, double hp, double lp, double noiseMix,
              double metalMix, bool choke, bool chokeable);
    void cymbal (Ctx& c, double level, double decay, double scale, double hp, double bp, double noiseMix,
                 double fast, double tail);
    void noiseHit (Ctx& c, double level, double decay, double hp);
    void cowbell (Ctx& c, double level);

    float p (const char* key, const ParamView& pv) const;

    int device;
    double sr = 44100.0;
    static constexpr int kPool = 48;
    DrumHit hits[kPool];
    uint32_t genCounter = 1;
    int openHat = -1;
    uint32_t openHatGen = 0;
    uint32_t noiseSeed = 1;
    std::unordered_map<std::string, int> knobIndex;
};

} // namespace rb
