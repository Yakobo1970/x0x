#include "AboutOverlay.h"
#include "IconArt.h"

#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "1.0.0"
#endif

namespace rb::ui
{
namespace
{
    // sources are compiled as UTF-8 (/utf-8)
    juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

    const juce::Colour kInk (0xffe9e4d6), kMuted (0xffa29d92), kHead (0xffffa53a), kLink (0xff9fe870);

    // One scrollable page of formatted text: "# " = heading, "- " = bullet, "" = paragraph break.
    class TextPage : public juce::Component
    {
    public:
        explicit TextPage (std::initializer_list<const char*> lines)
        {
            for (auto* l : lines)
            {
                const auto line = u8 (l);
                if (line.startsWith ("# "))
                {
                    if (text.getText().isNotEmpty())
                        text.append ("\n", font (8.0f), kInk);
                    text.append (line.substring (2) + "\n", font (16.0f, true), kHead);
                }
                else if (line.startsWith ("- "))
                    text.append (u8 ("  •  ") + line.substring (2) + "\n", font (14.0f), kInk);
                else if (line.startsWith ("~ "))
                    text.append (line.substring (2) + "\n", font (13.0f, false, true), kMuted);
                else
                    text.append (line + "\n", font (14.0f), kInk);
            }
            text.setWordWrap (juce::AttributedString::byWord);
            text.setLineSpacing (2.0f);
        }

        void layoutFor (int width)
        {
            layout.createLayout (text, (float) width - 24.0f);
            setSize (width, (int) std::ceil (layout.getHeight()) + 24);
        }

        void paint (juce::Graphics& g) override
        {
            layout.draw (g, getLocalBounds().reduced (12).toFloat());
        }

    private:
        static juce::Font font (float size, bool bold = false, bool italic = false)
        {
            return juce::Font (juce::FontOptions (size, (bold ? juce::Font::bold : 0) | (italic ? juce::Font::italic : 0)));
        }

        juce::AttributedString text;
        juce::TextLayout layout;
    };

    class Page : public juce::Viewport
    {
    public:
        explicit Page (std::initializer_list<const char*> lines) : content (lines)
        {
            setViewedComponent (&content, false);
            setScrollBarsShown (true, false);
        }

        void resized() override
        {
            juce::Viewport::resized();
            content.layoutFor (getMaximumVisibleWidth());
        }

