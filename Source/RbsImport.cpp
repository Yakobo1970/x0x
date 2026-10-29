#include "RbsImport.h"
#include "Params.h"

namespace rb
{
namespace
{
    // Knob lists in file order (empty entries are unused bytes).
    const char* const k808Knobs[] = { "ac.level", "bd.level", "bd.tone", "bd.decay", "sd.level", "sd.tone", "sd.snappy",
                                      "lt.level", "lt.tuning", "lt.alt", "mt.level", "mt.tuning", "mt.alt", "ht.level",
                                      "ht.tuning", "ht.alt", "rs.level", "rs.alt", "cp.level", "cp.alt", "cb.level",
                                      "cy.level", "cy.tone", "cy.decay", "oh.level", "oh.decay", "ch.level" };
    const char* const k909Knobs[] = { "ac.level", "bd.level", "bd.tune", "bd.attack", "bd.decay", "sd.level", "sd.tune",
                                      "sd.tone", "sd.snappy", "lt.level", "lt.tune", "lt.decay", "mt.level", "mt.tune",
                                      "mt.decay", "ht.level", "ht.tune", "ht.decay", "rs.level", "cp.level", "hh.level",
                                      "hh.chDecay", "hh.ohDecay", "cy.crLevel", "cy.crTune", "cy.rdLevel", "cy.rdTune",
                                      "ac.flam" };
    const char* const kBassKnobs[] = { "tuning", "cutoff", "resonance", "envmod", "decay", "accent" };
    constexpr int kTicksPerBarFile = 768;

    float T (int v) { return std::min (1.0f, (float) v / 127.0f); }
    float rn (int v, int n) { return (float) juce::jlimit (0, n - 1, v) / (float) (n - 1); }
    float knobValue (const juce::String& key, int v) { return key.endsWith (".alt") ? (v ? 0.0f : 1.0f) : T (v); }
    int patternLength (int v) { return juce::jlimit (1, 16, v ? v : 16); }

    struct Chunk
    {
        juce::String id, type;
        size_t at = 0, size = 0;
        std::vector<Chunk> children;
    };

    struct Reader
    {
        const juce::MemoryBlock& mb;
        const uint8_t* b() const { return static_cast<const uint8_t*> (mb.getData()); }
        size_t len() const { return mb.getSize(); }
        int u8 (size_t p) const { return p < len() ? b()[p] : 0; }
        uint32_t u32 (size_t p) const
        {
            return ((uint32_t) u8 (p) << 24) | ((uint32_t) u8 (p + 1) << 16) | ((uint32_t) u8 (p + 2) << 8) | (uint32_t) u8 (p + 3);
        }
        juce::String str (size_t p, size_t n) const
        {
            juce::String s;
            for (size_t i = 0; i < n; ++i) s << (juce::juce_wchar) u8 (p + i);
            return s;
        }
        juce::String zstr (size_t p, size_t n) const
        {
            size_t e = p;
            while (e < p + n && e < len() && b()[e] != 0) ++e;
            // windows-1252 is close enough to Latin-1 for song titles
            juce::String s;
            for (size_t i = p; i < e; ++i) s << (juce::juce_wchar) b()[i];
            return s;
        }
        std::vector<Chunk> chunks (size_t from, size_t to) const
        {
            std::vector<Chunk> out;
            size_t r = from;
            while (r + 8 <= to)
            {
                Chunk c;
                c.id = str (r, 4);
                const size_t size = u32 (r + 4);
                const size_t a = r + 8;
                if (a + size > to)
                    throw std::runtime_error ("Truncated \"" + c.id.toStdString() + "\" chunk");
                if (c.id == "CAT ")
                {
                    c.type = str (a, 4);
                    c.children = chunks (a + 4, a + size);
                }
                c.at = a;
                c.size = size;
                out.push_back (std::move (c));
                r = a + size + (size & 1);
            }
            return out;
        }
    };

    const Chunk* find (const std::vector<Chunk>& list, const char* id, const char* type = nullptr)
    {
        for (auto& c : list)
            if (c.id == id && (type == nullptr || c.type == type))
                return &c;
        return nullptr;
    }

    struct RawEvent { int tick, id, value; };

