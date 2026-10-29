// Offline render of the demo patterns through the engine, for regression checks.
// Usage: RenderTest <out.wav> [bars] [solo-device 0..3 | -1] [sampleRate] [song.rbs]
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/dsp/Engine.h"
#include "../Source/RbsImport.h"
#include "../Source/MidiExport.h"

// FX regression: every effect control must audibly change the output, with and without a song.
static int fxTest (const char* rbsPath)
{
    struct Result { double rms; float gr; std::vector<float> out; };
    auto render = [&] (std::function<void (std::function<void (const char*, float)>)> setup, bool withSong) {
        std::vector<std::unique_ptr<juce::AudioParameterFloat>> owned;
        std::vector<juce::RangedAudioParameter*> params;
        for (auto& d : rb::paramDefs())
        {
            owned.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { d.id, 1 }, d.name,
                                                                          juce::NormalisableRange<float> (0, 1), d.def));
            params.push_back (owned.back().get());
        }
        auto set = [&] (const char* id, float v) { params[(size_t) rb::paramIndex (id)]->setValueNotifyingHost (v); };
        rb::PatternBank bank;
        bank.loadDemo();
        rb::Engine engine (bank, rb::ParamView { &params });
        if (withSong)
        {
            rb::RbsData data;
            bank.clearAll();
            rb::importRbs (juce::File (juce::String (rbsPath)), bank, data);
            for (auto& [id, v] : data.params)
                set (id.toRawUTF8(), v);
            set ("song.mode", 1.0f);
            engine.setSong (std::make_shared<rb::Song> (data.song));
        }
        engine.prepare (48000, 512);
        // play 2 bars with the song's own values, then the user turns knobs
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        rb::TransportInfo tr;
        tr.hostPlaying = tr.hasPpq = true;
        tr.bpm = 125;
        const int total = (int) (8 * 16 * 60.0 / 125 / 4 * 48000);
        double sum = 0;
        float gr = 0;
        std::vector<float> out;
        for (int s = 0, b = 0; s < total; s += 512, ++b)
        {
            if (s >= total / 4 && s - 512 < total / 4)
                setup (set);
            tr.ppq = s / 48000.0 * 125 / 60.0;
            engine.process (buf, 0, 512, tr, midi);
            // like the processor's timer: mirror song automation into the parameters
            rb::Engine::ParamWrite writes[512];
            for (int n; (n = engine.popParamWrites (writes, 512)) > 0;)
                for (int i = 0; i < n; ++i)
                    params[(size_t) writes[i].index]->setValueNotifyingHost (writes[i].value);
            if (s >= total / 2)
            {
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 512; ++i)
                    {
                        sum += buf.getSample (c, i) * buf.getSample (c, i);
                        out.push_back (buf.getSample (c, i));
                    }
                gr = std::min (gr, engine.compReduction.load());
            }
        }
        return Result { std::sqrt (sum / total), gr, out };
    };

    int fails = 0;
    for (bool withSong : { false, true })
    {
        if (withSong && rbsPath == nullptr)
            continue;
        std::printf ("--- %s\n", withSong ? "song mode" : "pattern mode");
        const auto base = render ([] (auto) {}, withSong);
        auto check = [&] (const char* what, std::function<void (std::function<void (const char*, float)>)> fn) {
            const auto r = render (fn, withSong);
            const double db = 20 * std::log10 ((r.rms + 1e-9) / (base.rms + 1e-9));
            double diff = 0, ref = 1e-12;
            for (size_t i = 0; i < std::min (r.out.size(), base.out.size()); ++i)
            {
                diff += (r.out[i] - base.out[i]) * (r.out[i] - base.out[i]);
                ref += base.out[i] * base.out[i];
            }
            const double change = 10 * std::log10 (diff / ref + 1e-12); // energy of the change relative to the signal
            const bool ok = change > -30.0;
            std::printf ("%-36s %+6.2f dB  change %6.1f dB  GR %5.1f dB  %s\n", what, db, change, r.gr, ok ? "ok" : "NO EFFECT");
            fails += ok ? 0 : 1;
        };
        // Each case changes the effect's state regardless of what the song had set.
        check ("delay on, send max", [] (auto set) { set ("fx.delay.on", 1); set ("mixer.bass1.delay", 1); set ("mixer.bass2.delay", 1); });
        check ("delay off", [] (auto set) { set ("fx.delay.on", 0); set ("mixer.bass1.delay", 1); });
        check ("dist on bass 1, amount max", [] (auto set) { set ("fx.dist.on", 1); set ("fx.dist.amount", 1); set ("mixer.bass1.dist", 1); });
        check ("comp on master, amount max, thr min", [] (auto set) {
            set ("fx.comp.on", 1); set ("fx.comp.routed", 1); set ("fx.comp.target", 0); set ("fx.comp.amount", 1); set ("fx.comp.threshold", 0); });
        check ("pcf on bass 1", [] (auto set) { set ("fx.pcf.on", 1); set ("fx.pcf.target", 0.25f); set ("fx.pcf.amount", 1); });
        check ("pcf on bass 2", [] (auto set) { set ("fx.pcf.on", 1); set ("fx.pcf.target", 0.5f); set ("fx.pcf.amount", 1); });
        check ("bass 1 muted (is it playing?)", [] (auto set) { set ("mixer.bass1.mute", 1); });
    }
    return fails;
}

