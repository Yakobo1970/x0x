// Loads the built VST3 like a DAW would: host transport sync, rendering and state round-trip.
// Usage: HostTest <path-to.vst3>
#include <juce_audio_processors/juce_audio_processors.h>

struct FakePlayHead : juce::AudioPlayHead
{
    double ppq = 0, bpm = 125;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setIsPlaying (playing);
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        return p;
    }
};

static double renderRms (juce::AudioPluginInstance& inst, FakePlayHead& ph, double sr, int bars)
{
    const int block = 512;
    const int total = (int) (bars * 16 * 60.0 / ph.bpm / 4.0 * sr);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    double sum = 0;
    for (int s = 0; s < total; s += block)
    {
        ph.ppq = s / sr * ph.bpm / 60.0;
        buf.clear();
        inst.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < block; ++i)
                sum += buf.getSample (c, i) * buf.getSample (c, i);
    }
    return std::sqrt (sum / (2.0 * total));
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::AudioPluginFormatManager fm;
    juce::addDefaultFormatsToManager (fm);
    juce::OwnedArray<juce::PluginDescription> found;
    juce::VST3PluginFormat vst3;
    vst3.findAllTypesForFile (found, argc > 1 ? argv[1] : "");
    if (found.isEmpty()) { std::printf ("plugin not found\n"); return 1; }
    std::printf ("found: %s (%s) instrument=%d\n", found[0]->name.toRawUTF8(), found[0]->manufacturerName.toRawUTF8(), found[0]->isInstrument ? 1 : 0);

    juce::String err;
    const double sr = 48000;
    auto inst = fm.createPluginInstance (*found[0], sr, 512, err);
    if (inst == nullptr) { std::printf ("load failed: %s\n", err.toRawUTF8()); return 1; }
    FakePlayHead ph;
    inst->setPlayHead (&ph);
    inst->prepareToPlay (sr, 512);
    std::printf ("params=%d\n", inst->getParameters().size());

    const double rms1 = renderRms (*inst, ph, sr, 4);
    std::printf ("host-sync render rms=%.4f\n", rms1);

    juce::MemoryBlock state;
    inst->getStateInformation (state);
    // change something, then restore
    for (auto* p : inst->getParameters())
        if (p->getName (64) == "Bass 1 Cutoff") p->setValueNotifyingHost (0.9f);
    inst->setStateInformation (state.getData(), (int) state.getSize());
    inst->releaseResources();
    inst->prepareToPlay (sr, 512);
    const double rms2 = renderRms (*inst, ph, sr, 4);
    std::printf ("after state restore rms=%.4f (state %d bytes)\n", rms2, (int) state.getSize());

    ph.playing = false;
    const double rmsStop = renderRms (*inst, ph, sr, 1);
    std::printf ("stopped rms=%.5f\n", rmsStop);

    if (argc > 2) // single mode: transport stopped, one MIDI note must sound
    {
        const int mode = juce::String (argv[2]).getIntValue();
        for (auto* p : inst->getParameters())
            if (p->getName (64) == "Play Mode") p->setValueNotifyingHost (mode / 4.0f);
        juce::AudioBuffer<float> buf (2, 512);
        double sum = 0;
        for (int b = 0; b < 40; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 2) midi.addEvent (juce::MidiMessage::noteOn (1, mode <= 2 ? 48 : 38, (juce::uint8) 100), 0);
            if (b == 12) midi.addEvent (juce::MidiMessage::noteOff (1, mode <= 2 ? 48 : 38), 0);
            buf.clear();
            inst->processBlock (buf, midi);
            sum += buf.getRMSLevel (0, 0, 512);
        }
        std::printf ("single mode %d: live note rms %.4f\n", mode, sum / 40);
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (inst->createEditorIfNeeded());
    std::printf ("editor=%s %dx%d\n", ed ? "ok" : "none", ed ? ed->getWidth() : 0, ed ? ed->getHeight() : 0);
    if (ed != nullptr && argc > 3)
    {
        ed->setSize (1350, 960);
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds());
        const juce::File out { juce::String (argv[3]) };
        out.deleteFile();
        juce::FileOutputStream os (out);
        juce::PNGImageFormat().writeImageToStream (img, os);
    }
    ed.reset();
    inst->releaseResources();
    return 0;
}
