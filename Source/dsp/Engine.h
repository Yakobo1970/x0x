// The complete x0x rack: two bass voices, two drum kits, the step
// sequencer, per-channel insert chains, mixer, delay and master section.
#pragma once

#include "BassVoice.h"
#include "DrumKit.h"
#include "Effects.h"
#include "../Params.h"
#include "../Patterns.h"
#include "../Song.h"
#include <atomic>
#include <memory>

namespace rb
{
struct TransportInfo
{
    bool hostPlaying = false;
    bool hasPpq = false;
    double ppq = 0.0;
    double bpm = 0.0; // 0 = unknown
};

class Engine
{
public:
    Engine (PatternBank& bank, ParamView params);

    void prepare (double sampleRate, int maxBlock);
    void process (juce::AudioBuffer<float>& out, int start, int numSamples, const TransportInfo& tr, const juce::MidiBuffer& midi);

    // Editor-facing state
    std::atomic<bool> internalPlay { false };
    std::atomic<int> currentPattern[NumDevices] {};
    std::atomic<int> queuedPattern[NumDevices] {};
    std::atomic<int> uiStep[NumDevices] {};
    std::atomic<float> meter[NumDevices + 1] {}; // channels + master (peak, linear)
    std::atomic<float> compReduction { 0.0f };
    std::atomic<float> pcfHz { 0.0f };
    std::atomic<bool> isPlaying { false };
    std::atomic<float> tempo { 125.0f };
    std::atomic<int> singleDevice { -1 }; // play.mode: -1 = whole rack, else the one device played by MIDI notes

    void requestPattern (int device, int index); // from UI or MIDI (queued while playing)
    void audition (int device, int lane);        // drum lane preview (lane index) from the UI

    // Song arrangement (replaced from the message thread, read by the audio thread)
    void setSong (std::shared_ptr<const Song> s);
    std::shared_ptr<const Song> getSong() const;
    std::atomic<int> uiSongTick { -1 };

    // Song automation values for the message thread to mirror into the real parameters
    // (so knobs follow the song). Single producer (audio) / single consumer (message thread).
    struct ParamWrite { int index; float value; };
    int popParamWrites (ParamWrite* dest, int maxCount);

private:
    struct Chain
    {
        Distortion dist;
        Compressor comp;
        PatternFilter pcf;
        Smoothed dryWet[3]; // wet amount per stage: dist, comp, pcf
        void prepare (double sr, int maxBlock)
        {
            dist.prepare (sr, maxBlock);
            comp.prepare (sr);
            pcf.prepare (sr);
            for (auto& s : dryWet) s.prepare (sr, 0.01);
        }
    };

    struct Channel
    {
        Chain chain;
        Smoothed pan, fader, mute, send;
        std::vector<float> bufL, bufR;
        float peak = 0.0f;
    };

    void syncParams (bool snap);
    void onStep (long long k, double t, double stepDur);
    void playStep (double t, double stepDur);
    void playDrums (int device, int pattern, int pos, double t);
    void pcfStep (double t, double stepDur);
    void stopVoices (double t);
    void processChain (Chain& c, float* l, float* r, int n, double t0);
    void renderRange (juce::AudioBuffer<float>& out, int start, int from, int to, double t0);
    int pi (const char* id) const { return paramIndex (id); }
    void songChase (const Song& s, int tick, long long k);
    void songAdvance (const Song& s, int tick);
    void applySongEvent (int track, const SongEvent& e, int tick, int* lastPatternTick);
    void clearOverrides();
    void setAutomatedValue (int param, float value); // song / MIDI CC automation
    void handleMidi (const juce::MidiMessage& msg, double absSample);
    void handleSingleMidi (const juce::MidiMessage& msg, double t);

    PatternBank& bank;
    ParamView pv;
    double sr = 44100.0;
    int maxBlock = 512;
    long long sampleCounter = 0;

    BassVoice bass[2];
    std::unique_ptr<DrumKit> kits[2];
    Channel ch[NumDevices];
    Chain masterChain;
    Delay delay;
    Smoothed delayReturn, masterFader, masterVolume;
    MasterLimiter limiter;
    std::vector<float> masterL, masterR, delayInL, delayInR;
    BassParams bassParams[2];

    // sequencer
    bool wasPlaying = false;
    bool hostWasPlaying = false, hostPpqValid = false;
    double lastHostPpq = 0.0;
    bool resync = true;
    double internalPos = 0.0; // in steps
    double expectedPos = -1.0;
    long long lastStep = -1;
    int devPos[NumDevices] {};
    int lastParamPattern[NumDevices] {};
    double stepDur = 0.125;
    std::atomic<int> auditionReq[2] { -1, -1 };
    bool firstBlock = true;

    // song mode
    mutable juce::SpinLock songLock;
    std::shared_ptr<const Song> song;
    const Song* activeSong = nullptr;
    std::vector<float> overrides;
    std::vector<float> lastSeen; // parameter values seen last block (to detect knob moves)
    static constexpr int kWriteFifoSize = 4096;
    juce::AbstractFifo writeFifo { kWriteFifoSize };
    ParamWrite writeBuffer[kWriteFifoSize] {};
    bool overridesActive = false;
    bool songParamsChanged = false, songChased = false, songWasActive = false;
    bool immediateSwitch[NumDevices] {};
    float blockPeak[NumDevices + 1] {};
    double lastStepSample = -1.0; // absolute sample of the most recent sequencer step
    bool snapUntilFirstStep = false;
    int songCursor[NumSongTracks] {};
    int lastSongTick = -1000;
    int iSongMode = -1;

    // single mode
    int soloDev = -1, iPlayMode = -1;
    int heldNotes[16] {}, numHeld = 0; // bass keys held, oldest first
    int singleStep = 0, pendingStepSync = -1; // pattern position for the pattern filter

    // cached parameter indices
    int iBass[2][8] {}, iPattern[NumDevices] {}, iOn[NumDevices] {};
    int iPan[NumDevices] {}, iSend[NumDevices] {}, iLevel[NumDevices] {}, iMute[NumDevices] {}, iSolo[NumDevices] {}, iDistIns[NumDevices] {};
    int iAcLevel[2] {}, iFlam = -1;
    int iDelayReturn, iMasterLevel, iMasterVolume, iShuffle, iTempo;
    int iDelayOn, iDelaySteps, iDelayFb, iDelayPan, iDelayTriplet;
    int iDistOn, iDistAmount, iDistShape;
    int iCompOn, iCompAmount, iCompThreshold, iCompTarget, iCompRouted;
    int iPcfOn, iPcfFreq, iPcfReso, iPcfAmount, iPcfDecay, iPcfWave, iPcfMode, iPcfTarget;
};

} // namespace rb
