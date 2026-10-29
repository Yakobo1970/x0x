// MIDI layout used for DAW control and song export:
//   channel 1 = BASS 1, 2 = BASS 2, 3 = DRUM 08, 4 = DRUM 09, 5 = mixer + effects.
//   Notes 36..67 on channels 1-4 select pattern A1..D8 (velocity >= 100 switches
//   immediately and restarts the pattern, lower velocities queue the change).
//   Every automatable knob/switch has a fixed CC number on its channel.
// Note export / single mode (one device played by notes, any channel):
//   303: MIDI note 36 = pattern note C, octave 0; overlapping notes slide; velocity >= 100 = accent.
//   Drums: General MIDI drum notes (BD 36, RS 37, SD 38, CP 39, LT 41, CH 42, MT 45, OH 46,
//   HT 48, CY/CR 49, RD 51, CB 56). Velocity: < 64 flam grace hit (half level), 64-99 normal,
//   100-119 909 voice accent (808: accent), 120-126 accent, 127 accent + voice accent.
#pragma once

#include "Params.h"
#include "Patterns.h"

namespace rb
{
constexpr int kMidiMixerChannel = 5;
constexpr int kPatternNoteBase = 36;
constexpr int kImmediateVelocity = 100;
constexpr int kBassNoteBase = 36; // 303 note C, octave 0
constexpr int kStepSyncCC = 119;  // single mode: pattern step (0-15) the next step plays (drives the pattern filter)

struct MidiMap
{
    int paramFor[kMidiMixerChannel + 1][128]; // [channel][cc] -> parameter index or -1
    std::vector<std::pair<int, int>> ccFor;   // parameter index -> (channel, cc), channel 0 = not mapped

    static const MidiMap& get()
    {
        static const MidiMap m;
        return m;
    }

    // Channel a parameter belongs to (0 = not controllable via MIDI).
    static int channelFor (const juce::String& id)
    {
        if (id.endsWith (".pattern") || id == "tempo" || id == "song.mode" || id == "play.mode")
            return 0;
        for (int d = 0; d < NumDevices; ++d)
            if (id.startsWith (juce::String (kDeviceIds[d]) + "."))
                return d + 1;
        return kMidiMixerChannel; // mixer.*, fx.*, master.volume, song.shuffle
    }

    static int drumNoteFor (int device, int lane)
    {
        static const std::pair<const char*, int> notes[] = { { "bd", 36 }, { "rs", 37 }, { "sd", 38 }, { "cp", 39 },
                                                             { "lt", 41 }, { "ch", 42 }, { "mt", 45 }, { "oh", 46 },
                                                             { "ht", 48 }, { "cy", 49 }, { "cr", 49 }, { "rd", 51 },
                                                             { "cb", 56 } };
        const juce::String id (drumLanes (device)[lane]);
        for (auto& [lid, note] : notes)
            if (id == lid)
                return note;
        return -1;
    }

    // Lane for an incoming drum note (also accepts the neighbouring GM alternatives), 0 = none.
    static int drumLaneFor (int device, int note)
    {
        static const std::pair<int, int> alias[] = { { 35, 36 }, { 40, 38 }, { 43, 41 }, { 47, 45 }, { 50, 48 },
                                                     { 44, 42 }, { 57, 49 }, { 52, 49 }, { 55, 49 }, { 59, 51 } };
        for (auto [from, to] : alias)
            if (note == from)
                note = to;
        for (int lane = 1; lane < kDrumLanes; ++lane)
            if (drumNoteFor (device, lane) == note)
                return lane;
        return 0;
    }

    static int drumVelocity (bool accent, bool voiceAccent, bool grace)
    {
        return grace ? 45 : accent ? (voiceAccent ? 127 : 120) : voiceAccent ? 110 : 90;
    }

    static float toNormalised (int ccValue) { return (float) ccValue / 127.0f; }
    static int toCC (float normalised) { return juce::jlimit (0, 127, juce::roundToInt (normalised * 127.0f)); }

private:
    MidiMap()
    {
        for (auto& ch : paramFor)
            for (auto& p : ch)
                p = -1;

        // CC numbers that avoid bank select, mod wheel, data entry, sustain, (N)RPN and channel mode messages.
        std::vector<int> allowed;
        for (int cc = 3; cc <= 119; ++cc)
        {
            const bool reserved = cc == 6 || cc == 7 || cc == 10 || cc == 11 || cc == 32 || cc == 38
                               || (cc >= 64 && cc <= 69) || (cc >= 96 && cc <= 101) || cc == kStepSyncCC;
            if (! reserved)
                allowed.push_back (cc);
        }

        const auto& defs = paramDefs();
        ccFor.assign (defs.size(), { 0, 0 });
        int next[kMidiMixerChannel + 1] {};
        for (int i = 0; i < (int) defs.size(); ++i)
        {
            const int ch = channelFor (defs[(size_t) i].id);
            if (ch == 0 || next[ch] >= (int) allowed.size())
                continue;
            const int cc = allowed[(size_t) next[ch]++];
            paramFor[ch][cc] = i;
            ccFor[(size_t) i] = { ch, cc };
        }
    }
};

} // namespace rb
