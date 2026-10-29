#include "MidiExport.h"

namespace rb
{
namespace
{
    const char* const kTrackNames[kMidiMixerChannel] = { "x0x BASS 1", "x0x BASS 2", "x0x DRUM 08", "x0x DRUM 09",
                                                         "x0x MIXER + FX" };
    constexpr double kStepTicks = kMidiPpq / 4.0;
    constexpr double kSwing = 0.42;     // same as the engine's swing amount
    constexpr double kGateTicks = kStepTicks * 0.5; // 303 gate length
    constexpr double kDrumTicks = kStepTicks * 0.5;
    constexpr double kSlideOverlap = 2.0; // ticks the old note overlaps the next one on a slide

    void addPatternNote (juce::MidiMessageSequence& seq, int channel, int pattern, double tick)
    {
        // One MIDI tick ahead of the step, so host rounding can never deliver it after the step.
        tick = std::max (0.0, tick - 1.0);
        const int note = kPatternNoteBase + juce::jlimit (0, kPatterns - 1, pattern);
        seq.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) 127), tick);
        seq.addEvent (juce::MidiMessage::noteOff (channel, note), tick + kMidiTicksPerSongTick);
    }

    std::vector<float> startValues (const MidiExportInput& in)
    {
        const size_t n = paramDefs().size();
        std::vector<float> v (n);
        for (size_t i = 0; i < n; ++i)
            v[i] = in.song != nullptr && i < in.song->startValues.size() ? in.song->startValues[i] : (*in.params)[i]->getValue();
        return v;
    }

    // Steps through the song like the engine's song mode and writes the notes each device plays.
    void addSequencedNotes (const MidiExportInput& in, juce::MidiMessageSequence* tracks, int numSteps)
    {
        auto& bank = *in.bank;
        auto vals = startValues (in);
        const int iShuffle = paramIndex ("song.shuffle"), iFlam = paramIndex ("r909.ac.flam");
        int iOn[NumDevices], iMute[NumDevices];
        for (int d = 0; d < NumDevices; ++d)
        {
            iOn[d] = paramIndex (juce::String (kDeviceIds[d]) + ".on");
            iMute[d] = paramIndex ("mixer." + juce::String (kDeviceIds[d]) + ".mute");
        }

        int pattern[NumDevices], pos[NumDevices] {}, cursor[NumSongTracks] {}, rxStep[NumDevices] {};
        for (int d = 0; d < NumDevices; ++d)
            pattern[d] = juce::jlimit (0, kPatterns - 1, in.patterns[d]);

        struct BassState { int open = -1; bool sliding = false, holding = false; } bs[2];

        for (int k = 0; k < numSteps; ++k)
        {
            const int tick = k * 2;
            if (in.song != nullptr)
                for (int t = 0; t < NumSongTracks; ++t)
                {
                    const auto& events = in.song->tracks[t];
                    for (int& i = cursor[t]; i < (int) events.size() && events[(size_t) i].tick <= tick; ++i)
                    {
                        const auto& e = events[(size_t) i];
                        if (e.param >= 0)
                            vals[(size_t) e.param] = e.value;
                        else if (t >= TrackBass1 && t <= Track909)
                        {
                            pattern[t - TrackBass1] = juce::jlimit (0, kPatterns - 1, (int) e.value);
                            pos[t - TrackBass1] = 0;
                        }
                    }
                }

            const double stepTick = k * kStepTicks;
            for (int d = 0; d < NumDevices; ++d)
            {
                auto& seq = tracks[d];
                const int ch = d + 1;
                const int len = std::max (1, bank.length (d, pattern[d]));
                const int o = pos[d] % len;
                pos[d] = (o + 1) % len;
                // step sync for the pattern filter whenever the position leaves the receiver's 16-step count
                if (o != rxStep[d])
                    seq.addEvent (juce::MidiMessage::controllerEvent (ch, kStepSyncCC, o), std::max (0.0, stepTick - 1.0));
                rxStep[d] = (o + 1) % kSteps;
                const bool audible = in.notesWhileOff || (vals[(size_t) iOn[d]] >= 0.5f && vals[(size_t) iMute[d]] < 0.5f);
                const double t = stepTick + ((bank.shuffle (d, pattern[d]) && (o % 2)) ? kStepTicks * kSwing * vals[(size_t) iShuffle] : 0.0);

                if (isBass (d))
                {
                    auto& st = bs[d];
                    auto closeAt = [&] (double at) {
                        if (st.open >= 0)
                            seq.addEvent (juce::MidiMessage::noteOff (ch, st.open), at);
                        st.open = -1;
                    };
                    auto& pat = bank.bassPattern (d, pattern[d]);
                    const auto cur = pat.step (o), next = pat.step ((o + 1) % len);
                    if (! audible || ! cur.gate)
                    {
                        closeAt (t);
                        st.sliding = st.holding = false;
                        continue;
                    }
                    if (cur.tie && st.holding && st.open >= 0)
                    {
                        st.holding = next.tie;
                        st.sliding = cur.slide && next.gate && ! next.tie;
                        if (! st.holding && ! st.sliding)
                            closeAt (t + kGateTicks);
                        continue;
                    }
                    const int note = juce::jlimit (0, 127, kBassNoteBase + cur.note + cur.octave * 12);
                    const auto vel = (juce::uint8) (cur.accent ? 127 : 90);
                    if (st.sliding && st.open >= 0)
                    {
                        // slide: the new note starts while the old one is still held
                        if (note != st.open)
                        {
                            seq.addEvent (juce::MidiMessage::noteOn (ch, note, vel), t);
                            seq.addEvent (juce::MidiMessage::noteOff (ch, st.open), t + kSlideOverlap);
                            st.open = note;
                        }
                    }
                    else
                    {
                        closeAt (t);
                        seq.addEvent (juce::MidiMessage::noteOn (ch, note, vel), t);
                        st.open = note;
                    }
                    st.sliding = cur.slide && next.gate && ! next.tie;
                    st.holding = next.tie;
                    if (! st.sliding && ! st.holding)
                        closeAt (t + kGateTicks);
                    continue;
                }

                if (! audible)
                    continue;
                auto& pat = bank.drumPattern (d, pattern[d]);
                const bool accent = pat.get (0, o) != Off;
                for (int lane = 1; lane < kDrumLanes; ++lane)
                {
                    const uint8_t u = pat.get (lane, o);
                    const int note = MidiMap::drumNoteFor (d, lane);
                    if (u == Off || note < 0)
                        continue;
                    double tt = t;
                    if (d == R909 && u == Flam)
                    {
                        const double flamTicks = (0.006 + vals[(size_t) iFlam] * 0.03) * in.bpm / 60.0 * kMidiPpq;
                        seq.addEvent (juce::MidiMessage::noteOn (ch, note, (juce::uint8) MidiMap::drumVelocity (accent, false, true)), tt);
                        seq.addEvent (juce::MidiMessage::noteOff (ch, note), tt + flamTicks * 0.5);
                        tt += flamTicks;
                    }
                    const int vel = MidiMap::drumVelocity (accent, d == R909 && u == Accent, false);
                    seq.addEvent (juce::MidiMessage::noteOn (ch, note, (juce::uint8) vel), tt);
                    seq.addEvent (juce::MidiMessage::noteOff (ch, note), tt + kDrumTicks);
                }
            }
        }
        for (int d = 0; d < 2; ++d)
            if (bs[d].open >= 0)
                tracks[d].addEvent (juce::MidiMessage::noteOff (d + 1, bs[d].open), numSteps * kStepTicks);
    }
}

