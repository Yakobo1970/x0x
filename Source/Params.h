// Parameter table. Values are kept normalised (0..1) exactly like the web
// app's state store, so the engine maths can be ported 1:1.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <unordered_map>
#include <vector>

namespace rb
{
struct ParamDef
{
    juce::String id, name;
    float def = 0.0f;       // normalised default
    int kind = 0;           // 0 = continuous, 1 = bool, 2 = choice, 3 = int
    juce::StringArray choices;
    int intMin = 0, intMax = 0;
};

inline const std::vector<ParamDef>& paramDefs()
{
    static const std::vector<ParamDef> defs = [] {
        std::vector<ParamDef> d;
        auto knob = [&] (juce::String id, juce::String name, float def) { d.push_back ({ id, name, def, 0, {}, 0, 0 }); };
        auto toggle = [&] (juce::String id, juce::String name, bool def) { d.push_back ({ id, name, def ? 1.0f : 0.0f, 1, {}, 0, 0 }); };
        auto choice = [&] (juce::String id, juce::String name, juce::StringArray c, int def) {
            d.push_back ({ id, name, c.size() > 1 ? (float) def / (float) (c.size() - 1) : 0.0f, 2, c, 0, 0 });
        };
        auto integer = [&] (juce::String id, juce::String name, int lo, int hi, int def) {
            d.push_back ({ id, name, (float) (def - lo) / (float) (hi - lo), 3, {}, lo, hi });
        };

        for (auto [id, label] : { std::pair { "bass1", "Bass 1" }, std::pair { "bass2", "Bass 2" } })
        {
            juce::String p (id), l (label);
            knob (p + ".tuning", l + " Tuning", 0.5f);
            knob (p + ".cutoff", l + " Cutoff", 0.35f);
            knob (p + ".resonance", l + " Resonance", 0.78f);
            knob (p + ".envmod", l + " Env Mod", 0.55f);
            knob (p + ".decay", l + " Decay", 0.4f);
            knob (p + ".accent", l + " Accent", 0.7f);
            knob (p + ".volume", l + " Volume", 0.8f);
            toggle (p + ".waveform", l + " Square Wave", false);
            toggle (p + ".on", l + " On", true);
            integer (p + ".pattern", l + " Pattern", 1, 32, 1);
        }

        // DRUM 08
        for (auto& [k, n, v] : std::initializer_list<std::tuple<const char*, const char*, float>> {
                 { "bd.level", "BD Level", 0.8f }, { "bd.tone", "BD Tone", 0.5f }, { "bd.decay", "BD Decay", 0.6f },
                 { "sd.level", "SD Level", 0.7f }, { "sd.tone", "SD Tone", 0.5f }, { "sd.snappy", "SD Snappy", 0.6f },
                 { "lt.level", "LT Level", 0.6f }, { "lt.tuning", "LT Tuning", 0.5f },
                 { "mt.level", "MT Level", 0.6f }, { "mt.tuning", "MT Tuning", 0.5f },
                 { "ht.level", "HT Level", 0.6f }, { "ht.tuning", "HT Tuning", 0.5f },
                 { "rs.level", "RS Level", 0.6f }, { "cp.level", "CP Level", 0.7f }, { "cb.level", "CB Level", 0.5f },
                 { "cy.level", "CY Level", 0.5f }, { "cy.tone", "CY Tone", 0.5f }, { "cy.decay", "CY Decay", 0.6f },
                 { "oh.level", "OH Level", 0.6f }, { "oh.decay", "OH Decay", 0.4f }, { "ch.level", "CH Level", 0.7f },
                 { "ac.level", "Accent Level", 0.6f } })
            knob (juce::String ("r808.") + k, juce::String ("808 ") + n, v);
        for (auto& [k, n] : { std::pair { "lt.alt", "LT/LC" }, std::pair { "mt.alt", "MT/MC" }, std::pair { "ht.alt", "HT/HC" },
                              std::pair { "rs.alt", "RS/CL" }, std::pair { "cp.alt", "CP/MA" } })
            toggle (juce::String ("r808.") + k, juce::String ("808 ") + n, false);
        toggle ("r808.on", "808 On", true);
        integer ("r808.pattern", "808 Pattern", 1, 32, 1);

        // DRUM 09
        for (auto& [k, n, v] : std::initializer_list<std::tuple<const char*, const char*, float>> {
                 { "bd.tune", "BD Tune", 0.5f }, { "bd.level", "BD Level", 0.8f }, { "bd.attack", "BD Attack", 0.4f }, { "bd.decay", "BD Decay", 0.6f },
                 { "sd.tune", "SD Tune", 0.5f }, { "sd.tone", "SD Tone", 0.5f }, { "sd.snappy", "SD Snappy", 0.6f }, { "sd.level", "SD Level", 0.7f },
                 { "lt.tune", "LT Tune", 0.5f }, { "lt.decay", "LT Decay", 0.5f }, { "lt.level", "LT Level", 0.6f },
                 { "mt.tune", "MT Tune", 0.5f }, { "mt.decay", "MT Decay", 0.5f }, { "mt.level", "MT Level", 0.6f },
                 { "ht.tune", "HT Tune", 0.5f }, { "ht.decay", "HT Decay", 0.5f }, { "ht.level", "HT Level", 0.6f },
                 { "rs.level", "Rim Level", 0.6f }, { "cp.level", "Clap Level", 0.7f },
                 { "hh.level", "HH Level", 0.7f }, { "hh.chDecay", "CH Decay", 0.3f }, { "hh.ohDecay", "OH Decay", 0.5f },
                 { "cy.crLevel", "Crash Level", 0.5f }, { "cy.crTune", "Crash Tune", 0.5f },
                 { "cy.rdLevel", "Ride Level", 0.5f }, { "cy.rdTune", "Ride Tune", 0.5f },
                 { "ac.level", "Accent Level", 0.6f }, { "ac.flam", "Flam", 0.3f } })
            knob (juce::String ("r909.") + k, juce::String ("909 ") + n, v);
        toggle ("r909.on", "909 On", true);
        integer ("r909.pattern", "909 Pattern", 1, 32, 1);

        // Mixer
        const char* devs[] = { "bass1", "bass2", "r808", "r909" };
        const char* labels[] = { "Bass 1", "Bass 2", "808", "909" };
        for (int i = 0; i < 4; ++i)
        {
            juce::String p = juce::String ("mixer.") + devs[i] + ".", l = juce::String (labels[i]) + " ";
            knob (p + "pan", l + "Pan", 0.5f);
            knob (p + "delay", l + "Delay Send", i == 0 ? 0.4f : 0.1f);
            knob (p + "level", l + "Level", 0.72f);
            toggle (p + "mute", l + "Mute", false);
            toggle (p + "solo", l + "Solo", false);
            toggle (p + "dist", l + "Dist Insert", i == 0);
        }
        knob ("mixer.delayReturn", "Delay Return", 0.6f);
        knob ("mixer.master.level", "Master Fader", 0.8f);
        knob ("master.volume", "Master Volume", 0.8f);
        knob ("song.shuffle", "Shuffle", 0.5f);
        toggle ("song.mode", "Song Mode", false);

        // Effects
        toggle ("fx.delay.on", "Delay On", true);
        integer ("fx.delay.steps", "Delay Steps", 1, 32, 3);
        knob ("fx.delay.feedback", "Delay Feedback", 0.45f);
        knob ("fx.delay.pan", "Delay Pan", 0.5f);
        toggle ("fx.delay.triplet", "Delay Triplet", false);

        toggle ("fx.dist.on", "Dist On", true);
        knob ("fx.dist.amount", "Dist Amount", 0.4f);
        knob ("fx.dist.shape", "Dist Shape", 0.55f);

        toggle ("fx.comp.on", "Comp On", true);
        knob ("fx.comp.amount", "Comp Amount", 0.3f);
        knob ("fx.comp.threshold", "Comp Threshold", 0.5f);
        choice ("fx.comp.target", "Comp Target", { "MASTER", "BASS 1", "BASS 2", "DRUM 08", "DRUM 09" }, 0);
        toggle ("fx.comp.routed", "Comp Routed", true);

        toggle ("fx.pcf.on", "PCF On", true);
        knob ("fx.pcf.freq", "PCF Freq", 0.45f);
        knob ("fx.pcf.reso", "PCF Reso", 0.5f);
        knob ("fx.pcf.amount", "PCF Amount", 0.6f);
        knob ("fx.pcf.decay", "PCF Decay", 0.4f);
        integer ("fx.pcf.wave", "PCF Wave", 1, 56, 1);
        choice ("fx.pcf.mode", "PCF Mode", { "BP", "LP" }, 1);
        choice ("fx.pcf.target", "PCF Target", { "OFF", "BASS 1", "BASS 2", "DRUM 08", "DRUM 09" }, 0);

        // Internal tempo (used when the host provides none / standalone play)
        integer ("tempo", "Internal Tempo", 20, 300, 125);

        // RACK = sequencer plays all four devices; otherwise only that device sounds, played by MIDI notes
        choice ("play.mode", "Play Mode", { "RACK", "BASS 1", "BASS 2", "DRUM 08", "DRUM 09" }, 0);
        return d;
    }();
    return defs;
}

inline int paramIndex (const juce::String& id)
{
    static const std::unordered_map<juce::String, int> map = [] {
        std::unordered_map<juce::String, int> m;
        const auto& defs = paramDefs();
        for (int i = 0; i < (int) defs.size(); ++i)
            m[defs[(size_t) i].id] = i;
        return m;
    }();
    auto it = map.find (id);
    jassert (it != map.end());
    return it == map.end() ? -1 : it->second;
}

// Read-only view of the plugin parameters by table index (normalised values).
struct ParamView
{
    const std::vector<juce::RangedAudioParameter*>* params = nullptr;
    const float* overrides = nullptr; // song-mode automation (NaN = use the parameter)

    float operator[] (int idx) const
    {
        if (idx < 0)
            return 0.0f;
        if (overrides != nullptr && ! std::isnan (overrides[idx]))
            return overrides[idx];
        return (*params)[(size_t) idx]->getValue();
    }
    float get (const juce::String& id) const { return (*this)[paramIndex (id)]; }
    bool on (int idx) const { return (*this)[idx] >= 0.5f; }
    int choice (int idx, int n) const { return (int) std::lround ((*this)[idx] * (float) (n - 1)); }
};

} // namespace rb