    private:
        TextPage content;
    };
}

AboutOverlay::AboutOverlay()
{
    const auto tabColour = juce::Colour (0xff1b1717);
    tabs.setTabBarDepth (30);
    tabs.setOutline (0);
    tabs.addTab (u8 ("Über x0x"), tabColour, new Page ({
        "# Was ist x0x?",
        "x0x ist ein virtuelles Acid-Rack als VST3-Plugin und eigenständiges Programm: zwei 303-artige "
        "Bass-Synthesizer, die Drumcomputer DRUM 08 und DRUM 09, ein Pattern-Sequencer mit Host-Sync, "
        "Mischpult, Delay, Distortion, Compressor und Pattern-Filter – eingebaut in ein 19-Zoll-Rack.",
        "Alte ReBirth-Songs (.rbs) lassen sich direkt laden und werden mit ihrem kompletten Arrangement "
        "abgespielt – inklusive aller Pattern-Wechsel und Regler-Automationen.",
        "# Kurzanleitung",
        "- IMPORT .RBS lädt einen ReBirth-Song, SONG MODE spielt sein Arrangement synchron zur DAW.",
        "- Ein Klick in die Step-Raster bearbeitet Patterns; Instrumentennamen spielen den Klang an.",
        "- NOTES TO DAW in die DAW ziehen: der Song als echte Noten plus Regler-CCs, eine Spur je Gerät.",
        "- PATTERNS TO DAW: der Song als Pattern-Wechsel + CCs für ein x0x im Modus RACK.",
        "- MODE BASS 1 … DRUM 09: Single Mode – nur dieses Gerät klingt, gespielt von MIDI-Noten. "
        "Für jedes Gerät eine x0x-Instanz laden.",
        "- EJECT SONG entlädt das Arrangement, Patterns und Regler bleiben.",
        "# Hinweis",
        "Der Klang ist eine Nachbildung. Er folgt dem Web-Nachbau Re-Rebirth sehr genau, ist aber nicht "
        "zu 100 % identisch mit dem Original-ReBirth – und das war auch nie das Ziel.",
        "# Links",
        "Website: www.undabtanzbar.de",
        "GitHub: github.com/Yakobo1970",
        "# Entwicklung",
        "Idee, Konzept und Regie: Kay 'Yakobo' Fiedler.",
        "Umgesetzt mit Claude (Anthropic) als KI-Programmierassistent für C++/JUCE-Code, Klangvergleich und "
        "Fehlersuche – abgeglichen an Offline-Renderings der Web-App und an 155 echten ReBirth-Songs.",
        "# Unterstützen",
        "x0x ist kostenlos. Wer die Entwicklung unterstützen möchte, kann über den Knopf "
        "unten eine kleine Spende per PayPal schicken – vielen Dank!" }), true);

    tabs.addTab ("Herkunft", tabColour, new Page ({
        "# Propellerhead ReBirth RB-338",
        "ReBirth RB-338 erschien 1997 bei Propellerhead Software in Stockholm und war eines der ersten "
        "Programme, die klassische Hardware vollständig in Software nachbildeten: zwei Roland TB-303 "
        "Bass Line und eine TR-808 Rhythm Composer, ab Version 2.0 (1998) zusätzlich eine TR-909 – "
        "dazu Delay, Distortion, Compressor und der Pattern Controlled Filter.",
        "Das Programm prägte die Acid-, Techno- und Trance-Szene der späten Neunziger. Über "
        "\"Mods\" bekam es neue Oberflächen und Samples, und über ReWire lief es als Klangquelle in "
        "anderen Sequenzern. Ein Song (.rbs) speichert alle Patterns, Reglerstellungen und das komplette "
        "Arrangement samt Automation.",
        "2005 wurde ReBirth eingestellt und im \"ReBirth Museum\" kostenlos zum Herunterladen freigegeben; "
        "2010 folgte eine iOS-Version. Propellerhead firmiert heute als Reason Studios.",
        "# Re-Rebirth",
        "Re-Rebirth von Oleh Khomei (rb.varyform.info) ist eine liebevolle Hommage an ReBirth im Browser: "
        "eigene Klangmodelle für 303, 808 und 909 auf Basis der Web-Audio-Schnittstelle, die Effekte "
        "des Originals und ein Import für ReBirth-Songs. Der Quelltext steht unter der MIT-Lizenz.",
        "# x0x",
        "x0x überträgt Klangerzeugung, Sequencer, Effekte und den Song-Import von Re-Rebirth nach "
        "C++/JUCE, damit das Rack als VST3 in jeder DAW läuft. Neu dazu kamen das 19-Zoll-Rack-Design, "
        "sample-genaue Song-Automation, die Synchronisation mit dem DAW-Transport, der MIDI-Export für "
        "die DAW und der Single Mode.",
        "Der Name x0x steht für die ganze Gerätefamilie: TB-303, TR-808, TR-909.",
        "# Marken und Lizenzen",
        "~ ReBirth ist eine Marke von Reason Studios AB. Roland, TB-303, TR-808 und TR-909 sind Marken der "
        "Roland Corporation. x0x steht in keiner Verbindung zu diesen Firmen und wird von ihnen weder "
        "unterstützt noch geprüft.",
        "~ DSP auf Basis von Re-Rebirth, Copyright (c) 2026 Oleh Khomei, MIT-Lizenz (siehe "
        "THIRD_PARTY_NOTICES.md). Gebaut mit JUCE (juce.com). VST ist eine Marke der Steinberg Media "
        "Technologies GmbH." }), true);

    tabs.addTab ("MIDI & Modi", tabColour, new Page ({
        "# MODE: RACK",
        "Der eingebaute Sequencer spielt alle vier Geräte. Per MIDI (z. B. aus PATTERNS TO DAW):",
        "- Kanal 1 BASS 1, 2 BASS 2, 3 DRUM 08, 4 DRUM 09, 5 MIXER + FX.",
        "- Noten C1–G3 (36–67, C3 = 60) wählen Pattern A1–D8. Velocity ab 100 "
        "schaltet sofort um und startet das Pattern neu, darunter wechselt es am Pattern-Ende.",
        "- Jeder Regler hat eine feste CC-Nummer auf dem Kanal seines Geräts (Liste: MIDI_CC_MAP.md).",
        "~ Das Stummschalten einer DAW-Spur wirkt hier nicht auf den Klang, denn der Sequencer spielt selbst.",
        "# MODE: BASS 1 … DRUM 09 (Single Mode)",
        "Nur das gewählte Gerät klingt, gespielt von MIDI-Noten auf beliebigem Kanal. Sequencer und "
        "Song ruhen, die anderen Geräte sind abgeschaltet. CCs auf Kanal 5 steuern Mixer + FX, alle "
        "anderen Kanäle das Gerät.",
        "- 303: C1 (36) = Pattern-Note C, Oktave 0. Überlappende Noten gleiten (Slide), Velocity ab 100 = Accent.",
        "- Drums (GM): BD C1, RS C#1, SD D1, CP D#1, LT F1, CH F#1, MT A1, OH A#1, HT C2, CY/Crash C#2, "
        "Ride D#2, Cowbell G#2.",
        "- Drum-Velocity: unter 64 Flam-Vorschlag, 64–99 normal, 100–119 909-Einzelaccent "
        "(808: Accent), 120–126 Accent, 127 Accent + Einzelaccent.",
        "- CC 119 hält den Pattern-Filter im Raster des Original-Patterns (schreibt NOTES TO DAW automatisch).",
        "# In der DAW (z. B. Studio Pro)",
        "- NOTES TO DAW ins Arrangement ziehen – es entstehen fünf Spuren.",
        "- Auf die vier Gerätespuren je ein x0x laden und MODE passend wählen.",
        "- Die Spur MIXER + FX zusätzlich an die Instanzen schicken, deren Effekte mitlaufen sollen.",
        "- Die Noten lassen sich auch mit jedem anderen Synthesizer abspielen." }), true);

    addAndMakeVisible (tabs);
    for (auto* b : { &donate, &website, &github, &close })
        addAndMakeVisible (b);
    donate.setTooltip (kDonateUrl);
    website.setTooltip (kWebsiteUrl);
    github.setTooltip (kGithubUrl);
    donate.onClick = [] { juce::URL (kDonateUrl).launchInDefaultBrowser(); };
    website.onClick = [] { juce::URL (kWebsiteUrl).launchInDefaultBrowser(); };
    github.onClick = [] { juce::URL (kGithubUrl).launchInDefaultBrowser(); };
    close.onClick = [this] { setVisible (false); };
    donate.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff6e1416));
}