juce::MidiFile buildSongMidi (const MidiExportInput& in, MidiExportKind kind)
{
    const auto& map = MidiMap::get();
    juce::MidiMessageSequence tracks[kMidiMixerChannel];
    const bool notes = kind == MidiExportKind::Notes && in.bank != nullptr;

    // Start state: every mapped knob at its start value, plus the start patterns.
    const auto start = startValues (in);
    for (size_t i = 0; i < map.ccFor.size(); ++i)
    {
        const auto [ch, cc] = map.ccFor[i];
        if (ch != 0)
            tracks[ch - 1].addEvent (juce::MidiMessage::controllerEvent (ch, cc, MidiMap::toCC (start[i])), 0.0);
    }
    if (! notes)
        for (int d = 0; d < NumDevices; ++d)
            addPatternNote (tracks[d], d + 1, in.patterns[d], 0.0);

    double endTick = 16.0 * kMidiPpq; // at least four bars
    if (in.song != nullptr)
    {
        for (int t = 0; t < NumSongTracks; ++t)
            for (const auto& e : in.song->tracks[t])
            {
                const double tick = (double) e.tick * kMidiTicksPerSongTick;
                if (e.param < 0)
                {
                    if (! notes && t >= TrackBass1 && t <= Track909)
                        addPatternNote (tracks[t - TrackBass1], t - TrackBass1 + 1, (int) e.value, tick);
                    continue;
                }
                const auto [ch, cc] = map.ccFor[(size_t) e.param];
                if (ch != 0)
                    tracks[ch - 1].addEvent (juce::MidiMessage::controllerEvent (ch, cc, MidiMap::toCC (e.value)), tick);
            }
        endTick = std::max (endTick, (double) in.song->length * kMidiTicksPerSongTick);
    }
    if (notes)
        addSequencedNotes (in, tracks, (int) std::ceil (endTick / kStepTicks));

    juce::MidiFile file;
    file.setTicksPerQuarterNote (kMidiPpq);

    juce::MidiMessageSequence conductor;
    conductor.addEvent (juce::MidiMessage::textMetaEvent (3, in.title.isNotEmpty() ? in.title : juce::String ("x0x")), 0.0);
    conductor.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / juce::jlimit (20.0, 300.0, in.bpm))), 0.0);
    conductor.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
    conductor.addEvent (juce::MidiMessage::endOfTrack(), endTick);
    file.addTrack (conductor);

    for (int c = 0; c < kMidiMixerChannel; ++c)
    {
        auto& seq = tracks[c];
        seq.addEvent (juce::MidiMessage::textMetaEvent (3, kTrackNames[c]), 0.0);
        seq.sort();
        seq.updateMatchedPairs();
        seq.addEvent (juce::MidiMessage::endOfTrack(), endTick);
        file.addTrack (seq);
    }
    return file;
}

bool writeSongMidi (const MidiExportInput& in, const juce::File& target, MidiExportKind kind)
{
    const auto file = buildSongMidi (in, kind);
    target.getParentDirectory().createDirectory();
    target.deleteFile();
    juce::FileOutputStream out (target);
    if (! out.openedOk())
        return false;
    const bool ok = file.writeTo (out, 1);
    out.flush();
    return ok && ! out.getStatus().failed();
}

} // namespace rb
