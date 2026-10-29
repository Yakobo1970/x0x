#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "RbsImport.h"
#include "MidiExport.h"

X0xProcessor::X0xProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "x0x", createLayout())
{
    for (const auto& def : rb::paramDefs())
        paramList.push_back (apvts.getParameter (def.id));

    patterns.loadDemo();
    engine = std::make_unique<rb::Engine> (patterns, rb::ParamView { &paramList });
    startTimerHz (30);
}

void X0xProcessor::timerCallback()
{
    rb::Engine::ParamWrite writes[512];
    // keep only the last value per parameter in this batch
    std::map<int, float> latest;
    for (int n; (n = engine->popParamWrites (writes, 512)) > 0;)
        for (int i = 0; i < n; ++i)
            latest[writes[i].index] = writes[i].value;
    for (auto [idx, value] : latest)
        if (auto* p = paramList[(size_t) idx]; std::abs (p->getValue() - value) > 1.0e-6f)
            p->setValueNotifyingHost (value);
}

juce::AudioProcessorValueTreeState::ParameterLayout X0xProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& d : rb::paramDefs())
    {
        const juce::ParameterID pid { d.id, 1 };
        switch (d.kind)
        {
            case 1:
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, d.name, d.def >= 0.5f));
                break;
            case 2:
                layout.add (std::make_unique<juce::AudioParameterChoice> (pid, d.name, d.choices,
                                                                          juce::roundToInt (d.def * (float) (d.choices.size() - 1))));
                break;
            case 3:
                layout.add (std::make_unique<juce::AudioParameterInt> (pid, d.name, d.intMin, d.intMax,
                                                                       d.intMin + juce::roundToInt (d.def * (float) (d.intMax - d.intMin))));
                break;
            default:
                layout.add (std::make_unique<juce::AudioParameterFloat> (pid, d.name, juce::NormalisableRange<float> (0.0f, 1.0f), d.def));
                break;
        }
    }
    return layout;
}

bool X0xProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void X0xProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    blockSize = juce::jmax (32, samplesPerBlock);
    engine->prepare (sampleRate, blockSize);
}

void X0xProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    rb::TransportInfo tr;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            tr.hostPlaying = pos->getIsPlaying();
            if (auto bpm = pos->getBpm())
                tr.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition())
            {
                tr.ppq = *ppq;
                tr.hasPpq = true;
            }
        }

    const int total = buffer.getNumSamples();
    const double sr = getSampleRate() > 0 ? getSampleRate() : 44100.0;
    for (int start = 0; start < total; start += blockSize)
    {
        const int n = juce::jmin (blockSize, total - start);
        juce::MidiBuffer chunkMidi;
        chunkMidi.addEvents (midi, start, n, -start);
        engine->process (buffer, start, n, tr, chunkMidi);
        if (tr.hostPlaying && tr.hasPpq && tr.bpm > 0.0)
            tr.ppq += n / sr * tr.bpm / 60.0;
    }
    midi.clear();
}

juce::AudioProcessorEditor* X0xProcessor::createEditor()
{
    return new X0xEditor (*this);
}

void X0xProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement root ("x0xState");
    root.setAttribute ("version", 1);
    if (auto params = apvts.copyState().createXml())
        root.addChildElement (params.release());
    root.addChildElement (patterns.toXml().release());
    if (auto song = engine->getSong())
    {
        auto songXml = song->toXml();
        songXml->setAttribute ("tempo", songTempo);
        root.addChildElement (songXml.release());
    }
    copyXmlToBinary (root, destData);
}

void X0xProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("x0xState"))
        return;
    if (auto* params = xml->getChildByName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*params));
    if (auto* pats = xml->getChildByName ("Patterns"))
        patterns.fromXml (*pats);
    if (auto* songXml = xml->getChildByName ("Song"))
    {
        auto song = std::make_shared<rb::Song>();
        song->fromXml (*songXml);
        songTitle = song->title;
        songTempo = songXml->getDoubleAttribute ("tempo");
        engine->setSong (std::move (song));
    }
}

