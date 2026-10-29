#include "Engine.h"
#include "PcfWaves.h"
#include "../MidiMap.h"

namespace rb
{
namespace
{
    constexpr float kChannelInput[NumDevices] = { 1.0f, 1.0f, 0.55f, 0.55f }; // St
    constexpr float kAccentScale[2] = { 0.8f, 6.0f };                           // xt (808, 909)
    constexpr float kSwing = 0.42f;                                             // pt

    float panPos (float e) { return clampf ((e - 0.5f) * 2.0f * 1.2f, -1.0f, 1.0f); }           // yt
    float faderGain (float e) { return e > 0 ? std::pow (10.0f, (-46.609f * (1 - e)) / 20.0f) : 0.0f; } // wt

    double interp (const double* arr, int len, double t, bool expo) // Ft
    {
        const double r = std::clamp (t, 0.0, 1.0) * (len - 1);
        const int i = std::min (len - 2, (int) std::floor (r));
        const double a = arr[i], o = arr[i + 1];
        return expo ? a * std::pow (o / a, r - i) : a + (o - a) * (r - i);
    }
}

Engine::Engine (PatternBank& b, ParamView params) : bank (b), pv (params)
{
    static const char* bassKnobs[] = { "tuning", "cutoff", "resonance", "envmod", "decay", "accent", "volume", "waveform" };
    for (int d = 0; d < 2; ++d)
        for (int k = 0; k < 8; ++k)
            iBass[d][k] = paramIndex (juce::String (kDeviceIds[d]) + "." + bassKnobs[k]);
    for (int d = 0; d < NumDevices; ++d)
    {
        const juce::String id (kDeviceIds[d]);
        iPattern[d] = paramIndex (id + ".pattern");
        iOn[d] = paramIndex (id + ".on");
        iPan[d] = paramIndex ("mixer." + id + ".pan");
        iSend[d] = paramIndex ("mixer." + id + ".delay");
        iLevel[d] = paramIndex ("mixer." + id + ".level");
        iMute[d] = paramIndex ("mixer." + id + ".mute");
        iSolo[d] = paramIndex ("mixer." + id + ".solo");
        iDistIns[d] = paramIndex ("mixer." + id + ".dist");
    }
    iAcLevel[0] = pi ("r808.ac.level");
    iAcLevel[1] = pi ("r909.ac.level");
    iFlam = pi ("r909.ac.flam");
    iDelayReturn = pi ("mixer.delayReturn");
    iMasterLevel = pi ("mixer.master.level");
    iMasterVolume = pi ("master.volume");
    iShuffle = pi ("song.shuffle");
    iTempo = pi ("tempo");
    iDelayOn = pi ("fx.delay.on");
    iDelaySteps = pi ("fx.delay.steps");
    iDelayFb = pi ("fx.delay.feedback");
    iDelayPan = pi ("fx.delay.pan");
    iDelayTriplet = pi ("fx.delay.triplet");
    iDistOn = pi ("fx.dist.on");
    iDistAmount = pi ("fx.dist.amount");
    iDistShape = pi ("fx.dist.shape");
    iCompOn = pi ("fx.comp.on");
    iCompAmount = pi ("fx.comp.amount");
    iCompThreshold = pi ("fx.comp.threshold");
    iCompTarget = pi ("fx.comp.target");
    iCompRouted = pi ("fx.comp.routed");
    iPcfOn = pi ("fx.pcf.on");
    iPcfFreq = pi ("fx.pcf.freq");
    iPcfReso = pi ("fx.pcf.reso");
    iPcfAmount = pi ("fx.pcf.amount");
    iPcfDecay = pi ("fx.pcf.decay");
    iPcfWave = pi ("fx.pcf.wave");
    iPcfMode = pi ("fx.pcf.mode");
    iPcfTarget = pi ("fx.pcf.target");

    iSongMode = pi ("song.mode");
    iPlayMode = pi ("play.mode");
    overrides.assign (paramDefs().size(), std::numeric_limits<float>::quiet_NaN());
    pv.overrides = overrides.data();
    lastSeen.assign (paramDefs().size(), -1.0f);

    kits[0] = std::make_unique<DrumKit> (R808);
    kits[1] = std::make_unique<DrumKit> (R909);
    for (auto& s : uiStep) s = -1;
}

void Engine::prepare (double sampleRate, int block)
{
    sr = sampleRate;
    maxBlock = std::max (1, block);
    for (auto& b : bass) b.prepare (sr);
    for (auto& k : kits) k->prepare (sr);
    for (auto& c : ch)
    {
        c.chain.prepare (sr, maxBlock);
        for (auto* s : { &c.pan, &c.fader, &c.send }) s->prepare (sr);
        c.mute.prepare (sr, 0.005);
        c.bufL.assign ((size_t) maxBlock, 0.0f);
        c.bufR.assign ((size_t) maxBlock, 0.0f);
    }
    masterChain.prepare (sr, maxBlock);
    delay.prepare (sr);
    for (auto* s : { &delayReturn, &masterFader, &masterVolume }) s->prepare (sr);
    limiter.prepare (sr);
    for (auto* v : { &masterL, &masterR, &delayInL, &delayInR })
        v->assign ((size_t) maxBlock, 0.0f);
    firstBlock = true;
    wasPlaying = false;
    resync = true;
}

void Engine::requestPattern (int device, int index)
{
    queuedPattern[device] = juce::jlimit (0, kPatterns - 1, index);
}

void Engine::audition (int device, int lane)
{
    if (device == R808 || device == R909)
        auditionReq[device - R808] = lane;
}

//==============================================================================
void Engine::syncParams (bool snap)
{
    for (int d = 0; d < 2; ++d)
    {
        auto& p = bassParams[d];
        float* fields[] = { &p.tuning, &p.cutoff, &p.resonance, &p.envmod, &p.decay, &p.accent, &p.volume, &p.waveform };
        for (int k = 0; k < 8; ++k)
            *fields[k] = pv[iBass[d][k]];
        bass[d].setParams (p);
    }

    bool anySolo = false;
    for (int d = 0; d < NumDevices; ++d)
        anySolo |= pv.on (iSolo[d]);

    for (int d = 0; d < NumDevices; ++d)
    {
        auto& c = ch[d];
        const bool audible = pv.on (iOn[d]) && ! pv.on (iMute[d]) && (! anySolo || pv.on (iSolo[d]))
                          && (soloDev < 0 || d == soloDev);
        c.pan.setTarget (panPos (pv[iPan[d]]));
        c.fader.setTarget (faderGain (pv[iLevel[d]]));
        c.send.setTarget (pv[iSend[d]]);
        c.mute.setTarget (audible ? 1.0f : 0.0f);
    }
    masterFader.setTarget (pv[iMasterLevel] * pv[iMasterLevel]);
    masterVolume.setTarget (pv[iMasterVolume] * pv[iMasterVolume] * 0.8f);
    delayReturn.setTarget (pv[iDelayReturn] * 1.48f);

    // Delay
    const int steps = pv.choice (iDelaySteps, 32) + 1;
    const double dt = stepDur * (pv.on (iDelayTriplet) ? 4.0 / 3.0 : 1.0);
    delay.set ((float) std::min (11.9, steps * dt), pv.on (iDelayOn) ? 1.0f : 0.0f, pv[iDelayFb],
               panPos (pv[iDelayPan]), snap);

    // Insert routing
    const int compTarget = (pv.on (iCompOn) && pv.on (iCompRouted)) ? pv.choice (iCompTarget, 5) : -1; // 0 = master
    const int pcfTarget = pv.on (iPcfOn) ? pv.choice (iPcfTarget, 5) : 0;                             // 0 = off

    // Distortion settings (shared by all chains)
    const float amt = pv[iDistAmount];
    const float s = std::pow (10.0f, (24.25f * amt * amt) / 20.0f);
    const float shape = std::round (pv[iDistShape] * 127.0f) / 127.0f;
    const bool fold = shape > 0.0f;
    const Distortion::Settings ds { s / (1.97f * 0.2f), s / (3.446f * 0.2f), fold ? 0.0f : 1.97f * 0.2f,
                                    fold ? 0.2f : 0.0f, shape };

    // Compressor (Ot)
    const float thrKnob = pv[iCompThreshold], compAmt = pv[iCompAmount];
    const float Et = -14.94f;
    const float thr = std::max (-48.94f, 20.0f * std::log10 (std::max (thrKnob, 1e-6f)) + Et);
    const float slope = 0.978f * std::pow (compAmt, 1.34f);
    const float makeup = compAmt * (0.077f * std::pow (Et - thr, 1.57f) - 2.31f);

    // Pattern filter (Lt)
    static const double kt[] = { 0.7, 1.11, 2.06, 4.7, 7.5 }, At[] = { 0.99, 0.94, 1, 1.11, 1.17 },
                        jt[] = { 4.5, 2, -1.6, -4.1, -4.1 }, Mt[] = { 5, 6.5, 8.2, 12.8, 16.9 };
    const double fe = pv[iPcfFreq], re = pv[iPcfReso];
    const double nn = (fe * 127 - 64) / 32, rr = std::min (1.0, re / 0.5);
    const double ii = rr * std::clamp (nn, 0.0, 1.0), aa = rr * std::clamp (-nn, 0.0, 1.0);
    const double pcfHzBase = std::min (14000.0, 379 * std::pow (2.0, nn * 2.65) * interp (At, 5, re, false));
    const double pcfQ = interp (kt, 5, re, true) * std::exp (0.82 * ii - 0.5 * aa);
    const double lpGain = interp (jt, 5, re, false) - 2.4 * ii + 4 * aa;
    const double bpGain = interp (Mt, 5, re, false) + 4.7 * ii - 1.4 * aa;
    const bool lp = pv.on (iPcfMode);

    auto setupChain = [&] (Chain& c, bool distOn, bool compOn, bool pcfOn, bool isMaster) {
        c.dryWet[0].setTarget (distOn ? 1.0f : 0.0f);
        c.dryWet[1].setTarget (compOn ? 1.0f : 0.0f);
        c.dryWet[2].setTarget (pcfOn ? 1.0f : 0.0f);
        if (snap)
            for (auto& w : c.dryWet) w.snap (w.targetValue());
        c.dist.set (ds, snap);
        c.comp.set ({ thr, slope, makeup, isMaster ? -6.64f : -13.64f }, snap);
        c.pcf.set (lp, (float) pcfHzBase, (float) (lp ? 20.0 * std::log10 (pcfQ) : pcfQ),
                   (float) std::pow (10.0, (lp ? lpGain : bpGain) / 20.0), snap);
    };

    for (int d = 0; d < NumDevices; ++d)
        setupChain (ch[d].chain, pv.on (iDistOn) && pv.on (iDistIns[d]), compTarget == d + 1, pcfTarget == d + 1, false);
    setupChain (masterChain, false, compTarget == 0, false, true);

    compReduction = compTarget >= 0 ? (compTarget == 0 ? masterChain : ch[compTarget - 1].chain).comp.reduction : 0.0f;
    pcfHz = pcfTarget > 0 ? std::min (14000.0f, ch[pcfTarget - 1].chain.pcf.currentHz) : 0.0f;

    if (snap)
    {
        for (auto& c : ch)
            for (auto* sm : { &c.pan, &c.fader, &c.send, &c.mute }) sm->snap (sm->targetValue());
        for (auto* sm : { &masterFader, &masterVolume, &delayReturn }) sm->snap (sm->targetValue());
    }
}

//==============================================================================
void Engine::stopVoices (double t)
{
    for (auto& b : bass)
        b.release (t);
    numHeld = 0;
}

void Engine::onStep (long long k, double t, double dur)
{
    if (soloDev >= 0)
    {
        // single mode: the device plays MIDI notes; the clock only drives the pattern filter,
        // kept on the pattern's step grid by the step-sync CC of the note export
        if (resync)
            singleStep = (int) (((k % kSteps) + kSteps) % kSteps);
        if (pendingStepSync >= 0)
            singleStep = pendingStepSync;
        resync = false;
        pendingStepSync = -1;
        for (int d = 0; d < NumDevices; ++d)
        {
            devPos[d] = singleStep;
            uiStep[d] = -1;
        }
        pcfStep (t, dur);
        singleStep = (singleStep + 1) % kSteps;
        return;
    }

    if (activeSong != nullptr && k >= 0)
    {
        const Song& sg = *activeSong;
        int tick = (int) (k * 2);
        if (sg.loop && sg.loopEnd > sg.loopStart && tick >= sg.loopEnd)
            tick = sg.loopStart + (tick - sg.loopStart) % (sg.loopEnd - sg.loopStart);
        if (tick != lastSongTick + 2)
            songChase (sg, tick, k);
        else
            songAdvance (sg, tick);
        lastSongTick = tick;
        uiSongTick = tick;
        resync = false;
        // the step must already sound with the values the song sets on it (like the web app)
        if (songParamsChanged || songChased)
            syncParams (songChased);
        songParamsChanged = songChased = false;
    }

    if (resync)
    {
        for (int d = 0; d < NumDevices; ++d)
        {
            const long long len = bank.length (d, currentPattern[d]);
            if (! immediateSwitch[d])
                devPos[d] = (int) (((k % len) + len) % len);
        }
        resync = false;
    }
    for (auto& f : immediateSwitch)
        f = false;

    for (int d = 0; d < NumDevices; ++d)
    {
        const int len = bank.length (d, currentPattern[d]);
        devPos[d] %= len;
        if (devPos[d] == 0 && queuedPattern[d] != currentPattern[d])
            currentPattern[d] = queuedPattern[d].load();
    }

    playStep (t, dur);

    for (int d = 0; d < NumDevices; ++d)
    {
        uiStep[d] = devPos[d];
        devPos[d] = (devPos[d] + 1) % bank.length (d, currentPattern[d]);
    }
}

void Engine::playStep (double t, double n)
{
    const float shuffleAmt = pv[iShuffle];
    auto swing = [&] (int d, int pos) {
        return (bank.shuffle (d, currentPattern[d]) && (pos % 2)) ? n * kSwing * shuffleAmt : 0.0;
    };

    for (int d = 0; d < 2; ++d)
    {
        auto& pat = bank.bassPattern (d, currentPattern[d]);
        const int len = std::max (1, pat.length.load());
        const int o = devPos[d] % len;
        bass[d].step (pat.step (o), pat.step ((o + 1) % len), t + swing (d, o), n, bassParams[d]);
    }
    for (int d = R808; d <= R909; ++d)
    {
        const int o = devPos[d];
        playDrums (d, currentPattern[d], o, t + swing (d, o));
    }
    pcfStep (t, n);
}

void Engine::playDrums (int device, int pattern, int pos, double t)
{
    const int kit = device - R808;
    auto& pat = bank.drumPattern (device, pattern);
    const float acc = pat.get (0, pos) != Off ? 1.0f + pv[iAcLevel[kit]] * kAccentScale[kit] : 1.0f;
    for (int lane = 1; lane < kDrumLanes; ++lane)
    {
        const uint8_t u = pat.get (lane, pos);
        if (u == Off)
            continue;
        const float a = acc * ((device == R909 && u == Accent) ? 1.55f : 1.0f);
        double tt = t;
        if (device == R909 && u == Flam)
        {
            kits[kit]->trigger (lane, tt, a * 0.5f, pv);
            tt += 0.006 + pv[iFlam] * 0.03;
        }
        kits[kit]->trigger (lane, tt, a, pv);
    }
}

void Engine::pcfStep (double t, double n)
{
    if (! pv.on (iPcfOn))
        return;
    const int target = pv.choice (iPcfTarget, 5);
    if (target == 0)
        return;
    const int dev = target - 1;
    const auto* wave = kPcfWaves[pv.choice (iPcfWave, kNumPcfWaves)];
    const int l = devPos[dev] % 16;
    static const double Pt[] = { 0.01, 0.025, 0.18, 0.9, 1e5 };
    const double decay = interp (Pt, 5, pv[iPcfDecay], true);
    auto& det = ch[dev].chain.pcf.detune;
    for (int e = 0; e < 2; ++e)
    {
        const float v = wave[l * 2 + e];
        if (v == kNoTrig)
            continue;
        const double ti = t + e * n / 2;
        det.cancelScheduledValues (ti);
        det.setValueAtTime ((float) (1200.0 * 9.5 * v * pv[iPcfAmount]), ti);
        det.setTargetAtTime (0.0f, ti, decay);
    }
}

//==============================================================================
void Engine::setSong (std::shared_ptr<const Song> s)
{
    const juce::SpinLock::ScopedLockType lock (songLock);
    song = std::move (s);
}

std::shared_ptr<const Song> Engine::getSong() const
{
    const juce::SpinLock::ScopedLockType lock (songLock);
    return song;
}

void Engine::setAutomatedValue (int param, float value)
{
    overrides[(size_t) param] = value;
    overridesActive = true;
    songParamsChanged = true;
    const auto scope = writeFifo.write (1);
    if (scope.blockSize1 > 0)
        writeBuffer[scope.startIndex1] = { param, value };
    else if (scope.blockSize2 > 0)
        writeBuffer[scope.startIndex2] = { param, value };
}

void Engine::handleMidi (const juce::MidiMessage& msg, double absSample)
{
    if (soloDev >= 0)
    {
        handleSingleMidi (msg, absSample / sr);
        return;
    }
    const int midiCh = msg.getChannel();
    if (msg.isNoteOn() && midiCh >= 1 && midiCh <= NumDevices)
    {
        const int idx = msg.getNoteNumber() - kPatternNoteBase;
        if (idx < 0 || idx >= kPatterns)
            return;
        const int d = midiCh - 1;
        if (msg.getVelocity() >= kImmediateVelocity)
        {
            // switch now and restart the pattern (how song arrangements change patterns)
            queuedPattern[d] = idx;
            currentPattern[d] = idx;
            devPos[d] = 0;
            immediateSwitch[d] = true;
            // A note meant for the step that just started but delivered a few samples late
            // (host rounding) belongs to that step: keep the new pattern in sync from position 1.
            if (wasPlaying && lastStepSample >= 0.0 && absSample - lastStepSample < 0.5 * stepDur * sr)
            {
                devPos[d] = 1 % bank.length (d, idx);
                immediateSwitch[d] = false;
            }
        }
        else
            requestPattern (d, idx);
    }
    else if (msg.isController() && midiCh >= 1 && midiCh <= kMidiMixerChannel)
    {
        const int p = MidiMap::get().paramFor[midiCh][msg.getControllerNumber()];
        if (p >= 0)
            setAutomatedValue (p, MidiMap::toNormalised (msg.getControllerValue()));
    }
}

void Engine::handleSingleMidi (const juce::MidiMessage& msg, double t)
{
    const int d = soloDev;
    if (msg.isAllNotesOff() || msg.isAllSoundOff())
    {
        stopVoices (t);
        return;
    }
    if (msg.isController() && msg.getControllerNumber() == kStepSyncCC)
    {
        pendingStepSync = msg.getControllerValue() % kSteps;
        return;
    }
    if (msg.isController())
    {
        // any channel controls the device; channel 5 stays the mixer + FX channel
        const int midiCh = msg.getChannel() == kMidiMixerChannel ? kMidiMixerChannel : d + 1;
        const int p = MidiMap::get().paramFor[midiCh][msg.getControllerNumber()];
        if (p >= 0)
            setAutomatedValue (p, MidiMap::toNormalised (msg.getControllerValue()));
        return;
    }
    const int note = msg.getNoteNumber();
    if (isBass (d))
    {
        auto& v = bass[d];
        if (msg.isNoteOn())
        {
            const bool legato = numHeld > 0;
            numHeld = (int) (std::remove (heldNotes, heldNotes + numHeld, note) - heldNotes);
            if (numHeld == (int) std::size (heldNotes))
                std::copy (heldNotes + 1, heldNotes + numHeld--, heldNotes);
            heldNotes[numHeld++] = note;
            v.noteOn (t, note - kBassNoteBase, msg.getVelocity() >= kImmediateVelocity, legato, bassParams[d], stepDur);
        }
        else if (msg.isNoteOff() && numHeld > 0)
        {
            const bool wasCurrent = heldNotes[numHeld - 1] == note;
            numHeld = (int) (std::remove (heldNotes, heldNotes + numHeld, note) - heldNotes);
            if (numHeld == 0)
                v.noteOff (t);
            else if (wasCurrent) // back to the key still held, like a mono synth
                v.noteOn (t, heldNotes[numHeld - 1] - kBassNoteBase, false, true, bassParams[d], stepDur);
        }
        return;
    }
    if (! msg.isNoteOn())
        return;
    const int lane = MidiMap::drumLaneFor (d, note);
    if (lane <= 0)
        return;
    const int kit = d - R808;
    const int vel = msg.getVelocity();
    const bool accent = vel >= 120 || (d == R808 && vel >= kImmediateVelocity);
    const bool voiceAccent = d == R909 && (vel == 127 || (vel >= kImmediateVelocity && vel < 120));
    float a = accent ? 1.0f + pv[iAcLevel[kit]] * kAccentScale[kit] : 1.0f;
    if (voiceAccent)
        a *= 1.55f;
    if (vel < 64)
        a *= 0.5f; // flam grace hit
    kits[kit]->trigger (lane, t, a, pv);
}

int Engine::popParamWrites (ParamWrite* dest, int maxCount)
{
    const auto scope = writeFifo.read (std::min (maxCount, writeFifo.getNumReady()));
    int n = 0;
    for (int i = 0; i < scope.blockSize1; ++i) dest[n++] = writeBuffer[scope.startIndex1 + i];
    for (int i = 0; i < scope.blockSize2; ++i) dest[n++] = writeBuffer[scope.startIndex2 + i];
    return n;
}

void Engine::clearOverrides()
{
    std::fill (overrides.begin(), overrides.end(), std::numeric_limits<float>::quiet_NaN());
    overridesActive = false;
}

void Engine::applySongEvent (int track, const SongEvent& e, int tick, int* lastPatternTick)
{
    if (e.param >= 0)
    {
        setAutomatedValue (e.param, e.value);
        return;
    }
    if (track < TrackBass1 || track > Track909)
        return;
    const int dev = track - TrackBass1;
    const int idx = juce::jlimit (0, kPatterns - 1, (int) e.value);
    queuedPattern[dev] = idx;
    currentPattern[dev] = idx;
    devPos[dev] = 0;
    if (lastPatternTick != nullptr)
        lastPatternTick[dev] = tick;
}

void Engine::songChase (const Song& s, int tick, long long k)
{
    clearOverrides();
    songChased = true;
    int lastPat[NumDevices] = { -1, -1, -1, -1 };
    for (int t = 0; t < NumSongTracks; ++t)
    {
        const auto& events = s.tracks[t];
        int i = 0;
        for (; i < (int) events.size() && events[(size_t) i].tick <= tick; ++i)
            applySongEvent (t, events[(size_t) i], events[(size_t) i].tick, lastPat);
        songCursor[t] = i;
    }
    for (int d = 0; d < NumDevices; ++d)
    {
        const int len = bank.length (d, currentPattern[d]);
        const long long steps = lastPat[d] >= 0 ? (tick - lastPat[d]) / 2 : k;
        devPos[d] = (int) (((steps % len) + len) % len);
    }
}

void Engine::songAdvance (const Song& s, int tick)
{
    for (int t = 0; t < NumSongTracks; ++t)
    {
        const auto& events = s.tracks[t];
        int& i = songCursor[t];
        for (; i < (int) events.size() && events[(size_t) i].tick <= tick; ++i)
            applySongEvent (t, events[(size_t) i], tick, nullptr);
    }
}

//==============================================================================
void Engine::processChain (Chain& c, float* l, float* r, int n, double t0)
{
    float tmpL[1024], tmpR[1024]; // processed in sub-blocks to keep these on the stack
    for (int start = 0; start < n; start += 1024)
    {
        const int m = std::min (1024, n - start);
        float* L = l + start;
        float* R = r + start;

        // Distortion
        auto& w0 = c.dryWet[0];
        if (w0.targetValue() > 0.0f || w0.value() > 1.0e-5f)
        {
            std::copy (L, L + m, tmpL);
            std::copy (R, R + m, tmpR);
            c.dist.process (tmpL, tmpR, m);
            for (int i = 0; i < m; ++i)
            {
                const float w = w0.next();
                L[i] += (tmpL[i] - L[i]) * w;
                R[i] += (tmpR[i] - R[i]) * w;
            }
        }

        // Compressor
        auto& w1 = c.dryWet[1];
        if (w1.targetValue() > 0.0f || w1.value() > 1.0e-5f)
            for (int i = 0; i < m; ++i)
            {
                float a = L[i], b = R[i];
                c.comp.process (a, b);
                const float w = w1.next();
                L[i] += (a - L[i]) * w;
                R[i] += (b - R[i]) * w;
            }

        // Pattern controlled filter
        auto& w2 = c.dryWet[2];
        if (w2.targetValue() > 0.0f || w2.value() > 1.0e-5f)
            for (int i = 0; i < m; ++i)
            {
                float a = L[i], b = R[i];
                c.pcf.process (a, b, t0 + (start + i) / sr);
                const float w = w2.next();
                L[i] += (a - L[i]) * w;
                R[i] += (b - R[i]) * w;
            }
    }
}

void Engine::process (juce::AudioBuffer<float>& out, int start, int numSamples, const TransportInfo& tr, const juce::MidiBuffer& midi)
{
    const int n = std::min (numSamples, maxBlock);
    const double t0 = (double) sampleCounter / sr;

    // Pattern selection: automatable parameter or MIDI note (ch 1-4, notes 36..67 = pattern 1..32)
    for (int d = 0; d < NumDevices; ++d)
    {
        const int p = pv.choice (iPattern[d], kPatterns);
        if (firstBlock || p != lastParamPattern[d])
        {
            lastParamPattern[d] = p;
            requestPattern (d, p);
        }
    }
    // Tempo
    const double bpm = tr.bpm > 0.0 ? tr.bpm : (double) juce::roundToInt (20.0 + pv[iTempo] * 280.0);
    tempo = (float) bpm;
    stepDur = 60.0 / bpm / 4.0;

    // A knob moved by the user or host automation takes over from song automation
    // until the song writes that parameter again.
    for (size_t i = 0; i < lastSeen.size(); ++i)
    {
        const float v = (*pv.params)[i]->getValue();
        if (v != lastSeen[i])
        {
            if (! firstBlock)
                overrides[i] = std::numeric_limits<float>::quiet_NaN();
            lastSeen[i] = v;
        }
    }

    // Single mode: one device, played by MIDI notes; the sequencer and the song stay silent
    const int mode = pv.choice (iPlayMode, NumDevices + 1) - 1;
    if (mode != soloDev)
    {
        stopVoices (t0);
        soloDev = mode;
        singleDevice = mode;
        resync = true;
    }

    // Song mode: hold the song for the whole block (try-lock, never blocks the audio thread)
    juce::SpinLock::ScopedTryLockType songGuard (songLock);
    activeSong = (songGuard.isLocked() && soloDev < 0 && pv.on (iSongMode) && song != nullptr && ! song->empty()) ? song.get() : nullptr;
    if (activeSong == nullptr)
    {
        if (songWasActive && overridesActive)
            clearOverrides();
        lastSongTick = -1000;
        uiSongTick = -1;
    }
    songWasActive = activeSong != nullptr;

    syncParams (firstBlock);
    if (firstBlock)
        for (int d = 0; d < NumDevices; ++d)
            currentPattern[d] = queuedPattern[d].load();
    firstBlock = false;

    // Drum previews from the editor start right at the block start.
    for (int k = 0; k < 2; ++k)
    {
        const int lane = auditionReq[k].exchange (-1);
        if (lane > 0)
            kits[k]->trigger (lane, t0, 1.0f, pv);
    }
    std::fill (std::begin (blockPeak), std::end (blockPeak), 0.0f);

    // The block is rendered in segments split at every sequencer step and MIDI event, so
    // song / MIDI automation takes effect on the exact sample (like the web app's AudioParams).
    int cursor = 0;
    auto renderTo = [&] (double at) {
        const int to = juce::jlimit (cursor, n, (int) std::ceil (at - 1.0e-9));
        if (to > cursor)
        {
            renderRange (out, start, cursor, to, t0);
            cursor = to;
        }
    };
    auto flushParams = [&] {
        // snapped after a jump / at playback start, so start values apply instantly
        // (a muted channel can't leak its first hit, a delay time doesn't glide)
        if (songParamsChanged || songChased)
            syncParams (songChased || snapUntilFirstStep);
        songParamsChanged = songChased = false;
    };
    if (! wasPlaying && (tr.hostPlaying || internalPlay))
        snapUntilFirstStep = true;
    auto midiIt = midi.cbegin();
    auto advanceMidiTo = [&] (double offset) {
        for (; midiIt != midi.cend() && (double) (*midiIt).samplePosition <= offset; ++midiIt)
        {
            renderTo ((double) (*midiIt).samplePosition);
            handleMidi ((*midiIt).getMessage(), (double) sampleCounter + (*midiIt).samplePosition);
            flushParams();
        }
    };

    // Sequencer
    // The host transport always wins over the internal PLAY button: host start takes over,
    // and a host stop (play -> stop) or a cursor jump while stopped (pressing STOP again
    // returns the cursor in most DAWs) ends an internal run.
    if (tr.hostPlaying)
        internalPlay = false;
    else if (internalPlay && (hostWasPlaying || (tr.hasPpq && hostPpqValid && std::abs (tr.ppq - lastHostPpq) > 1.0e-4)))
        internalPlay = false;
    hostWasPlaying = tr.hostPlaying;
    hostPpqValid = tr.hasPpq;
    lastHostPpq = tr.ppq;
    const bool playing = tr.hostPlaying || internalPlay;
    if (! playing)
    {
        if (wasPlaying)
            stopVoices (t0);
        for (int d = 0; d < NumDevices; ++d)
        {
            currentPattern[d] = queuedPattern[d].load();
            uiStep[d] = -1;
        }
        resync = true;
        expectedPos = -1.0;
        lastSongTick = -1000;
        lastStepSample = -1.0;
    }
    else
    {
        const bool host = tr.hostPlaying && tr.hasPpq;
        if (! host && ! wasPlaying)
            internalPos = 0.0;
        const double pos0 = host ? tr.ppq * 4.0 : internalPos;
        if (! wasPlaying || std::abs (pos0 - expectedPos) > 0.01)
        {
            resync = true;
            lastStep = (long long) std::ceil (pos0 - 1.0e-6) - 1;
        }
        const double stepsPerSample = bpm / 60.0 * 4.0 / sr;
        for (long long k = lastStep + 1;; ++k)
        {
            double off = ((double) k - pos0) / stepsPerSample;
            if (off >= n)
                break;
            off = std::max (0.0, off);
            advanceMidiTo (off + 1.0); // events landing on the step (within a sample) apply before it
            renderTo (off);
            onStep (k, t0 + off / sr, stepDur);
            lastStepSample = (double) sampleCounter + off;
            flushParams();
            snapUntilFirstStep = false;
            lastStep = k;
        }
        expectedPos = pos0 + n * stepsPerSample;
        if (! host)
            internalPos = expectedPos;
    }
    advanceMidiTo (1.0e12);
    renderTo ((double) n);
    wasPlaying = playing;
    isPlaying = playing;

    for (int d = 0; d <= NumDevices; ++d)
        meter[d] = blockPeak[d];
    for (int c = 2; c < out.getNumChannels(); ++c)
        out.clear (c, start, n);
    sampleCounter += n;
}

void Engine::renderRange (juce::AudioBuffer<float>& out, int start, int from, int to, double t0)
{
    const int m = to - from;
    const double ts = t0 + from / sr;

    // Voices -> channel buffers (mono upmixed to stereo)
    for (int i = from; i < to; ++i)
    {
        const double t = t0 + i / sr;
        const float v[NumDevices] = { bass[0].render (t), bass[1].render (t), kits[0]->render (t), kits[1]->render (t) };
        for (int d = 0; d < NumDevices; ++d)
            ch[d].bufL[(size_t) i] = ch[d].bufR[(size_t) i] = v[d] * kChannelInput[d];
    }

    std::fill (masterL.begin() + from, masterL.begin() + to, 0.0f);
    std::fill (masterR.begin() + from, masterR.begin() + to, 0.0f);
    std::fill (delayInL.begin() + from, delayInL.begin() + to, 0.0f);
    std::fill (delayInR.begin() + from, delayInR.begin() + to, 0.0f);

    for (int d = 0; d < NumDevices; ++d)
    {
        auto& c = ch[d];
        processChain (c.chain, c.bufL.data() + from, c.bufR.data() + from, m, ts);
        float peak = blockPeak[d];
        for (int i = from; i < to; ++i)
        {
            float l = c.bufL[(size_t) i], r = c.bufR[(size_t) i];
            stereoPan (c.pan.next(), l, r);
            const float g = c.fader.next() * c.mute.next();
            l *= g;
            r *= g;
            peak = std::max (peak, std::max (std::abs (l), std::abs (r)));
            masterL[(size_t) i] += l;
            masterR[(size_t) i] += r;
            const float s = c.send.next();
            delayInL[(size_t) i] += l * s;
            delayInR[(size_t) i] += r * s;
        }
        blockPeak[d] = peak;
    }

    for (int i = from; i < to; ++i)
    {
        float l, r;
        delay.process (delayInL[(size_t) i], delayInR[(size_t) i], l, r);
        const float g = delayReturn.next();
        masterL[(size_t) i] += l * g;
        masterR[(size_t) i] += r * g;
    }

    processChain (masterChain, masterL.data() + from, masterR.data() + from, m, ts);

    const int outCh = out.getNumChannels();
    float peak = blockPeak[NumDevices];
    for (int i = from; i < to; ++i)
    {
        const float g = masterFader.next() * masterVolume.next();
        float l = masterL[(size_t) i] * g, r = masterR[(size_t) i] * g;
        limiter.process (l, r);
        l = softClip (l);
        r = softClip (r);
        peak = std::max (peak, std::max (std::abs (l), std::abs (r)));
        if (outCh == 1)
            out.setSample (0, start + i, 0.5f * (l + r));
        else if (outCh >= 2)
        {
            out.setSample (0, start + i, l);
            out.setSample (1, start + i, r);
        }
    }
    blockPeak[NumDevices] = peak;
}

} // namespace rb
