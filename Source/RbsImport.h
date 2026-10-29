// Importer for ReBirth RB-338 2.0 song files (.rbs, "RB40" IFF container),
// ported from the Re-Rebirth web app's parser.
#pragma once

#include "Patterns.h"
#include "Song.h"
#include <map>

namespace rb
{
struct RbsData
{
    juce::String title, text, url, mod;
    double tempo = 120.0;
    bool songMode = false;
    std::map<juce::String, float> params; // normalised values by parameter id
    int selectedPattern[NumDevices] {};
    Song song;
};

// Parses `file`, writes all patterns into `bank` and returns the rest in `out`.
// Returns an empty string on success, otherwise an error message.
juce::String importRbs (const juce::File& file, PatternBank& bank, RbsData& out);

} // namespace rb
