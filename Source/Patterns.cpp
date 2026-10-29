#include "Patterns.h"

namespace rb
{
const char* const kDeviceIds[NumDevices] = { "bass1", "bass2", "r808", "r909" };
const char* const kDeviceLabels[NumDevices] = { "BASS 1", "BASS 2", "DRUM 08", "DRUM 09" };

const char* const k808Lanes[kDrumLanes] = { "ac", "bd", "sd", "lt", "mt", "ht", "rs", "cp", "cb", "cy", "oh", "ch" };
const char* const k909Lanes[kDrumLanes] = { "ac", "bd", "sd", "lt", "mt", "ht", "rs", "cp", "ch", "oh", "cr", "rd" };
const char* const k808LaneNames[kDrumLanes] = { "ACCENT", "BASS DRUM", "SNARE DRUM", "LOW TOM", "MID TOM", "HI TOM",
                                                "RIM SHOT", "HAND CLAP", "COW BELL", "CYMBAL", "OPEN HIHAT", "CLOSED HIHAT" };
const char* const k909LaneNames[kDrumLanes] = { "ACCENT", "BASS DRUM", "SNARE DRUM", "LOW TOM", "MID TOM", "HI TOM",
                                                "RIM SHOT", "HAND CLAP", "CLOSED HI-HAT", "OPEN HI-HAT", "CRASH CYMBAL", "RIDE CYMBAL" };

void BassPattern::clear()
{
    length = 16;
    shuffle = false;
    for (auto& s : steps)
        s.store (BassStep().pack());
}

void DrumPattern::clear()
{
    length = 16;
    shuffle = false;
    for (auto& lane : cells)
        for (auto& c : lane)
            c.store (Off);
}

void PatternBank::clearAll()
{
    for (auto& dev : bass)
        for (auto& p : dev)
            p.clear();
    for (auto& dev : drums)
        for (auto& p : dev)
            p.clear();
}

namespace
{
    // Same token syntax as the web app's demo song: "C#^+as" = note, upper C, octave, accent, slide.
    void parseBass (BassPattern& p, const char* text)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        auto tokens = juce::StringArray::fromTokens (text, " ", "");
        p.clear();
        for (int i = 0; i < juce::jmin (kSteps, tokens.size()); ++i)
        {
            auto tok = tokens[i];
            BassStep s;
            if (tok != ".")
            {
                int len = (tok.length() > 1 && tok[1] == '#') ? 2 : 1;
                auto name = tok.substring (0, len);
                for (int n = 0; n < 12; ++n)
                    if (name == names[n])
                        s.note = n;
                auto rest = tok.substring (len);
                if (rest.startsWithChar ('^')) { s.note += 12; rest = rest.substring (1); }
                if (rest.startsWithChar ('+')) { s.octave = 1; rest = rest.substring (1); }
                else if (rest.startsWithChar ('-')) { s.octave = -1; rest = rest.substring (1); }
                s.accent = rest.containsChar ('a');
                s.slide = rest.containsChar ('s');
                s.gate = true;
            }
            p.setStep (i, s);
        }
    }

    void parseDrum (DrumPattern& p, int device, std::initializer_list<std::pair<const char*, const char*>> lanes)
    {
        p.clear();
        auto ids = drumLanes (device);
        for (auto& [id, cells] : lanes)
            for (int lane = 0; lane < kDrumLanes; ++lane)
                if (juce::String (ids[lane]) == id)
                    for (int s = 0; s < kSteps && cells[s] != 0; ++s)
                        p.set (lane, s, cells[s] == 'X' ? Accent : cells[s] == 'x' ? On : Off);
    }
}

void PatternBank::loadDemo()
{
    clearAll();
    parseBass (bassPattern (Bass1, 0), "C . C+a D# C G-s G- A#a C . C+s C F#a . G A#s");
    parseBass (bassPattern (Bass2, 0), "C-a . . C- . . D#-a . C- . G-s A#- . C-a . .");
    parseDrum (drumPattern (R808, 0), R808,
               { { "bd", "x.....x...x..x.." }, { "sd", "....x.......x..." }, { "ch", "x.x.x.x.x.xxx.x." },
                 { "oh", "..............x." }, { "cb", "...x.......x...." }, { "cp", "............x..." },
                 { "ac", "....x.......x..." } });
    parseDrum (drumPattern (R909, 0), R909,
               { { "bd", "x...x...x...x..." }, { "cp", "....x.......x..." }, { "sd", ".......x.x....X." },
                 { "ch", "x...x...x...x..." }, { "oh", "..x...x...x...x." }, { "rd", "x.x.x.x.x.x.x.x." },
                 { "ac", "x...x...x...x..." } });
}

std::unique_ptr<juce::XmlElement> PatternBank::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement> ("Patterns");
    for (int d = 0; d < 2; ++d)
        for (int i = 0; i < kPatterns; ++i)
        {
            auto& p = bass[(size_t) d][(size_t) i];
            juce::String steps;
            for (auto& s : p.steps)
                steps << juce::String::toHexString ((int) s.load()).paddedLeft ('0', 4);
            auto* e = xml->createNewChildElement ("Bass");
            e->setAttribute ("dev", d);
            e->setAttribute ("idx", i);
            e->setAttribute ("len", p.length.load());
            e->setAttribute ("shuf", p.shuffle.load() ? 1 : 0);
            e->setAttribute ("steps", steps);
        }
    for (int d = 0; d < 2; ++d)
        for (int i = 0; i < kPatterns; ++i)
        {
            auto& p = drums[(size_t) d][(size_t) i];
            juce::String cells;
            for (auto& lane : p.cells)
                for (auto& c : lane)
                    cells << juce::String ((int) c.load());
            auto* e = xml->createNewChildElement ("Drum");
            e->setAttribute ("dev", d);
            e->setAttribute ("idx", i);
            e->setAttribute ("len", p.length.load());
            e->setAttribute ("shuf", p.shuffle.load() ? 1 : 0);
            e->setAttribute ("cells", cells);
        }
    return xml;
}

void PatternBank::fromXml (const juce::XmlElement& xml)
{
    clearAll();
    for (auto* e : xml.getChildIterator())
    {
        const int d = e->getIntAttribute ("dev"), i = e->getIntAttribute ("idx");
        if (d < 0 || d > 1 || i < 0 || i >= kPatterns)
            continue;
        if (e->hasTagName ("Bass"))
        {
            auto& p = bass[(size_t) d][(size_t) i];
            p.length = juce::jlimit (1, kSteps, e->getIntAttribute ("len", 16));
            p.shuffle = e->getIntAttribute ("shuf") != 0;
            auto steps = e->getStringAttribute ("steps");
            for (int s = 0; s < kSteps && (s + 1) * 4 <= steps.length(); ++s)
                p.steps[(size_t) s].store ((uint16_t) steps.substring (s * 4, s * 4 + 4).getHexValue32());
        }
        else if (e->hasTagName ("Drum"))
        {
            auto& p = drums[(size_t) d][(size_t) i];
            p.length = juce::jlimit (1, kSteps, e->getIntAttribute ("len", 16));
            p.shuffle = e->getIntAttribute ("shuf") != 0;
            auto cells = e->getStringAttribute ("cells");
            for (int k = 0; k < kDrumLanes * kSteps && k < cells.length(); ++k)
                p.cells[(size_t) (k / kSteps)][(size_t) (k % kSteps)].store ((uint8_t) juce::jlimit (0, 3, (int) (cells[k] - '0')));
        }
    }
}

} // namespace rb
