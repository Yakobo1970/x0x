// Turns the song arrangement (or the current state) into a type-1 MIDI file:
// one track per x0x channel with knob CCs plus either pattern-change notes
// (x0x in RACK mode plays them) or the notes the sequencer plays (for x0x
// instances in single mode or any other synth).
#pragma once

#include "MidiMap.h"
#include "Song.h"
#include "Patterns.h"

namespace rb
{
struct MidiExportInput
{
    const Song* song = nullptr;                                  // may be null: exports the current state only
    const std::vector<juce::RangedAudioParameter*>* params = nullptr; // used when the song has no start values
    int patterns[NumDevices] {};                                 // patterns selected at the start
    double bpm = 120.0;
    juce::String title;
    PatternBank* bank = nullptr;                                 // needed for MidiExportKind::Notes
    bool notesWhileOff = false;                                  // also write notes of switched-off / muted devices
};

enum class MidiExportKind { Patterns, Notes };

constexpr int kMidiPpq = 96;                    // MIDI ticks per quarter note
constexpr int kMidiTicksPerSongTick = kMidiPpq / 8; // song ticks are 1/32 bar = 1/8 quarter

juce::MidiFile buildSongMidi (const MidiExportInput& in, MidiExportKind kind = MidiExportKind::Patterns);

// Writes the file to `target` (overwriting it). Returns false on error.
bool writeSongMidi (const MidiExportInput& in, const juce::File& target, MidiExportKind kind = MidiExportKind::Patterns);

} // namespace rb
