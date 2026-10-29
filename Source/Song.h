// Song-mode arrangement: per-track event lists (pattern changes and knob
// automation) at 32 ticks per bar, as stored in ReBirth .rbs files.
#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace rb
{
enum SongTrack { TrackMixer = 0, TrackBass1, TrackBass2, Track808, Track909, TrackFx, NumSongTracks };

struct SongEvent
{
    int tick = 0;
    int param = -1;    // parameter index, or -1 = pattern change for the track's device
    float value = 0.0f; // normalised parameter value, or pattern index for pattern changes
};

struct Song
{
    std::vector<SongEvent> tracks[NumSongTracks];
    int length = 0, loopStart = 0, loopEnd = 0; // ticks
    bool loop = false;
    juce::String title;
    std::vector<float> startValues; // all parameter values right after import (normalised, by index)

    bool empty() const
    {
        for (auto& t : tracks)
            if (! t.empty()) return false;
        return true;
    }

    std::unique_ptr<juce::XmlElement> toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement> ("Song");
        xml->setAttribute ("length", length);
        xml->setAttribute ("loopStart", loopStart);
        xml->setAttribute ("loopEnd", loopEnd);
        xml->setAttribute ("loop", loop ? 1 : 0);
        xml->setAttribute ("title", title);
        juce::String start;
        for (float v : startValues)
            start << juce::String (v, 5) << ' ';
        xml->setAttribute ("start", start.trimEnd());
        for (int t = 0; t < NumSongTracks; ++t)
        {
            juce::String data;
            for (auto& e : tracks[t])
                data << e.tick << ':' << e.param << ':' << juce::String (e.value, 6) << ' ';
            xml->createNewChildElement ("Track")->setAttribute ("events", data.trimEnd());
        }
        return xml;
    }

    void fromXml (const juce::XmlElement& xml)
    {
        *this = Song();
        length = xml.getIntAttribute ("length");
        loopStart = xml.getIntAttribute ("loopStart");
        loopEnd = xml.getIntAttribute ("loopEnd");
        loop = xml.getIntAttribute ("loop") != 0;
        title = xml.getStringAttribute ("title");
        for (auto& tok : juce::StringArray::fromTokens (xml.getStringAttribute ("start"), " ", ""))
            startValues.push_back (tok.getFloatValue());
        int t = 0;
        for (auto* tr : xml.getChildWithTagNameIterator ("Track"))
        {
            if (t >= NumSongTracks) break;
            for (auto& tok : juce::StringArray::fromTokens (tr->getStringAttribute ("events"), " ", ""))
            {
                auto parts = juce::StringArray::fromTokens (tok, ":", "");
                if (parts.size() == 3)
                    tracks[t].push_back ({ parts[0].getIntValue(), parts[1].getIntValue(), parts[2].getFloatValue() });
            }
            ++t;
        }
    }
};

} // namespace rb