AboutOverlay::~AboutOverlay() = default;

juce::Rectangle<int> AboutOverlay::panelBounds() const
{
    return getLocalBounds().withSizeKeepingCentre (std::min (780, getWidth() - 40), std::min (700, getHeight() - 40));
}

void AboutOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.6f));
    const auto p = panelBounds().toFloat();
    g.setColour (juce::Colour (0xff141212));
    g.fillRoundedRectangle (p, 8.0f);
    g.setColour (juce::Colour (0xff6e1416));
    g.drawRoundedRectangle (p.reduced (1.0f), 8.0f, 2.0f);

    auto head = p.reduced (22.0f, 14.0f).removeFromTop (56.0f);
    {
        static const juce::Image logo = rb::icon::render (112);
        g.drawImage (logo, head.removeFromLeft (56.0f), juce::RectanglePlacement::centred);
        head.removeFromLeft (12.0f);
    }
    g.setFont (juce::Font (juce::FontOptions (40.0f, juce::Font::bold | juce::Font::italic)));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff0d0), 0, head.getY(), juce::Colour (0xffd8c8a8), 0,
                                             head.getBottom(), false));
    g.drawText ("x0x", head.removeFromLeft (90.0f), juce::Justification::centredLeft);
    g.setColour (kInk);
    g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    g.drawText (u8 ("Version " JucePlugin_VersionString " · Acid Bass & Rhythm Rack"), head.removeFromTop (28.0f),
                juce::Justification::bottomLeft);
    g.setColour (kMuted);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText (u8 ("Kay 'Yakobo' Fiedler · Un(d)AbtanzBAR · DSP based on Re-Rebirth by Oleh Khomei (MIT)"),
                head, juce::Justification::topLeft);
}

void AboutOverlay::resized()
{
    auto p = panelBounds().reduced (16);
    p.removeFromTop (70);
    auto buttons = p.removeFromBottom (32);
    p.removeFromBottom (10);
    tabs.setBounds (p);
    donate.setBounds (buttons.removeFromLeft (190));
    buttons.removeFromLeft (8);
    website.setBounds (buttons.removeFromLeft (170));
    buttons.removeFromLeft (8);
    github.setBounds (buttons.removeFromLeft (90));
    close.setBounds (buttons.removeFromRight (110));
}

void AboutOverlay::mouseDown (const juce::MouseEvent& e)
{
    if (! panelBounds().contains (e.getPosition()))
        setVisible (false);
}

} // namespace rb::ui