juce::String X0xProcessor::importRbs (const juce::File& file)
{
    rb::RbsData data;
    auto parsed = std::make_unique<rb::PatternBank>();
    parsed->clearAll();
    const auto error = rb::importRbs (file, *parsed, data);
    if (error.isNotEmpty())
        return error;
    patterns.fromXml (*parsed->toXml());

    auto setParam = [this] (const juce::String& id, float normalised) {
        const int idx = rb::paramIndex (id);
        if (idx < 0)
            return;
        auto* p = paramList[(size_t) idx];
        p->beginChangeGesture();
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
        p->endChangeGesture();
    };

    // Reset everything to defaults first, then apply what the song defines.
    for (const auto& def : rb::paramDefs())
        setParam (def.id, def.def);
    for (const auto& [id, value] : data.params)
        setParam (id, value);
    for (int d = 0; d < rb::NumDevices; ++d)
    {
        auto* p = param (juce::String (rb::kDeviceIds[d]) + ".pattern");
        setParam (juce::String (rb::kDeviceIds[d]) + ".pattern", p->convertTo0to1 ((float) data.selectedPattern[d] + 1.0f));
        engine->requestPattern (d, data.selectedPattern[d]);
    }
    auto* tempo = param ("tempo");
    setParam ("tempo", tempo->convertTo0to1 ((float) juce::jlimit (20.0, 300.0, data.tempo)));

    // The RBS "title" field usually holds the author line ("by ..."), so lead with the file name.
    songTitle = file.getFileNameWithoutExtension();
    if (data.title.isNotEmpty())
        songTitle << "  -  " << data.title;
    songTempo = data.tempo;
    songMod = data.mod == "Standard ReBirth" ? juce::String() : data.mod;
    data.song.title = songTitle;
    for (auto* p : paramList)
        data.song.startValues.push_back (p->getValue());
    const bool hasSong = ! data.song.empty();
    engine->setSong (std::make_shared<rb::Song> (std::move (data.song)));
    setParam ("song.mode", hasSong && data.songMode ? 1.0f : 0.0f);
    return {};
}

void X0xProcessor::ejectSong()
{
    engine->setSong (nullptr);
    songTitle.clear();
    songMod.clear();
    songTempo = 0.0;
    if (auto* p = param ("song.mode"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (0.0f);
        p->endChangeGesture();
    }
}

juce::File X0xProcessor::exportSongMidi (const juce::File& target, rb::MidiExportKind kind)
{
    rb::MidiExportInput in;
    auto song = engine->getSong();
    in.song = song != nullptr && ! song->empty() ? song.get() : nullptr;
    in.params = &paramList;
    for (int d = 0; d < rb::NumDevices; ++d)
    {
        const int idx = rb::paramIndex (juce::String (rb::kDeviceIds[d]) + ".pattern");
        const float v = in.song != nullptr && (size_t) idx < in.song->startValues.size() ? in.song->startValues[(size_t) idx]
                                                                                         : paramList[(size_t) idx]->getValue();
        in.patterns[d] = juce::roundToInt (v * (rb::kPatterns - 1));
    }
    in.bpm = songTempo > 0.0 ? songTempo : (double) engine->tempo.load();
    in.title = songTitle.isNotEmpty() ? songTitle : juce::String ("x0x");
    in.bank = &patterns;

    juce::File out = target;
    if (out == juce::File())
    {
        const auto name = juce::File::createLegalFileName (in.title.upToFirstOccurrenceOf ("  -  ", false, false)).trim();
        out = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("x0x")
                  .getChildFile ((name.isEmpty() ? juce::String ("x0x") : name)
                                 + (kind == rb::MidiExportKind::Notes ? " (notes).mid" : " (patterns).mid"));
    }
    return rb::writeSongMidi (in, out, kind) ? out : juce::File();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new X0xProcessor();
}