// MIDI round trip: song mode vs. the exported MIDI file driving x0x via notes + CCs.
static int midiRoundTrip (const char* rbsPath, int bars, const char* midOut)
{
    struct Rig
    {
        std::vector<std::unique_ptr<juce::AudioParameterFloat>> owned;
        std::vector<juce::RangedAudioParameter*> params;
        rb::PatternBank bank;
        rb::RbsData data;
        std::unique_ptr<rb::Engine> engine;
        Rig (const char* path)
        {
            for (auto& d : rb::paramDefs())
            {
                owned.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { d.id, 1 }, d.name,
                                                                              juce::NormalisableRange<float> (0, 1), d.def));
                params.push_back (owned.back().get());
            }
            bank.clearAll();
            rb::importRbs (juce::File (juce::String (path)), bank, data);
            for (auto& [id, v] : data.params)
                params[(size_t) rb::paramIndex (id)]->setValueNotifyingHost (v);
            for (int d = 0; d < rb::NumDevices; ++d)
                params[(size_t) rb::paramIndex (juce::String (rb::kDeviceIds[d]) + ".pattern")]->setValueNotifyingHost (data.selectedPattern[d] / 31.0f);
            if (const char* off = std::getenv ("RB_FORCE_OFF")) // e.g. fx.pcf.on: pin a switch off in both rigs
                for (auto& t : data.song.tracks)
                    t.erase (std::remove_if (t.begin(), t.end(), [&] (const rb::SongEvent& e) {
                                 return e.param == rb::paramIndex (off); }), t.end());
            if (const char* off = std::getenv ("RB_FORCE_OFF"))
                params[(size_t) rb::paramIndex (off)]->setValueNotifyingHost (0);
            if (const char* solo = std::getenv ("RB_SOLO"))
                params[(size_t) rb::paramIndex (juce::String ("mixer.") + rb::kDeviceIds[std::atoi (solo)] + ".solo")]->setValueNotifyingHost (1);
            for (auto* p : params)
                data.song.startValues.push_back (p->getValue());
            engine = std::make_unique<rb::Engine> (bank, rb::ParamView { &params });
            engine->prepare (48000, 512);
        }
        void mirror()
        {
            rb::Engine::ParamWrite w[512];
            for (int n; (n = engine->popParamWrites (w, 512)) > 0;)
                for (int i = 0; i < n; ++i)
                    params[(size_t) w[i].index]->setValueNotifyingHost (w[i].value);
        }
    };

    Rig a (rbsPath), b (rbsPath);
    const double bpm = a.data.tempo, sr = 48000;
    a.params[(size_t) rb::paramIndex ("song.mode")]->setValueNotifyingHost (1);
    a.engine->setSong (std::make_shared<rb::Song> (a.data.song));

    if (std::getenv ("RB_QUANT16")) // align events to 16ths like song mode applies them
        for (auto& t : b.data.song.tracks)
            for (auto& e : t)
                e.tick = (e.tick + 1) & ~1;
    rb::MidiExportInput in;
    in.song = &b.data.song;
    in.params = &b.params;
    for (int d = 0; d < rb::NumDevices; ++d)
        in.patterns[d] = b.data.selectedPattern[d];
    in.bpm = bpm;
    in.title = "roundtrip";
    in.bank = &b.bank;
    in.notesWhileOff = std::getenv ("RB_ALL_NOTES") != nullptr;
    // RB_NOTES=1 with RB_SOLO=<device>: note export played by x0x in single mode for that device
    const bool notesMode = std::getenv ("RB_NOTES") != nullptr && std::getenv ("RB_SOLO") != nullptr;
    const int notesDev = notesMode ? std::atoi (std::getenv ("RB_SOLO")) : -1;
    const auto kind = notesMode ? rb::MidiExportKind::Notes : rb::MidiExportKind::Patterns;
    if (notesMode)
        b.params[(size_t) rb::paramIndex ("play.mode")]->setValueNotifyingHost ((notesDev + 1) / 4.0f);
    const auto midi = rb::buildSongMidi (in, kind);
    if (midOut != nullptr)
        rb::writeSongMidi (in, juce::File (juce::String (midOut)), kind);

    if (const char* db = std::getenv ("RB_DUMP_BAR")) // note export of one bar, plus the bass pattern played there
    {
        const double from = (std::atoi (db) - 1) * 4.0 * rb::kMidiPpq, to = from + 4.0 * rb::kMidiPpq;
        for (auto* e : *midi.getTrack (notesDev + 1))
            if (e->message.isNoteOnOrOff() && e->message.getTimeStamp() >= from && e->message.getTimeStamp() < to)
                std::printf ("%7.2f step %5.2f %s %d vel %d\n", e->message.getTimeStamp(), (e->message.getTimeStamp() - from) / 24.0,
                             e->message.isNoteOn() ? "ON " : "off", e->message.getNoteNumber(), e->message.getVelocity());
        if (const char* pp = std::getenv ("RB_DUMP_PAT"))
        {
            auto& pat = b.bank.bassPattern (notesDev, std::atoi (pp));
            for (int i = 0; i < pat.length; ++i)
            {
                const auto st = pat.step (i);
                std::printf ("step %2d note %2d oct %2d gate %d tie %d acc %d slide %d\n", i, st.note, st.octave, st.gate, st.tie, st.accent, st.slide);
            }
        }
    }

    // flatten the tracks into one sequence in MIDI ticks
    juce::MidiMessageSequence all;
    for (int t = 1; t < midi.getNumTracks(); ++t)
        if (! notesMode || t == notesDev + 1 || t == rb::kMidiMixerChannel)
            all.addSequence (*midi.getTrack (t), 0.0);
    all.sort();
    int notes = 0, ccs = 0;
    for (auto* e : all)
    {
        notes += e->message.isNoteOn() ? 1 : 0;
        ccs += e->message.isController() ? 1 : 0;
    }
    std::printf ("MIDI: %d tracks, %d pattern notes, %d CCs\n", midi.getNumTracks(), notes, ccs);

    const int total = (int) (bars * 16 * 60.0 / bpm / 4.0 * sr);
    const double samplesPerTick = sr * 60.0 / bpm / rb::kMidiPpq;
    juce::AudioBuffer<float> ba (2, 512), bb (2, 512);
    rb::TransportInfo tr;
    tr.hostPlaying = tr.hasPpq = true;
    tr.bpm = bpm;
    juce::MidiBuffer none, chunk;
    int ev = 0;
    double diff = 0, ref = 1e-12, sa = 0, sb = 0;
    const int barLen = (int) (16 * 60.0 / bpm / 4.0 * sr);
    double barDiff = 0, barRef = 1e-12;
    // phase-independent check: RMS envelopes per 2048-sample window (free-running oscillators and
    // noise can drift apart while a device is muted, which a waveform diff would count)
    double envA = 0, envB = 0, envDiff = 0, envRef = 1e-12;
    int envN = 0;
    juce::String focus;
    juce::String barList;
    for (int s = 0; s < total; s += 512)
    {
        if (s / barLen != (s + 512) / barLen)
        {
            barList << juce::String (10 * std::log10 (barDiff / barLen + 1e-12), 0) << ' ';
            barDiff = 0;
            barRef = 1e-12;
        }
        tr.ppq = s / sr * bpm / 60.0;
        chunk.clear();
        for (; ev < all.getNumEvents(); ++ev)
        {
            const long long pos = std::llround (all.getEventPointer (ev)->message.getTimeStamp() * samplesPerTick) - s;
            if (pos >= 512)
                break;
            if (! all.getEventPointer (ev)->message.isMetaEvent())
                chunk.addEvent (all.getEventPointer (ev)->message, (int) std::max (0LL, pos));
        }
        a.engine->process (ba, 0, 512, tr, none);
        b.engine->process (bb, 0, 512, tr, chunk);
        if (const char* tb = std::getenv ("RB_TRACE_BAR"))
        {
            const int bar = (int) (tr.ppq / 4.0) + 1;
            const int td = std::getenv ("RB_TRACE_DEV") ? std::atoi (std::getenv ("RB_TRACE_DEV")) : 0;
            if (bar >= std::atoi (tb) - 1 && bar <= std::atoi (tb))
                std::printf ("ppq %8.3f  A pat %d step %2d | B pat %d step %2d  chunk %d ev\n", tr.ppq, a.engine->currentPattern[td].load(),
                             a.engine->uiStep[td].load(), b.engine->currentPattern[td].load(), b.engine->uiStep[td].load(), chunk.getNumEvents());
        }
        a.mirror();
        b.mirror();
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
            {
                const float x = ba.getSample (c, i), y = bb.getSample (c, i);
                diff += (x - y) * (x - y);
                ref += x * x;
                barDiff += (x - y) * (x - y);
                barRef += x * x;
                if (const char* fb = std::getenv ("RB_FOCUS_BAR"))
                {
                    const int sample = s + i;
                    const int stepLen = barLen / 16;
                    if (c == 0 && sample / barLen + 1 == std::atoi (fb) && sample % stepLen == 0)
                        focus << "|";
                    if (c == 0 && sample / barLen + 1 == std::atoi (fb) && sample % (stepLen / 4) == 0)
                        focus << juce::String (x, 3) << "/" << juce::String (y, 3) << " ";
                }
                sa += x * x;
                sb += y * y;
                envA += x * x;
                envB += y * y;
                if (++envN == 2048)
                {
                    const double ra = std::sqrt (envA / envN), rb = std::sqrt (envB / envN);
                    envDiff += (ra - rb) * (ra - rb);
                    envRef += ra * ra;
                    envA = envB = 0;
                    envN = 0;
                }
            }
    }
    if (std::getenv ("RB_DUMP_PARAMS"))
        for (size_t i = 0; i < a.params.size(); ++i)
            if (std::abs (a.params[i]->getValue() - b.params[i]->getValue()) > 0.002f)
                std::printf ("param %-24s song %.4f  midi %.4f\n", rb::paramDefs()[i].id.toRawUTF8(), a.params[i]->getValue(), b.params[i]->getValue());
    const double change = 10 * std::log10 (diff / ref + 1e-12);
    std::printf ("song mode rms %.4f | MIDI-driven rms %.4f | difference %.1f dB | envelope difference %.1f dB\n",
                 std::sqrt (sa / (2.0 * total)), std::sqrt (sb / (2.0 * total)), change, 10 * std::log10 (envDiff / envRef + 1e-12));
    if (focus.isNotEmpty())
        std::printf ("focus (song/midi per 1/64): %s\n", focus.toRawUTF8());
    if (std::getenv ("RB_BARS"))
        std::printf ("per bar diff dB: %s\n", barList.toRawUTF8());
    return change < -20.0 ? 0 : 1;
}

