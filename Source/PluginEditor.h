#pragma once

#include "PluginProcessor.h"

namespace rb::ui
{
class RackLookAndFeel;
}

class X0xEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit X0xEditor (X0xProcessor&);
    ~X0xEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kBaseW = 1350, kBaseH = 960;

private:
    void timerCallback() override;

    X0xProcessor& proc;
    std::unique_ptr<rb::ui::RackLookAndFeel> lnf;
    std::unique_ptr<juce::Component> content;
    juce::TooltipWindow tooltips { this, 500 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (X0xEditor)
};