    std::vector<RawEvent> readTrack (const Reader& r, const Chunk& c)
    {
        std::vector<RawEvent> out;
        const size_t end = c.at + c.size;
        const uint32_t count = r.u32 (c.at);
        size_t a = c.at + 4;
        int tick = 0;
        for (uint32_t k = 0; k < count && a < end; ++k)
        {
            int delta = 0, v;
            do
            {
                v = r.u8 (a++);
                delta = (delta << 7) | (v & 127);
            } while ((v & 128) && a < end);
            tick += delta;
            out.push_back ({ tick, r.u8 (a), r.u8 (a + 1) });
            a += 2;
        }
        return out;
    }

    void addEvent (std::vector<SongEvent>& track, int tick, const juce::String& key, float value)
    {
        if (key == "pattern")
        {
            track.push_back ({ tick, -1, value });
            return;
        }
        const auto& defs = paramDefs();
        for (int i = 0; i < (int) defs.size(); ++i)
            if (defs[(size_t) i].id == key)
            {
                track.push_back ({ tick, i, value });
                return;
            }
    }
}

juce::String importRbs (const juce::File& file, PatternBank& bank, RbsData& out)
{
    juce::MemoryBlock mb;
    if (! file.loadFileAsData (mb))
        return juce::String (juce::CharPointer_UTF8 ("Datei konnte nicht gelesen werden"));
    Reader r { mb };
    if (mb.getSize() <= 12 || r.str (0, 4) != "CAT " || r.str (8, 4) != "RB40")
        return juce::String (juce::CharPointer_UTF8 ("Keine ReBirth-2.0-Songdatei (RB40 erwartet). ReBirth-1.x-Songs bitte in ReBirth 2.0 öffnen und neu speichern."));

    try
    {
        const auto top = r.chunks (12, std::min ((size_t) mb.getSize(), (size_t) 8 + r.u32 (4)));
        const auto* glob = find (top, "GLOB");
        const auto* devl = find (top, "CAT ", "DEVL");
        const auto* trkl = find (top, "CAT ", "TRKL");
        if (glob == nullptr || devl == nullptr)
            return juce::String (juce::CharPointer_UTF8 ("Song enthält keinen GLOB- oder DEVL-Abschnitt"));
        // Mods only swap graphics and drum samples; the song data layout is the same,
        // so they are imported with the standard sounds.
        out.mod = r.zstr (glob->at + 15, 65);

        auto& p = out.params;
        const size_t u = glob->at;
        out.songMode = r.u8 (u) != 0;
        out.song.loop = r.u8 (u + 1) != 0;
        out.tempo = r.u32 (u + 2) / 1000.0;
        auto toTicks = [] (uint32_t v) { return (int) std::lround ((double) v / kTicksPerBarFile * 32.0); };
        out.song.loopStart = toTicks (r.u32 (u + 6));
        out.song.loopEnd = toTicks (r.u32 (u + 10));
        p["song.shuffle"] = T (r.u8 (u + 14));

        if (auto* usri = find (top, "USRI"))
        {
            out.title = r.zstr (usri->at, 41).trim();
            out.text = r.zstr (usri->at + 41, 401);
            out.url = r.zstr (usri->at + 442, 101);
        }
        out.song.title = out.title;
        const auto* head = find (top, "HEAD");
        const int version = head != nullptr ? r.u8 (head->at + 6) : 0;
        const auto& devs = devl->children;

        // Mixer
        if (auto* mixr = find (devs, "MIXR"))
        {
            const size_t o = mixr->at;
            p["mixer.master.level"] = T (r.u8 (o));
            int comp = version >= 1 ? r.u8 (o + 1) : 0;
            int pcf = r.u8 (o + 2);
            for (int d = 0; d < NumDevices; ++d)
            {
                const size_t i = o + 16 + (size_t) d * 12;
                const juce::String m = "mixer." + juce::String (kDeviceIds[d]) + ".";
                p[m + "mute"] = r.u8 (i) ? 0.0f : 1.0f;
                p[m + "level"] = T (r.u8 (i + 1));
                p[m + "pan"] = T (r.u8 (i + 2));
                p[m + "delay"] = T (r.u8 (i + 3));
                p[m + "dist"] = r.u8 (i + 4) ? 1.0f : 0.0f;
                if (r.u8 (i + 5)) pcf = d + 1;
                if (r.u8 (i + 6)) comp = d + 2;
            }
            p["fx.comp.routed"] = (comp >= 1 && comp <= 5) ? 1.0f : 0.0f;
            p["fx.comp.target"] = rn (juce::jlimit (0, 4, comp - 1), 5);
            p["fx.pcf.target"] = rn (pcf < 5 ? pcf : 0, 5);
        }

        // Effects
        if (auto* c = find (devs, "DELY"))
        {
            const size_t t = c->at;
            p["fx.delay.on"] = r.u8 (t) ? 1.0f : 0.0f;
            p["fx.delay.steps"] = rn (r.u8 (t + 1) - 1, 32);
            p["fx.delay.triplet"] = r.u8 (t + 2) ? 0.0f : 1.0f;
            p["fx.delay.feedback"] = T (r.u8 (t + 3));
            p["fx.delay.pan"] = T (r.u8 (t + 4));
        }
        if (auto* c = find (devs, "PCF "))
        {
            const size_t t = c->at;
            p["fx.pcf.on"] = r.u8 (t) ? 1.0f : 0.0f;
            p["fx.pcf.freq"] = T (r.u8 (t + 1));
            p["fx.pcf.reso"] = T (r.u8 (t + 2));
            p["fx.pcf.amount"] = T (r.u8 (t + 3));
            p["fx.pcf.wave"] = rn (r.u8 (t + 4), 56);
            p["fx.pcf.decay"] = T (r.u8 (t + 5));
            p["fx.pcf.mode"] = r.u8 (t + 6) ? 1.0f : 0.0f;
        }
        if (auto* c = find (devs, "DIST"))
        {
            p["fx.dist.on"] = r.u8 (c->at) ? 1.0f : 0.0f;
            p["fx.dist.amount"] = T (r.u8 (c->at + 1));
            p["fx.dist.shape"] = T (r.u8 (c->at + 2));
        }
        if (auto* c = find (devs, "COMP"))
        {
            p["fx.comp.on"] = r.u8 (c->at) ? 1.0f : 0.0f;
            p["fx.comp.amount"] = T (r.u8 (c->at + 1));
            p["fx.comp.threshold"] = T (r.u8 (c->at + 2));
        }

        // Bass lines
        int bassIdx = 0;
        for (auto& c : devs)
        {
            if (c.id != "303 " || bassIdx >= 2)
                continue;
            const int dev = bassIdx++;
            const juce::String id = kDeviceIds[dev];
            const size_t i = c.at;
            p[id + ".on"] = r.u8 (i) ? 1.0f : 0.0f;
            for (int k = 0; k < 6; ++k)
                p[id + "." + kBassKnobs[k]] = T (r.u8 (i + 2 + (size_t) k));
            p[id + ".waveform"] = r.u8 (i + 8) ? 1.0f : 0.0f;
            for (int n = 0; n < kPatterns; ++n)
            {
                const size_t base = i + 9 + (size_t) n * 34;
                auto& pat = bank.bassPattern (dev, n);
                pat.shuffle = r.u8 (base) != 0;
                const int len = patternLength (r.u8 (base + 1));
                pat.length = len;
                int flags[kSteps];
                bool gate[kSteps];
                for (int s = 0; s < kSteps; ++s)
                {
                    flags[s] = r.u8 (base + 3 + (size_t) s * 2);
                    gate[s] = ! (flags[s] & 16);
                }
                // A "pause" step becomes a tie when the previous (sounding) step slides into it.
                for (int e = 0; e < 2 * len; ++e)
                {
                    const int t = e % len, prev = (t + len - 1) % len;
                    if (flags[t] & 16)
                        gate[t] = (flags[prev] & 1) && gate[prev];
                }
                for (int s = 0; s < kSteps; ++s)
                {
                    BassStep st;
                    st.note = std::min (12, r.u8 (base + 2 + (size_t) s * 2));
                    st.octave = (flags[s] & 4) ? 1 : (flags[s] & 8) ? -1 : 0;
                    st.accent = (flags[s] & 2) != 0;
                    st.slide = (flags[s] & 1) != 0;
                    st.gate = gate[s];
                    st.tie = (flags[s] & 16) && gate[s];
                    pat.setStep (s, st);
                }
            }
            out.selectedPattern[dev] = std::min (31, r.u8 (i + 1));
        }

        // Drum machines
        auto readDrums = [&] (const Chunk& c, int dev, const char* const* knobs, int numKnobs) {
            const juce::String id = kDeviceIds[dev];
            const size_t o = c.at;
            p[id + ".on"] = r.u8 (o) ? 1.0f : 0.0f;
            for (int k = 0; k < numKnobs; ++k)
                p[id + "." + knobs[k]] = knobValue (knobs[k], r.u8 (o + 2 + (size_t) k));
            const size_t first = 2 + (size_t) numKnobs + 1;
            const size_t stride = 2 + 16 * (size_t) kDrumLanes;
            for (int n = 0; n < kPatterns; ++n)
            {
                const size_t base = o + first + (size_t) n * stride;
                auto& pat = bank.drumPattern (dev, n);
                pat.shuffle = r.u8 (base) != 0;
                pat.length = patternLength (r.u8 (base + 1));
                for (int lane = 0; lane < kDrumLanes; ++lane)
                    for (int s = 0; s < kSteps; ++s)
                        pat.set (lane, s, (uint8_t) std::min (3, r.u8 (base + 2 + (size_t) s * kDrumLanes + (size_t) lane)));
            }
            out.selectedPattern[dev] = std::min (31, r.u8 (o + 1));
        };
        if (auto* c = find (devs, "808 ")) readDrums (*c, R808, k808Knobs, (int) std::size (k808Knobs));
        if (auto* c = find (devs, "909 ")) readDrums (*c, R909, k909Knobs, (int) std::size (k909Knobs));

        // Song arrangement
        if (trkl != nullptr)
        {
            std::vector<std::vector<RawEvent>> raw;
            for (auto& c : trkl->children)
                if (c.id == "TRAK")
                    raw.push_back (readTrack (r, c));
            auto track = [&] (size_t i) { return i < raw.size() ? raw[i] : std::vector<RawEvent> {}; };

            auto deviceTrack = [&] (size_t idx, int dev, SongTrack target) {
                const juce::String id = kDeviceIds[dev];
                for (auto& e : track (idx))
                {
                    if (e.id == 0) addEvent (out.song.tracks[target], e.tick, id + ".on", e.value ? 1.0f : 0.0f);
                    else if (e.id == 1) addEvent (out.song.tracks[target], e.tick, "pattern", (float) std::min (31, e.value));
                    else if (isBass (dev))
                    {
                        if (e.id == 8) addEvent (out.song.tracks[target], e.tick, id + ".waveform", e.value ? 1.0f : 0.0f);
                        else if (e.id - 2 < 6) addEvent (out.song.tracks[target], e.tick, id + "." + kBassKnobs[e.id - 2], T (e.value));
                    }
                    else
                    {
                        const auto* list = dev == R808 ? k808Knobs : k909Knobs;
                        const int n = dev == R808 ? (int) std::size (k808Knobs) : (int) std::size (k909Knobs);
                        if (e.id - 2 < n)
                            addEvent (out.song.tracks[target], e.tick, id + "." + list[e.id - 2], knobValue (list[e.id - 2], e.value));
                    }
                }
            };
            deviceTrack (1, Bass1, TrackBass1);
            deviceTrack (2, Bass2, TrackBass2);
            deviceTrack (3, R808, Track808);
            deviceTrack (4, R909, Track909);

            // Effect tracks: delay, dist, pcf, comp
            static const std::vector<std::pair<const char*, std::vector<const char*>>> fx = {
                { "fx.delay", { "on", "steps", "triplet", "feedback", "pan" } },
                { "fx.dist", { "on", "amount", "shape" } },
                { "fx.pcf", { "on", "freq", "reso", "amount", "wave", "decay", "mode" } },
                { "fx.comp", { "on", "amount", "threshold" } } };
            for (size_t f = 0; f < fx.size(); ++f)
                for (auto& e : track (5 + f))
                {
                    if (e.id < 0 || e.id >= (int) fx[f].second.size())
                        continue;
                    const juce::String key = juce::String (fx[f].first) + "." + fx[f].second[(size_t) e.id];
                    float v;
                    if (key == "fx.delay.triplet") v = e.value ? 0.0f : 1.0f;
                    else if (key.endsWith (".on") || key == "fx.pcf.mode") v = e.value ? 1.0f : 0.0f;
                    else if (key == "fx.delay.steps") v = rn (e.value - 1, 32);
                    else if (key == "fx.pcf.wave") v = rn (e.value, 56);
                    else v = T (e.value);
                    addEvent (out.song.tracks[TrackFx], e.tick, key, v);
                }
            std::stable_sort (out.song.tracks[TrackFx].begin(), out.song.tracks[TrackFx].end(),
                              [] (const SongEvent& a, const SongEvent& b) { return a.tick < b.tick; });

            // Mixer track (two layouts depending on the file version)
            auto& mt = out.song.tracks[TrackMixer];
            const auto mixerRaw = track (0);
            if (version >= 1)
            {
                for (auto& e : mixerRaw)
                {
                    if (e.id == 1 && e.value <= 5)
                    {
                        addEvent (mt, e.tick, "fx.comp.routed", e.value ? 1.0f : 0.0f);
                        if (e.value)
                            addEvent (mt, e.tick, "fx.comp.target", rn (std::min (e.value - 1, 4), 5));
                    }
                    if (e.id == 2 && e.value < 5)
                        addEvent (mt, e.tick, "fx.pcf.target", rn (e.value, 5));
                    if (e.id < 6 || e.id >= 30)
                        continue;
                    const juce::String m = "mixer." + juce::String (kDeviceIds[(e.id - 6) / 6]) + ".";
                    switch ((e.id - 6) % 6)
                    {
                        case 0: addEvent (mt, e.tick, m + "level", T (e.value)); break;
                        case 1: addEvent (mt, e.tick, m + "pan", T (e.value)); break;
                        case 2: addEvent (mt, e.tick, m + "delay", T (e.value)); break;
                        case 3: addEvent (mt, e.tick, m + "dist", e.value ? 1.0f : 0.0f); break;
                        default: break;
                    }
                }
            }
            else
            {
                int pcfOn[4] {}, compOn[4] {};
                float lastPcf = -1.0f;
                int lastComp = -1;
                for (size_t k = 0; k < mixerRaw.size(); ++k)
                {
                    const auto& e = mixerRaw[k];
                    if (e.id >= 5 && e.id < 37)
                    {
                        const int d = (e.id - 5) / 8, f = (e.id - 5) % 8;
                        const juce::String m = "mixer." + juce::String (kDeviceIds[d]) + ".";
                        if (f == 0) addEvent (mt, e.tick, m + "level", T (e.value));
                        if (f == 1) addEvent (mt, e.tick, m + "pan", T (e.value));
                        if (f == 2) addEvent (mt, e.tick, m + "delay", T (e.value));
                        if (f == 3) addEvent (mt, e.tick, m + "dist", e.value ? 1.0f : 0.0f);
                        if (f == 4) pcfOn[d] = e.value ? 1 : 0;
                        if (f == 5) compOn[d] = e.value ? 1 : 0;
                    }
                    if (k + 1 < mixerRaw.size() && mixerRaw[k + 1].tick == e.tick)
                        continue;
                    int pi = -1, ci = -1;
                    for (int d = 0; d < 4; ++d)
                    {
                        if (pi < 0 && pcfOn[d]) pi = d;
                        if (ci < 0 && compOn[d]) ci = d;
                    }
                    const float pcfVal = rn (pi + 1, 5);
                    const int comp = ci + 1;
                    if (pcfVal != lastPcf) addEvent (mt, e.tick, "fx.pcf.target", pcfVal);
                    if (comp != lastComp)
                    {
                        addEvent (mt, e.tick, "fx.comp.routed", comp ? 1.0f : 0.0f);
                        if (comp) addEvent (mt, e.tick, "fx.comp.target", rn (comp, 5));
                    }
                    lastPcf = pcfVal;
                    lastComp = comp;
                }
            }
        }

        int maxTick = 0;
        for (auto& t : out.song.tracks)
            for (auto& e : t)
                maxTick = std::max (maxTick, e.tick);
        out.song.length = std::max (out.song.loopEnd, (int) std::ceil ((maxTick + 1) / 32.0) * 32);
        if (out.song.loopEnd == 0)
            out.song.loopEnd = out.song.length;
    }
    catch (const std::exception& e)
    {
        return juce::String (e.what());
    }
    return {};
}

} // namespace rb