// Prints the CC map as Markdown.
static int printCcMap()
{
    const auto& map = rb::MidiMap::get();
    const char* names[] = { "", "BASS 1", "BASS 2", "DRUM 08", "DRUM 09", "MIXER + FX" };
    std::printf ("# x0x MIDI-Steuerung\n\n");
    std::printf ("## MODE: RACK (\"PATTERNS TO DAW\")\n\n"
                 "Noten 36-67 (C1-G3, C3 = 60) auf Kanal 1-4 wählen Pattern A1-D8. Velocity >= 100 wechselt sofort "
                 "und startet das Pattern neu, kleinere Velocity wechselt am Pattern-Ende.\n\n"
                 "## MODE: BASS 1 ... DRUM 09 (Single Mode, \"NOTES TO DAW\")\n\n"
                 "Nur das gewählte Gerät klingt, gespielt von MIDI-Noten auf beliebigem Kanal. CCs auf Kanal 5 steuern "
                 "Mixer + FX, CCs auf jedem anderen Kanal das Gerät (Nummern wie unten).\n\n"
                 "- **303:** Note 36 (C1) = Pattern-Note C, Oktave 0. Überlappende Noten gleiten (Slide), "
                 "Velocity >= 100 = Accent.\n"
                 "- **Drums (GM):** BD 36, RS 37, SD 38, CP 39, LT 41, CH 42, MT 45, OH 46, HT 48, CY/Crash 49, "
                 "Ride 51, Cowbell 56. Velocity < 64 = Flam-Vorschlag (halbe Lautstärke), 64-99 normal, "
                 "100-119 = 909-Einzelaccent (808: Accent), 120-126 = Accent, 127 = Accent + Einzelaccent.\n");
    for (int ch = 1; ch <= rb::kMidiMixerChannel; ++ch)
    {
        std::printf ("\n## Kanal %d - %s\n\n| CC | Parameter |\n|---:|---|\n", ch, names[ch]);
        for (int cc = 0; cc < 128; ++cc)
            if (const int p = map.paramFor[ch][cc]; p >= 0)
                std::printf ("| %d | %s (`%s`) |\n", cc, rb::paramDefs()[(size_t) p].name.toRawUTF8(), rb::paramDefs()[(size_t) p].id.toRawUTF8());
    }
    return 0;
}

