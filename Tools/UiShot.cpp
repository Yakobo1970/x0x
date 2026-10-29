// Renders the x0x editor into a PNG.
// Usage: UiShot <out.png> [play mode 0-4] [song.rbs]
#include "../Source/PluginProcessor.h"
#include "../Source/IconArt.h"

class UiShotApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "UiShot"; }
    const juce::String getApplicationVersion() override { return "1"; }

    void initialise (const juce::String&) override
    {
        const auto args = getCommandLineParameterArray();
        if (args.size() > 1 && args[0] == "icon") // UiShot icon <dir>: icon_<size>.png
        {
            for (int size : { 16, 20, 24, 32, 40, 48, 64, 128, 256, 512, 1024 })
            {
                const auto out = juce::File (args[1]).getChildFile ("icon_" + juce::String (size) + ".png");
                out.deleteFile();
                juce::FileOutputStream os (out);
                juce::PNGImageFormat().writeImageToStream (rb::icon::render (size), os);
            }
            quit();
            return;
        }
        X0xProcessor proc;
        if (args.size() > 2 && args[2].isNotEmpty())
            proc.importRbs (juce::File (args[2]));
        if (args.size() > 1)
            proc.param ("play.mode")->setValueNotifyingHost (args[1].getIntValue() / 4.0f);
        {
            std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
            ed->setSize (1350, 960);
            if (args.size() > 3 && args[3] == "about") // UiShot out.png 0 "" about [tab]
            {
                std::function<void (juce::Component&)> visit = [&] (juce::Component& c) {
                    if (auto* b = dynamic_cast<juce::Button*> (&c); b != nullptr && b->getName() == "about" && b->onClick)
                        b->onClick();
                    if (auto* t = dynamic_cast<juce::TabbedComponent*> (&c); t != nullptr && args.size() > 4)
                        t->setCurrentTabIndex (args[4].getIntValue());
                    for (auto* child : c.getChildren())
                        visit (*child);
                };
                visit (*ed);
                visit (*ed);
            }
            const auto img = ed->createComponentSnapshot (ed->getLocalBounds());
            const juce::File out { args.isEmpty() ? juce::String ("uishot.png") : args[0] };
            out.deleteFile();
            juce::FileOutputStream os (out);
            juce::PNGImageFormat().writeImageToStream (img, os);
        }
        quit();
    }

    void shutdown() override {}
};

START_JUCE_APPLICATION (UiShotApp)
