// "About x0x" panel: shown over the rack when the logo is clicked.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace rb::ui
{
class AboutOverlay : public juce::Component
{
public:
    AboutOverlay();
    ~AboutOverlay() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override; // click outside the panel closes it

    static constexpr const char* kWebsiteUrl = "https://www.undabtanzbar.de";
    static constexpr const char* kDonateUrl = "https://paypal.me/yakobo";
    static constexpr const char* kGithubUrl = "https://github.com/Yakobo1970";
    static constexpr const char* kReRebirthUrl = "https://rb.varyform.info";

private:
    juce::Rectangle<int> panelBounds() const;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::TextButton donate { juce::CharPointer_UTF8 ("♥  Spenden via PayPal") }, website { "www.undabtanzbar.de" },
        github { "GitHub" }, close { juce::CharPointer_UTF8 ("Schließen") };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutOverlay)
};

} // namespace rb::ui