// Transport regression: internal PLAY must yield to host stop / cursor jumps.
static int transportTest()
{
    std::vector<std::unique_ptr<juce::AudioParameterFloat>> owned;
    std::vector<juce::RangedAudioParameter*> params;
    for (auto& d : rb::paramDefs())
    {
        owned.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { d.id, 1 }, d.name,
                                                                      juce::NormalisableRange<float> (0, 1), d.def));
        params.push_back (owned.back().get());
    }
    rb::PatternBank bank;
    bank.loadDemo();
    rb::Engine engine (bank, rb::ParamView { &params });
    engine.prepare (48000, 512);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    rb::TransportInfo tr;
    tr.hasPpq = true;
    tr.bpm = 120;
    tr.ppq = 8.0;
    auto run = [&] (int blocks) { for (int i = 0; i < blocks; ++i) engine.process (buf, 0, 512, tr, midi); };
    int fails = 0;
    auto expect = [&] (const char* what, bool cond) { std::printf ("%-48s %s\n", what, cond ? "ok" : "FAIL"); fails += cond ? 0 : 1; };

    engine.internalPlay = true;
    run (20);
    expect ("internal play runs while host stopped", engine.isPlaying.load());
    tr.ppq = 0.0; // STOP pressed again in the DAW -> cursor returns
    run (2);
    expect ("cursor jump while stopped ends internal play", ! engine.isPlaying.load());

    engine.internalPlay = true;
    run (5);
    tr.hostPlaying = true;
    run (10);
    expect ("host start takes over", engine.isPlaying.load() && ! engine.internalPlay.load());
    tr.hostPlaying = false;
    run (2);
    expect ("host stop stops everything", ! engine.isPlaying.load());

    engine.internalPlay = true;
    tr.hasPpq = false;
    run (20);
    expect ("internal play without host position keeps running", engine.isPlaying.load());
    return fails;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 1 && juce::String (argv[1]) == "transport-test")
        return transportTest();
    if (argc > 2 && juce::String (argv[1]) == "midi-roundtrip")
        return midiRoundTrip (argv[2], argc > 3 ? juce::String (argv[3]).getIntValue() : 32, argc > 4 ? argv[4] : nullptr);
    if (argc > 1 && juce::String (argv[1]) == "cc-map")
        return printCcMap();
    if (argc > 1 && juce::String (argv[1]) == "fx-test")
        return fxTest (argc > 2 ? argv[2] : nullptr);
    const juce::File outFile (argc > 1 ? juce::String (argv[1]) : juce::String ("render.wav"));
    const int bars = argc > 2 ? juce::String (argv[2]).getIntValue() : 4;
    const int solo = argc > 3 ? juce::String (argv[3]).getIntValue() : -1;
    const double sr = argc > 4 ? juce::String (argv[4]).getDoubleValue() : 48000.0;

    std::vector<std::unique_ptr<juce::AudioParameterFloat>> owned;
    std::vector<juce::RangedAudioParameter*> params;
    for (auto& d : rb::paramDefs())
    {
        owned.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { d.id, 1 }, d.name,
                                                                      juce::NormalisableRange<float> (0, 1), d.def));
        params.push_back (owned.back().get());
    }
    auto set = [&] (const char* id, float v) { params[(size_t) rb::paramIndex (id)]->setValueNotifyingHost (v); };
    if (solo >= 0)
        set ((juce::String ("mixer.") + rb::kDeviceIds[solo] + ".solo").toRawUTF8(), 1.0f);

    rb::PatternBank bank;
    bank.loadDemo();
    rb::Engine engine (bank, rb::ParamView { &params });
    double bpm = 125.0;
    if (argc > 5)
    {
        rb::RbsData data;
        bank.clearAll();
        const auto err = rb::importRbs (juce::File (juce::String (argv[5])), bank, data);
        if (err.isNotEmpty()) { std::printf ("import failed: %s\n", err.toRawUTF8()); return 2; }
        for (auto& [id, v] : data.params)
            set (id.toRawUTF8(), v);
        for (int d = 0; d < rb::NumDevices; ++d)
            set ((juce::String (rb::kDeviceIds[d]) + ".pattern").toRawUTF8(), data.selectedPattern[d] / 31.0f);
        set ("song.mode", data.songMode ? 1.0f : 0.0f);
        if (const char* v = std::getenv ("RB_MASTER_VOL")) set ("master.volume", (float) std::atof (v));
        if (std::getenv ("RB_PATTERN_MODE")) set ("song.mode", 0.0f);
        if (std::getenv ("RB_NO_DELAY")) set ("fx.delay.on", 0.0f);
        bpm = data.tempo;
        int events = 0;
        for (auto& t : data.song.tracks) events += (int) t.size();
        std::printf ("song '%s' tempo=%.2f mode=%s length=%d bars loop=%d (%d-%d) events=%d\n", data.title.toRawUTF8(), data.tempo,
                     data.songMode ? "song" : "pattern", data.song.length / 32, data.song.loop ? 1 : 0, data.song.loopStart,
                     data.song.loopEnd, events);
        engine.setSong (std::make_shared<rb::Song> (data.song));
        if (const char* tr = std::getenv ("RB_DUMP_TRACK"))
            for (auto& e : data.song.tracks[std::atoi (tr)])
                std::printf ("tick %5d bar %3d step %5.1f  %-22s %.3f\n", e.tick, e.tick / 32 + 1, (e.tick % 32) / 2.0,
                             e.param >= 0 ? rb::paramDefs()[(size_t) e.param].id.toRawUTF8() : "PATTERN", e.value);
        if (std::getenv ("RB_DUMP_FX"))
            for (int t = 0; t < rb::NumSongTracks; ++t)
                for (auto& e : data.song.tracks[t])
                    if (e.param >= 0 && rb::paramDefs()[(size_t) e.param].id.startsWith ("fx."))
                        std::printf ("track %d tick %5d %-20s %.3f\n", t, e.tick, rb::paramDefs()[(size_t) e.param].id.toRawUTF8(), e.value);
    }
    const int block = 512;
    engine.prepare (sr, block);

    const int total = (int) (bars * 16 * 60.0 / bpm / 4.0 * sr);
    juce::AudioBuffer<float> out (2, total);
    juce::MidiBuffer midi;
    rb::TransportInfo tr;
    tr.hostPlaying = true;
    tr.hasPpq = true;
    tr.bpm = bpm;
    for (int s = 0; s < total; s += block)
    {
        const int n = std::min (block, total - s);
        tr.ppq = s / sr * bpm / 60.0;
        engine.process (out, s, n, tr, midi);
    }

    bool bad = false;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < total; ++i)
            if (! std::isfinite (out.getSample (c, i))) bad = true;
    std::printf ("samples=%d peak=%.3f rmsL=%.4f rmsR=%.4f nan=%d\n", total, out.getMagnitude (0, total),
                 out.getRMSLevel (0, 0, total), out.getRMSLevel (1, 0, total), bad ? 1 : 0);

    outFile.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os = outFile.createOutputStream();
    auto writer = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (24));
    if (writer != nullptr)
        writer->writeFromAudioSampleBuffer (out, 0, total);
    return bad ? 1 : 0;
}
