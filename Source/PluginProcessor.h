#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "Patterns.h"
#include "dsp/Engine.h"
#include "MidiExport.h"

class X0xProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    X0xProcessor();
    ~X0xProcessor() override { stopTimer(); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    rb::PatternBank patterns;
    std::vector<juce::RangedAudioParameter*> paramList;
    std::unique_ptr<rb::Engine> engine;

    juce::RangedAudioParameter* param (const juce::String& id) const { return paramList[(size_t) rb::paramIndex (id)]; }

    // Imports a ReBirth 2.0 .rbs song (patterns, knobs, mixer, effects, arrangement).
    // Must be called on the message thread. Returns an error message or an empty string.
    juce::String importRbs (const juce::File& file);
    juce::String songTitle, songMod;

    // Writes the song (or the current state) as a 5-track MIDI file; empty target = temp file.
    // Returns the written file or juce::File() on failure. Message thread only.
    juce::File exportSongMidi (const juce::File& target = {}, rb::MidiExportKind kind = rb::MidiExportKind::Patterns);

    // Removes the imported song (patterns and knobs stay as they are).
    void ejectSong();
    double songTempo = 0.0;

private:
    void timerCallback() override; // mirrors song automation into the parameters
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    int blockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (X0xProcessor)
};
