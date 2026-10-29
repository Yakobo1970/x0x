#include "PluginEditor.h"
#include "dsp/PcfWaves.h"
#include "RackSkin.h"
#include "AboutOverlay.h"

using namespace rb;
using APVTS = juce::AudioProcessorValueTreeState;

namespace rb::ui
{
//==============================================================================
namespace col
{
    const juce::Colour bg (0xff111113);
    // Faceplate metal base colours
    const juce::Colour darkRed (0xff6e1416), blueMetal (0xff2a5ea8), orangeMetal (0xffc0601a);
    const juce::Colour greenMetal (0xff1d5532), goldMetal (0xffb08a2c);
    const juce::Colour bassAccent (0xffffa53a), a808 (0xffffd23f), a909 (0xffff7a1f);
    const juce::Colour delay (0xff4fc3ff), dist (0xffff6b4a), comp (0xff8ee05a), pcf (0xffffd84a);
    const juce::Colour ink (0xffe9e4d6), inkDark (0xff1d1d1d), inkMuted (0xff8d8a82);
    const juce::Colour led (0xffff3b2a), cellOff (0xff2a2322), lcd (0xff9fe870);
}

inline juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

//==============================================================================
class RackLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RackLookAndFeel()
    {
        setColour (juce::Slider::rotarySliderFillColourId, col::bassAccent);
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2e3035));
        setColour (juce::TextButton::buttonOnColourId, col::led);
        setColour (juce::TextButton::textColourOffId, col::ink.withAlpha (0.8f));
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff17181b));
        setColour (juce::ComboBox::textColourId, col::lcd);
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3b3d42));
        setColour (juce::ComboBox::arrowColourId, col::inkMuted);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1c1d20));
        setColour (juce::PopupMenu::textColourId, col::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::bassAccent.withAlpha (0.6f));
        setColour (juce::Slider::textBoxTextColourId, col::lcd);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::backgroundColourId, juce::Colour (0xff17181b));
        setColour (juce::Slider::trackColourId, col::lcd.withAlpha (0.25f));
        setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xee1c1d20));
        setColour (juce::BubbleComponent::outlineColourId, col::inkMuted);
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xee1c1d20));
        setColour (juce::TooltipWindow::textColourId, col::ink);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end,
                           juce::Slider& s) override
    {
        const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
        const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (1.5f);
        const float d = std::min (b.getWidth(), b.getHeight());
        const auto c = b.withSizeKeepingCentre (d, d);
        const float r = d * 0.5f;
        const float angle = start + pos * (end - start);

        // dark bezel so the value ring reads on every faceplate colour
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (c);

        // scale ring
        juce::Path arcBg, arcVal;
        arcBg.addCentredArc (c.getCentreX(), c.getCentreY(), r - 1, r - 1, 0, start, end, true);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.strokePath (arcBg, juce::PathStrokeType (2.0f));
        arcVal.addCentredArc (c.getCentreX(), c.getCentreY(), r - 1, r - 1, 0, start, angle, true);
        g.setColour (accent);
        g.strokePath (arcVal, juce::PathStrokeType (2.0f));

        // body
        const auto body = c.reduced (d * 0.16f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4d54), body.getX(), body.getY(),
                                                 juce::Colour (0xff15161a), body.getRight(), body.getBottom(), false));
        g.fillEllipse (body);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawEllipse (body, 1.0f);

        // pointer
        const float pr = body.getWidth() * 0.5f;
        const juce::Point<float> ctr = body.getCentre();
        const juce::Point<float> tip = ctr.getPointOnCircumference (pr * 0.9f, angle);
        const juce::Point<float> base = ctr.getPointOnCircumference (pr * 0.25f, angle);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawLine ({ base, tip }, std::max (1.5f, d * 0.07f));
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        if (style == juce::Slider::LinearBar)
        {
            g.setColour (s.findColour (juce::Slider::backgroundColourId));
            g.fillRoundedRectangle ((float) x, (float) y, (float) w, (float) h, 3.0f);
            g.setColour (s.findColour (juce::Slider::trackColourId));
            g.fillRoundedRectangle ((float) x, (float) y, pos - (float) x, (float) h, 3.0f);
            return;
        }
        // vertical mixer fader
        const auto track = juce::Rectangle<float> ((float) x + w * 0.5f - 2.0f, (float) y + 4, 4.0f, (float) h - 8);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.fillRoundedRectangle (track, 2.0f);
        for (int i = 0; i <= 10; ++i)
        {
            const float ty = (float) y + 6 + (h - 12) * i / 10.0f;
            g.setColour (col::inkMuted.withAlpha (0.35f));
            g.drawHorizontalLine ((int) ty, (float) x + 2, (float) x + w * 0.5f - 5);
        }
        const auto cap = juce::Rectangle<float> ((float) x + 2, pos - 7, (float) w - 4, 14.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8d8d8), cap.getX(), cap.getY(),
                                                 juce::Colour (0xff7a7a7a), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 2.0f);
        g.setColour (juce::Colours::black);
        g.drawHorizontalLine ((int) pos, cap.getX() + 2, cap.getRight() - 2);
        juce::ignoreUnused (s);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        auto base = b.findColour (juce::TextButton::buttonColourId);
        if (on)
            base = b.findColour (juce::TextButton::buttonOnColourId);
        if (over) base = base.brighter (0.12f);
        if (down) base = base.darker (0.2f);
        g.setColour (base);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        if (on)
        {
            g.setColour (base.withAlpha (0.25f));
            g.drawRoundedRectangle (r.expanded (1.0f), 4.0f, 2.0f);
        }
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return font (std::min (11.0f, h * 0.62f), true); }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        juce::Path arrow;
        const float ax = (float) w - 11, ay = h * 0.5f;
        arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (11.0f, true); }
    juce::Font getPopupMenuFont() override { return font (13.0f); }
    juce::Font getSliderPopupFont (juce::Slider&) override { return font (12.0f, true); }
    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (font (12.0f, true));
        return l;
    }
};

//==============================================================================
class Knob : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& id, const juce::String& text, juce::Colour accent, juce::Colour labelColour,
          bool withLabel = true)
        : label (text), labelCol (labelColour), showLabel (withLabel)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        slider.setPopupDisplayEnabled (true, false, nullptr);
        slider.setMouseDragSensitivity (180);
        if (auto* p = state.getParameter (id))
            slider.setTooltip (p->getName (64));
        addAndMakeVisible (slider);
        attachment = std::make_unique<APVTS::SliderAttachment> (state, id, slider);
        slider.setDoubleClickReturnValue (true, (double) state.getParameter (id)->convertFrom0to1 (
                                                    state.getParameter (id)->getDefaultValue()));
    }

    void resized() override
    {
        auto b = getLocalBounds();
        if (showLabel)
            b.removeFromBottom (12);
        slider.setBounds (b);
    }

    void paint (juce::Graphics& g) override
    {
        if (! showLabel)
            return;
        g.setColour (labelCol);
        g.setFont (font (8.5f, true));
        g.drawFittedText (label, getLocalBounds().removeFromBottom (12), juce::Justification::centred, 1, 0.7f);
    }

    juce::Slider slider;

private:
    std::unique_ptr<APVTS::SliderAttachment> attachment;
    juce::String label;
    juce::Colour labelCol;
    bool showLabel;
};

class Toggle : public juce::TextButton
{
public:
    Toggle (APVTS& state, const juce::String& id, const juce::String& text, juce::Colour onColour)
        : juce::TextButton (text)
    {
        setClickingTogglesState (true);
        setColour (juce::TextButton::buttonOnColourId, onColour);
        if (auto* p = state.getParameter (id))
            setTooltip (p->getName (64));
        attachment = std::make_unique<APVTS::ButtonAttachment> (state, id, *this);
    }

private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

class Choice : public juce::ComboBox
{
public:
    Choice (APVTS& state, const juce::String& id)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (id)))
        {
            addItemList (p->choices, 1);
            setTooltip (p->getName (64));
        }
        attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, id, *this);
        // onChange also fires for host/automation updates; only report picks made in the popup
        onChange = [this] {
            if (userPick && onUserChange)
                onUserChange();
            userPick = false;
        };
    }

    void showPopup() override
    {
        userPick = true;
        juce::ComboBox::showPopup();
    }

    std::function<void()> onUserChange;

private:
    bool userPick = false;
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

class Fader : public juce::Slider
{
public:
    Fader (APVTS& state, const juce::String& id)
    {
        setSliderStyle (juce::Slider::LinearVertical);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setPopupDisplayEnabled (true, false, nullptr);
        if (auto* p = state.getParameter (id))
            setTooltip (p->getName (64));
        attachment = std::make_unique<APVTS::SliderAttachment> (state, id, *this);
    }

private:
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// Generic custom-painted box (used for small live displays).
class PaintBox : public juce::Component
{
public:
    std::function<void (juce::Graphics&, juce::Rectangle<float>)> painter;
    void paint (juce::Graphics& g) override
    {
        if (painter)
            painter (g, getLocalBounds().toFloat());
    }
};

//==============================================================================
// Drag source: writes the song as a 5-track MIDI file and hands it to the DAW.
class MidiDragTile : public juce::Component, public juce::SettableTooltipClient
{
public:
    MidiDragTile (X0xProcessor& p, MidiExportKind k) : proc (p), kind (k)
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        if (kind == MidiExportKind::Notes)
            setTooltip ("Drag into the DAW: the notes the sequencer plays (303 notes with slides / accents, drums on GM "
                        "notes) plus the knob CCs, one track per device (ch 1-4) and ch 5 = MIXER + FX. Play them with "
                        "x0x instances in single mode (MODE: BASS 1 ... DRUM 09) or any other synth.");
        else
            setTooltip ("Drag into the DAW: 5 MIDI tracks (ch 1 BASS 1, ch 2 BASS 2, ch 3 DRUM 08, ch 4 DRUM 09, "
                        "ch 5 MIXER + FX) with pattern-change notes and knob CCs. Route all of them to one x0x in RACK "
                        "mode on their channel and switch SONG MODE off.");
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colours::black.withAlpha (hover ? 0.85f : 0.7f));
        g.fillRoundedRectangle (r, 4.0f);
        juce::Path outline;
        outline.addRoundedRectangle (r.reduced (1.5f), 3.0f);
        juce::Path dashed;
        const float dashes[] = { 4.0f, 3.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, outline, dashes, 2);
        g.setColour (hover ? col::lcd : col::lcd.withAlpha (0.7f));
        g.fillPath (dashed);

        // little "drag out" arrow
        auto icon = r.removeFromLeft (r.getHeight()).reduced (4.0f);
        juce::Path arrow;
        arrow.addArrow ({ icon.getX(), icon.getCentreY(), icon.getRight(), icon.getCentreY() }, 1.6f, 6.0f, 5.0f);
        g.fillPath (arrow);
        g.setFont (font (9.0f, true));
        g.drawText (busy ? "EXPORT..." : kind == MidiExportKind::Notes ? "NOTES TO DAW" : "PATTERNS TO DAW", r,
                    juce::Justification::centredLeft);
    }

    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 6)
            return;
        dragging = busy = true;
        repaint();
        const auto file = proc.exportSongMidi ({}, kind);
        busy = false;
        repaint();
        if (! file.existsAsFile())
        {
            dragging = false;
            return;
        }
        juce::Component::SafePointer<MidiDragTile> safe (this);
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this, [safe] {
            if (safe != nullptr)
                safe->dragging = false;
        });
    }

private:
    X0xProcessor& proc;
    MidiExportKind kind;
    bool hover = false, dragging = false, busy = false;
};

// Covers a device in single mode: the whole panel when it is switched off, or
// just its pattern section when it is the device played by MIDI notes.
class ModeShade : public juce::Component
{
public:
    ModeShade (bool full, juce::String text, juce::String hint = {})
        : wholeDevice (full), label (std::move (text)), help (std::move (hint))
    {
        setVisible (false);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (wholeDevice ? 0.72f : 0.84f));
        g.fillRoundedRectangle (r, 4.0f);
        if (! wholeDevice)
        {
            // hatched like a section without steps (LEN 0)
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 12.0f)
                g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 4.0f);
        }
        g.setColour (wholeDevice ? col::inkMuted : col::lcd);
        g.setFont (font (wholeDevice ? 16.0f : 13.0f, true));
        auto text = r.reduced (10.0f);
        if (help.isEmpty())
        {
            g.drawText (label, text, juce::Justification::centred);
            return;
        }
        g.drawText (label, text.removeFromTop (text.getHeight() * 0.5f), juce::Justification::centredBottom);
        g.setColour (col::ink.withAlpha (0.75f));
        g.setFont (font (9.5f, true));
        g.drawFittedText (help, text.toNearestInt().withTrimmedTop (6), juce::Justification::centredTop, 3);
    }

private:
    bool wholeDevice;
    juce::String label, help;
};

//==============================================================================
class Meter : public juce::Component
{
public:
    Meter (Engine& e, int index) : engine (e), idx (index) {}

    void tick()
    {
        const float v = engine.meter[idx].load();
        shown = std::max (v, shown * 0.82f);
        peakHold = std::max (shown, peakHold - 0.004f);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (b, 2.0f);
        auto norm = [] (float v) { return juce::jlimit (0.0f, 1.0f, (20.0f * std::log10 (std::max (v, 1e-6f)) + 48.0f) / 51.0f); };
        const int segs = 24;
        const float segH = (b.getHeight() - 4) / segs;
        const int lit = (int) std::round (norm (shown) * segs);
        for (int i = 0; i < segs; ++i)
        {
            const float y = b.getBottom() - 2 - (i + 1) * segH;
            const auto c = i >= segs - 2 ? juce::Colour (0xffff3b2a) : i >= segs - 6 ? juce::Colour (0xffffc12a) : juce::Colour (0xff6fe04a);
            g.setColour (i < lit ? c : c.withAlpha (0.12f));
            g.fillRect (b.getX() + 2, y + 1, b.getWidth() - 4, segH - 1.5f);
        }
        const int ph = (int) std::round (norm (peakHold) * segs);
        if (ph > 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.fillRect (b.getX() + 2, b.getBottom() - 2 - ph * segH + 1, b.getWidth() - 4, 1.5f);
        }
    }

private:
    Engine& engine;
    int idx;
    float shown = 0.0f, peakHold = 0.0f;
};

//==============================================================================
struct Clipboard
{
    bool hasBass = false, hasDrum = false;
    int bassLen = 16, drumLen = 16;
    bool bassShuf = false, drumShuf = false;
    std::array<uint16_t, kSteps> bass {};
    std::array<std::array<uint8_t, kSteps>, kDrumLanes> drum {};
};
static Clipboard clipboard;

class PatternPanel : public juce::Component
{
public:
    PatternPanel (X0xProcessor& p, int device, juce::Colour accentColour, juce::Colour inkColour, juce::Colour bgColour)
        : proc (p), dev (device), accent (accentColour), ink (inkColour), bgc (bgColour) {}

    int current() const { return proc.engine->currentPattern[dev].load(); }
    int queued() const { return proc.engine->queuedPattern[dev].load(); }

    void selectPattern (int idx)
    {
        auto* prm = proc.param (juce::String (kDeviceIds[dev]) + ".pattern");
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) idx + 1.0f));
        prm->endChangeGesture();
        proc.engine->requestPattern (dev, idx);
        if (! proc.engine->isPlaying)
            proc.engine->currentPattern[dev] = idx;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const int cur = current(), q = queued();
        const bool blink = (juce::Time::getMillisecondCounter() / 250) % 2 == 0;
        for (int i = 0; i < 12; ++i)
        {
            auto r = cellRect (i).toFloat();
            bool lit, pending;
            juce::String text;
            if (i < 4)
            {
                lit = cur / 8 == i;
                pending = q != cur && q / 8 == i;
                text = juce::String::charToString ((juce::juce_wchar) ('A' + i));
            }
            else
            {
                lit = cur % 8 == i - 4;
                pending = q != cur && q % 8 == i - 4;
                text = juce::String (i - 3);
            }
            g.setColour (lit ? accent : col::cellOff);
            g.fillRoundedRectangle (r.reduced (1), 3);
            if (pending && blink)
            {
                g.setColour (accent.brighter (0.4f));
                g.drawRoundedRectangle (r.reduced (1), 3, 2);
            }
            g.setColour (lit ? juce::Colours::white : col::ink.withAlpha (0.7f));
            g.setFont (font (11, true));
            g.drawText (text, r, juce::Justification::centred);
        }

        // Second row: length / shuffle / clear / copy / paste
        const bool shuf = proc.patterns.shuffle (dev, cur);
        const int len = proc.patterns.length (dev, cur);
        const char* labels[] = { "-", nullptr, "+", "SHUF", "CLR", "COPY", "PASTE" };
        for (int i = 0; i < 7; ++i)
        {
            auto r = toolRect (i).toFloat();
            if (labels[i] == nullptr)
            {
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.fillRoundedRectangle (r.reduced (1), 3);
                g.setColour (col::lcd);
                g.setFont (font (12, true));
                g.drawText ("LEN " + juce::String (len), r, juce::Justification::centred);
                continue;
            }
            const bool on = i == 3 && shuf;
            g.setColour (on ? accent : col::cellOff);
            g.fillRoundedRectangle (r.reduced (1), 3);
            g.setColour (on ? juce::Colours::white : col::ink.withAlpha (0.8f));
            g.setFont (font (9.5f, true));
            g.drawText (labels[i], r, juce::Justification::centred);
        }

        g.setColour (ink.withAlpha (0.75f));
        g.setFont (font (9.5f, true));
        juce::String status = "PATTERN " + name (cur);
        if (q != cur)
            status << "   NEXT " << name (q);
        g.drawText (status, getLocalBounds().removeFromBottom (14).toFloat(), juce::Justification::centredLeft);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int cur = current();
        for (int i = 0; i < 12; ++i)
            if (cellRect (i).contains (e.getPosition()))
            {
                const int base = queued();
                selectPattern (i < 4 ? i * 8 + base % 8 : (base / 8) * 8 + (i - 4));
                return;
            }
        for (int i = 0; i < 7; ++i)
            if (toolRect (i).contains (e.getPosition()))
            {
                tool (i, cur);
                repaint();
                return;
            }
    }

private:
    static juce::String name (int idx) { return juce::String::charToString ((juce::juce_wchar) ('A' + idx / 8)) + juce::String (idx % 8 + 1); }

    juce::Rectangle<int> cellRect (int i) const
    {
        const float w = (getWidth() - 6.0f) / 12.0f;
        return juce::Rectangle<float> (i * w + (i >= 4 ? 6.0f : 0.0f), 0, w, 24).toNearestInt();
    }

    juce::Rectangle<int> toolRect (int i) const
    {
        static const float widths[] = { 0.08f, 0.2f, 0.08f, 0.15f, 0.13f, 0.17f, 0.19f };
        float x = 0;
        for (int k = 0; k < i; ++k)
            x += widths[k] * getWidth();
        return juce::Rectangle<float> (x, 28, widths[i] * getWidth(), 22).toNearestInt();
    }

    void tool (int i, int cur)
    {
        auto& bank = proc.patterns;
        const bool bass = isBass (dev);
        auto setLen = [&] (int delta) {
            if (bass) { auto& p = bank.bassPattern (dev, cur); p.length = juce::jlimit (1, kSteps, p.length + delta); }
            else      { auto& p = bank.drumPattern (dev, cur); p.length = juce::jlimit (1, kSteps, p.length + delta); }
        };
        switch (i)
        {
            case 0: setLen (-1); break;
            case 2: setLen (+1); break;
            case 3:
                if (bass) bank.bassPattern (dev, cur).shuffle = ! bank.bassPattern (dev, cur).shuffle;
                else      bank.drumPattern (dev, cur).shuffle = ! bank.drumPattern (dev, cur).shuffle;
                break;
            case 4:
                if (bass) bank.bassPattern (dev, cur).clear();
                else      bank.drumPattern (dev, cur).clear();
                break;
            case 5:
                if (bass)
                {
                    auto& p = bank.bassPattern (dev, cur);
                    clipboard.hasBass = true;
                    clipboard.bassLen = p.length;
                    clipboard.bassShuf = p.shuffle;
                    for (int s = 0; s < kSteps; ++s) clipboard.bass[(size_t) s] = p.steps[(size_t) s].load();
                }
                else
                {
                    auto& p = bank.drumPattern (dev, cur);
                    clipboard.hasDrum = true;
                    clipboard.drumLen = p.length;
                    clipboard.drumShuf = p.shuffle;
                    for (int l = 0; l < kDrumLanes; ++l)
                        for (int s = 0; s < kSteps; ++s) clipboard.drum[(size_t) l][(size_t) s] = p.get (l, s);
                }
                break;
            case 6:
                if (bass && clipboard.hasBass)
                {
                    auto& p = bank.bassPattern (dev, cur);
                    p.length = clipboard.bassLen;
                    p.shuffle = clipboard.bassShuf;
                    for (int s = 0; s < kSteps; ++s) p.steps[(size_t) s] = clipboard.bass[(size_t) s];
                }
                else if (! bass && clipboard.hasDrum)
                {
                    auto& p = bank.drumPattern (dev, cur);
                    p.length = clipboard.drumLen;
                    p.shuffle = clipboard.drumShuf;
                    for (int l = 0; l < kDrumLanes; ++l)
                        for (int s = 0; s < kSteps; ++s) p.set (l, s, clipboard.drum[(size_t) l][(size_t) s]);
                }
                break;
            default: break;
        }
    }

    X0xProcessor& proc;
    int dev;
    juce::Colour accent, ink, bgc;
};

//==============================================================================
class BassGrid : public juce::Component
{
public:
    BassGrid (X0xProcessor& p, int device) : proc (p), dev (device) {}

    static constexpr int kLabelW = 44;
    static constexpr int kRows = 7;
    static constexpr int rowY[kRows + 1] = { 0, 16, 50, 70, 90, 110, 130, 150 };

    BassPattern& pat() { return proc.patterns.bassPattern (dev, proc.engine->currentPattern[dev].load()); }

    void paint (juce::Graphics& g) override
    {
        auto& p = pat();
        const int len = p.length;
        const int playing = proc.engine->uiStep[dev].load();
        static const char* rowNames[] = { "STEP", "NOTE", "DOWN", "UP", "ACC", "SLIDE", "GATE" };
        static const char* notes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C'" };

        g.setFont (font (8.5f, true));
        for (int r = 0; r < kRows; ++r)
        {
            g.setColour (col::ink.withAlpha (0.85f));
            g.drawText (rowNames[r], 0, rowY[r], kLabelW - 4, rowY[r + 1] - rowY[r] - 2, juce::Justification::centredRight);
        }

        for (int s = 0; s < kSteps; ++s)
        {
            const auto st = p.step (s);
            const bool active = s < len;
            const float x = (float) (kLabelW + s * cellW());
            const float w = (float) cellW() - 2.0f;
            const float dim = active ? 1.0f : 0.3f;

            // LED + number
            const bool lit = s == playing;
            g.setColour ((lit ? col::led : juce::Colour (0xff4a1510)).withMultipliedAlpha (dim));
            g.fillEllipse (x + w * 0.5f - 4, 1, 8, 8);
            g.setColour (col::ink.withAlpha (0.8f).withMultipliedAlpha (dim));
            g.setFont (font (7.5f));
            g.drawText (juce::String (s + 1), juce::Rectangle<float> (x, 8, w, 8), juce::Justification::centred);

            // Note cell
            auto nr = juce::Rectangle<float> (x, (float) rowY[1], w, (float) (rowY[2] - rowY[1] - 3));
            const bool sep = (s % 4) == 0;
            g.setColour ((st.gate ? juce::Colour (0xff17181b) : juce::Colour (0xff222327)).withMultipliedAlpha (dim));
            g.fillRoundedRectangle (nr, 3);
            if (sep)
            {
                g.setColour (col::bassAccent.withAlpha (0.5f * dim));
                g.fillRect (x, (float) rowY[1] - 2, w, 1.5f);
            }
            juce::String txt = ! st.gate ? juce::String ("-") : st.tie ? juce::String ("TIE") : juce::String (notes[juce::jlimit (0, 12, st.note)]);
            g.setColour ((st.gate ? (st.tie ? col::ink.withAlpha (0.6f) : col::lcd) : col::inkMuted).withMultipliedAlpha (dim));
            g.setFont (font (st.tie ? 10.0f : 14.0f, true));
            g.drawText (txt, nr, juce::Justification::centred);
            if (lit)
            {
                g.setColour (col::led.withAlpha (0.8f));
                g.drawRoundedRectangle (nr, 3, 1.5f);
            }

            // Flag rows
            const bool on[] = { st.octave < 0, st.octave > 0, st.accent, st.slide };
            for (int r = 0; r < 4; ++r)
            {
                auto fr = juce::Rectangle<float> (x + 3, (float) rowY[r + 2] + 2, w - 6, (float) (rowY[r + 3] - rowY[r + 2] - 6));
                g.setColour ((on[r] ? col::bassAccent : col::cellOff).withMultipliedAlpha (dim));
                g.fillRoundedRectangle (fr, 2);
            }

            // Gate row
            auto gr = juce::Rectangle<float> (x + 1, (float) rowY[6] + 1, w - 2, (float) (rowY[7] - rowY[6] - 4));
            g.setColour ((st.gate ? (st.tie ? juce::Colour (0xff7a6a3a) : juce::Colour (0xff5d6a2b)) : col::cellOff).withMultipliedAlpha (dim));
            g.fillRoundedRectangle (gr, 2);
            g.setColour (col::ink.withMultipliedAlpha (dim));
            g.setFont (font (8.0f, true));
            g.drawText (! st.gate ? "REST" : st.tie ? "TIE" : "NOTE", gr, juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int s = stepAt (e.x), r = rowAt (e.y);
        if (s < 0 || r < 0)
            return;
        auto& p = pat();
        auto st = p.step (s);
        dragStep = -1;
        switch (r)
        {
            case 1:
                if (e.mods.isPopupMenu())
                {
                    showNoteMenu (s);
                    return;
                }
                if (! st.gate || st.tie) { st.gate = true; st.tie = false; }
                dragStep = s;
                dragStartY = e.y;
                dragStartNote = st.note;
                break;
            case 2: st.octave = st.octave < 0 ? 0 : -1; break;
            case 3: st.octave = st.octave > 0 ? 0 : 1; break;
            case 4: st.accent = ! st.accent; break;
            case 5: st.slide = ! st.slide; break;
            case 6:
                if (! st.gate) { st.gate = true; st.tie = false; }
                else if (! st.tie) st.tie = true;
                else { st.gate = false; st.tie = false; }
                break;
            default: return;
        }
        p.setStep (s, st);
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragStep < 0)
            return;
        auto& p = pat();
        auto st = p.step (dragStep);
        st.note = juce::jlimit (0, 12, dragStartNote + (dragStartY - e.y) / 7);
        p.setStep (dragStep, st);
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int s = stepAt (e.x);
        if (s < 0 || rowAt (e.y) != 1)
            return;
        auto& p = pat();
        auto st = p.step (s);
        st.gate = false;
        st.tie = false;
        p.setStep (s, st);
        repaint();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int s = stepAt (e.x);
        if (s < 0 || rowAt (e.y) != 1)
            return;
        auto& p = pat();
        auto st = p.step (s);
        st.note = juce::jlimit (0, 12, st.note + (w.deltaY > 0 ? 1 : -1));
        st.gate = true;
        p.setStep (s, st);
        repaint();
    }

private:
    int cellW() const { return (getWidth() - kLabelW) / kSteps; }
    int stepAt (int x) const { const int s = (x - kLabelW) / cellW(); return x < kLabelW || s >= kSteps ? -1 : s; }
    int rowAt (int y) const
    {
        for (int r = 0; r < kRows; ++r)
            if (y >= rowY[r] && y < rowY[r + 1]) return r;
        return -1;
    }

    void showNoteMenu (int s)
    {
        static const char* notes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C'" };
        juce::PopupMenu m;
        for (int n = 12; n >= 0; --n)
            m.addItem (n + 1, notes[n]);
        m.addSeparator();
        m.addItem (100, "Rest");
        m.addItem (101, "Tie");
        juce::Component::SafePointer<BassGrid> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, s] (int res) {
            if (safe == nullptr || res == 0)
                return;
            auto& p = safe->pat();
            auto st = p.step (s);
            if (res == 100) { st.gate = false; st.tie = false; }
            else if (res == 101) { st.gate = true; st.tie = true; }
            else { st.gate = true; st.tie = false; st.note = res - 1; }
            p.setStep (s, st);
            safe->repaint();
        });
    }

    X0xProcessor& proc;
    int dev;
    int dragStep = -1, dragStartY = 0, dragStartNote = 0;
};

//==============================================================================
class DrumGrid : public juce::Component
{
public:
    DrumGrid (X0xProcessor& p, int device, juce::Colour accentColour, juce::Colour inkColour)
        : proc (p), dev (device), accent (accentColour), ink (inkColour) {}

    static constexpr int kLabelW = 96, kCellX = 200, kCellW = 24, kRowH = 18;

    DrumPattern& pat() { return proc.patterns.drumPattern (dev, proc.engine->currentPattern[dev].load()); }

    void paint (juce::Graphics& g) override
    {
        auto& p = pat();
        const int len = p.length;
        const int playing = proc.engine->uiStep[dev].load();
        const auto names = drumLaneNames (dev);
        for (int lane = 0; lane < kDrumLanes; ++lane)
        {
            const int y = lane * kRowH;
            g.setColour (ink.withAlpha (lane == 0 ? 0.6f : 0.9f));
            g.setFont (font (9.0f, true));
            g.drawText (names[lane], 4, y, kLabelW - 6, kRowH, juce::Justification::centredLeft);
            if (lane % 2 == 1)
            {
                g.setColour (juce::Colours::black.withAlpha (0.06f));
                g.fillRect (0, y, getWidth(), kRowH);
            }
            for (int s = 0; s < kSteps; ++s)
            {
                const auto v = p.get (lane, s);
                auto r = juce::Rectangle<float> ((float) (kCellX + s * kCellW) + 1, (float) y + 2, kCellW - 3.0f, kRowH - 4.0f);
                const float dim = s < len ? 1.0f : 0.3f;
                juce::Colour c = v == Off ? col::cellOff
                               : v == Accent ? accent.brighter (0.35f) : v == Flam ? juce::Colour (0xff6fb6ff) : accent;
                if (v == Off && (s / 4) % 2 == 1)
                    c = c.darker (0.15f);
                g.setColour (c.withMultipliedAlpha (dim));
                g.fillRoundedRectangle (r, 2.5f);
                if (v == Accent || v == Flam)
                {
                    g.setColour (juce::Colours::white);
                    g.setFont (font (8.0f, true));
                    g.drawText (v == Accent ? "A" : "F", r, juce::Justification::centred);
                }
                if (s == playing)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.55f));
                    g.drawRoundedRectangle (r, 2.5f, 1.2f);
                }
            }
        }
        // step LEDs under the grid header row
        for (int s = 0; s < kSteps; ++s)
            if (s == playing)
            {
                g.setColour (col::led);
                g.fillRect ((float) (kCellX + s * kCellW) + 1, (float) (kDrumLanes * kRowH) + 1, kCellW - 3.0f, 2.5f);
            }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int lane = e.y / kRowH;
        if (lane < 0 || lane >= kDrumLanes)
            return;
        if (e.x < kLabelW)
        {
            proc.engine->audition (dev, lane);
            return;
        }
        const int s = stepAt (e.x);
        if (s < 0)
            return;
        auto& p = pat();
        const auto v = p.get (lane, s);
        if (dev == R909 && (e.mods.isPopupMenu() || e.mods.isShiftDown()))
        {
            p.set (lane, s, (uint8_t) ((v + 1) % 4));
            paintValue = -1;
        }
        else
        {
            paintValue = v == Off ? On : Off;
            p.set (lane, s, (uint8_t) paintValue);
        }
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (paintValue < 0)
            return;
        const int lane = e.y / kRowH, s = stepAt (e.x);
        if (lane < 0 || lane >= kDrumLanes || s < 0)
            return;
        pat().set (lane, s, (uint8_t) paintValue);
        repaint();
    }

private:
    int stepAt (int x) const { const int s = (x - kCellX) / kCellW; return x < kCellX || s >= kSteps ? -1 : s; }

    X0xProcessor& proc;
    int dev;
    juce::Colour accent, ink;
    int paintValue = -1;
};

//==============================================================================
class RackContent : public juce::Component
{
public:
    explicit RackContent (X0xProcessor& p) : proc (p), state (p.apvts)
    {
        buildTopBar();
        buildBass (Bass1, { 8, 60, 850, 186 });
        buildBass (Bass2, { 8, 252, 850, 186 });
        buildDrums (R808, { 8, 444, 850, 252 });
        buildDrums (R909, { 8, 702, 850, 252 });
        buildMixer ({ 866, 60, 406, 404 });
        buildFx();
        buildShades();

        // Controls were laid out on a 1280 px grid; move them between the rack rails.
        for (auto* c : getChildren())
            c->setTopLeftPosition (mapX (c->getX()), c->getY());

        // the logo opens the About panel
        logoButton.setBounds (40, 4, 250, 46);
        logoButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        logoButton.setTooltip (juce::String::fromUTF8 ("Über x0x"));
        logoButton.onClick = [this] { showAbout(); };
        addAndMakeVisible (logoButton);
        addChildComponent (about);
    }

    void showAbout()
    {
        about.setBounds (getLocalBounds());
        about.setVisible (true);
        about.toFront (true);
    }

    // Rack geometry (content is kWidth x 960)
    static constexpr int kWidth = 1350;
    static int mapX (int x) { return x < 862 ? x + 22 : x + 48; }

    void paint (juce::Graphics& g) override
    {
        namespace sk = skin;
        const auto& tex = sk::Textures::get (kWidth, getHeight());
        g.fillAll (juce::Colour (0xff09090a));

        // rails: left, centre (below the top unit) and right
        sk::drawRail (g, { 0, 0, 30, (float) getHeight() });
        sk::drawRail (g, { 880, 56, 34, (float) getHeight() - 56 });
        sk::drawRail (g, { 1320, 0, 30, (float) getHeight() });

        // top unit spans the whole rack
        sk::drawFace (g, tex, { 12, 3, 1326, 50 }, col::darkRed, 18, 18, 7);
        g.setFont (juce::Font (juce::FontOptions (34.0f, juce::Font::bold | juce::Font::italic)));
        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.drawText ("x0x", 46, 6, 120, 38, juce::Justification::centredLeft);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff0d0), 0, 8, juce::Colour (0xffd8c8a8), 0, 42, false));
        g.drawText ("x0x", 44, 4, 120, 38, juce::Justification::centredLeft);
        g.setColour (col::ink.withAlpha (0.85f));
        g.setFont (font (8.5f, true));
        g.drawText ("ACID BASS & RHYTHM RACK", 112, 22, 200, 12, juce::Justification::centredLeft);
        g.setColour (col::ink.withAlpha (0.5f));
        g.setFont (font (7.5f, true));
        g.drawText ("DSP based on Re-Rebirth by Oleh Khomei (MIT)", 112, 34, 220, 10, juce::Justification::centredLeft);

        int seed = 100;
        for (auto& pn : panels)
        {
            const bool left = pn.r.getX() < 862;
            auto r = pn.r.withX (mapX (pn.r.getX())).toFloat();
            const float earL = left ? 18.0f : 17.0f, earR = left ? 17.0f : 18.0f;
            auto face = r.withLeft (r.getX() - earL).withRight (r.getRight() + earR);
            sk::drawFace (g, tex, face, pn.colour, earL, earR, seed++);

            // black silkscreen title strip
            auto header = r.removeFromTop (22.0f).reduced (2.0f, 2.0f);
            g.setColour (juce::Colours::black.withAlpha (0.62f));
            g.fillRoundedRectangle (header, 3.0f);
            g.setColour (pn.accent);
            g.fillRect (header.getX() + 5, header.getY() + 4, 4.0f, header.getHeight() - 8);
            g.setColour (col::ink);
            g.setFont (font (13.0f, true));
            g.drawText (pn.title, header.withTrimmedLeft (14), juce::Justification::centredLeft);
            if (pn.subtitle.isNotEmpty())
            {
                g.setColour (col::ink.withAlpha (0.55f));
                g.setFont (font (9.0f, true));
                g.drawText (pn.subtitle, header.withTrimmedLeft (14 + juce::GlyphArrangement::getStringWidth (font (13.0f, true), pn.title) + 10),
                            juce::Justification::centredLeft);
            }
        }
    }

    void tick()
    {
        for (auto* c : dynamic)
            c->repaint();
        for (auto* m : meters)
            m->tick();
        const bool hostPlaying = proc.engine->isPlaying && ! proc.engine->internalPlay;
        playButton->setToggleState (proc.engine->isPlaying.load(), juce::dontSendNotification);
        playButton->setButtonText (hostPlaying ? "HOST" : proc.engine->internalPlay ? "STOP" : "PLAY");
        updateShades();
    }

    void updateShades()
    {
        const int single = juce::roundToInt (proc.param ("play.mode")->getValue() * (float) NumDevices) - 1;
        for (int d = 0; d < NumDevices; ++d)
        {
            offShade[d]->setVisible (single >= 0 && single != d);
            for (auto* s : midiShade[d])
                s->setVisible (single == d);
        }
    }

private:
    struct Panel { juce::Rectangle<int> r; juce::String title, subtitle; juce::Colour colour, accent; };

    template <typename T, typename... Args>
    T* add (juce::Rectangle<int> b, Args&&... args)
    {
        auto* c = new T (std::forward<Args> (args)...);
        owned.add (c);
        addAndMakeVisible (c);
        c->setBounds (b);
        return c;
    }

    Knob* knob (juce::Rectangle<int> b, const juce::String& id, const juce::String& label, juce::Colour accent,
                juce::Colour ink = col::ink, bool showLabel = true)
    {
        return add<Knob> (b, state, id, label, accent, ink, showLabel);
    }

    Toggle* toggle (juce::Rectangle<int> b, const juce::String& id, const juce::String& text, juce::Colour on)
    {
        return add<Toggle> (b, state, id, text, on);
    }

    PaintBox* paintBox (juce::Rectangle<int> b, std::function<void (juce::Graphics&, juce::Rectangle<float>)> fn)
    {
        auto* pb = add<PaintBox> (b);
        pb->painter = std::move (fn);
        pb->setInterceptsMouseClicks (false, false);
        dynamic.push_back (pb);
        return pb;
    }

    void buildTopBar()
    {
        playButton = add<juce::TextButton> (juce::Rectangle<int> { 290, 12, 76, 30 }, "PLAY");
        playButton->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff3dbb4c));
        playButton->setTooltip ("Internal play (when the host transport is stopped). The sequencer follows the host transport automatically.");
        playButton->onClick = [this] { proc.engine->internalPlay = ! proc.engine->internalPlay.load(); };

        paintBox ({ 376, 10, 150, 34 }, [this] (juce::Graphics& g, juce::Rectangle<float> r) {
            g.setColour (juce::Colours::black);
            g.fillRoundedRectangle (r, 4);
            g.setColour (col::led);
            g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
            g.drawText (juce::String (proc.engine->tempo.load(), 1), r.withTrimmedRight (40), juce::Justification::centredRight);
            g.setColour (col::inkMuted);
            g.setFont (font (8.5f, true));
            g.drawText ("BPM", r.removeFromRight (36), juce::Justification::centredLeft);
        });

        auto* tempo = add<juce::Slider> (juce::Rectangle<int> { 534, 15, 96, 24 });
        tempo->setSliderStyle (juce::Slider::LinearBar);
        tempo->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 96, 24);
        tempo->setTooltip ("Internal tempo, used only when the host provides no tempo");
        tempoAttachment = std::make_unique<APVTS::SliderAttachment> (state, "tempo", *tempo);
        tempo->setTextValueSuffix (" INT");

        knob ({ 646, 4, 44, 48 }, "song.shuffle", "SHUFFLE", col::bassAccent);
        knob ({ 696, 4, 44, 48 }, "master.volume", "VOLUME", col::bassAccent);

        auto* importButton = add<juce::TextButton> (juce::Rectangle<int> { 752, 8, 96, 18 }, "IMPORT .RBS");
        importButton->setTooltip ("Load a ReBirth RB-338 2.0 song (.rbs): patterns, knobs, mixer, effects and song arrangement");
        importButton->onClick = [this] { chooseRbs(); };
        toggle ({ 752, 30, 96, 16 }, "song.mode", "SONG MODE", juce::Colour (0xff3dbb4c))
            ->setTooltip ("Song mode plays the imported arrangement (pattern changes + automation) following the host position. "
                          "Off = pattern mode.");

        add<MidiDragTile> (juce::Rectangle<int> { 1162, 4, 110, 15 }, proc, MidiExportKind::Notes);
        add<MidiDragTile> (juce::Rectangle<int> { 1162, 21, 110, 15 }, proc, MidiExportKind::Patterns);
        auto* saveMidi = add<juce::TextButton> (juce::Rectangle<int> { 1162, 38, 110, 14 }, "SAVE .MID");
        saveMidi->setTooltip ("Save one of the two MIDI files to disk");
        saveMidi->onClick = [this] { saveMidiMenu(); };

        auto* mode = add<Choice> (juce::Rectangle<int> { 858, 38, 150, 14 }, state, "play.mode");
        mode->setTooltip ("RACK: the sequencer plays all four devices. BASS 1 ... DRUM 09: single mode, only that device "
                          "sounds and it is played by MIDI notes from the DAW (load one x0x per device).");
        auto* eject = add<juce::TextButton> (juce::Rectangle<int> { 1014, 38, 90, 14 }, "EJECT SONG");
        eject->setTooltip ("Unload the imported song arrangement (patterns and knobs stay)");
        eject->onClick = [this] { proc.ejectSong(); repaint(); };

        paintBox ({ 858, 4, 296, 31 }, [this] (juce::Graphics& g, juce::Rectangle<float> r) {
            g.setColour (juce::Colours::black);
            g.fillRoundedRectangle (r, 4);
            auto song = proc.engine->getSong();
            auto inner = r.reduced (8, 2);
            g.setFont (font (10.0f, true));
            if (song == nullptr || song->empty())
            {
                g.setColour (col::inkMuted);
                g.drawText (proc.param ("play.mode")->getValue() > 0.1f ? "SINGLE MODE  -  MIDI notes play the device, CCs = knobs"
                                                                  : "NO SONG  -  MIDI: notes C1..G3 = patterns, CCs = knobs",
                            inner, juce::Justification::centredLeft);
                return;
            }
            const int bars = std::max (1, song->length / 32);
            const int tick = proc.engine->uiSongTick.load();
            g.setColour (col::lcd);
            juce::String head = proc.songTitle.toUpperCase();
            if (proc.songMod.isNotEmpty())
                head << "  (MOD " << proc.songMod.toUpperCase() << ": STANDARD SOUNDS)";
            g.drawText (head, inner.removeFromTop (13), juce::Justification::centredLeft);
            juce::String line = "RBS TEMPO " + juce::String (proc.songTempo, 1) + "  -  " + juce::String (bars) + " BARS";
            if (song->loop)
                line << "  -  LOOP " << song->loopStart / 32 + 1 << "-" << song->loopEnd / 32;
            if (tick >= 0)
                line << "  -  BAR " << tick / 32 + 1;
            g.setColour (col::inkMuted);
            g.drawText (line, inner, juce::Justification::centredLeft);
        });
    }

    void buildBass (int dev, juce::Rectangle<int> r)
    {
        const juce::String id (kDeviceIds[dev]);
        const auto metal = dev == Bass1 ? col::blueMetal : col::orangeMetal;
        panels.push_back ({ r, kDeviceLabels[dev], "ANALOG BASS SYNTHESIZER", metal, col::bassAccent });
        toggle ({ r.getX() + 286, r.getY() + 3, 40, 16 }, id + ".on", "ON", col::led);
        toggle ({ r.getX() + 330, r.getY() + 3, 64, 16 }, id + ".waveform", "SQUARE", col::bassAccent);

        static const std::pair<const char*, const char*> knobs[] = { { "tuning", "TUNING" }, { "cutoff", "CUTOFF" },
                                                                     { "resonance", "RESO" }, { "envmod", "ENV MOD" },
                                                                     { "decay", "DECAY" }, { "accent", "ACCENT" },
                                                                     { "volume", "VOLUME" } };
        for (int k = 0; k < 7; ++k)
            knob ({ r.getX() + 10 + k * 46, r.getY() + 28, 44, 58 }, id + "." + knobs[k].first, knobs[k].second, col::bassAccent);

        auto* pp = add<PatternPanel> (juce::Rectangle<int> { r.getX() + 12, r.getY() + 98, 318, 70 }, proc, dev, col::bassAccent,
                                      col::ink, metal);
        dynamic.push_back (pp);
        auto* grid = add<BassGrid> (juce::Rectangle<int> { r.getX() + 336, r.getY() + 28, 44 + 16 * 29, 152 }, proc, dev);
        dynamic.push_back (grid);
    }

    void buildDrums (int dev, juce::Rectangle<int> r)
    {
        const bool is808 = dev == R808;
        const juce::String id (kDeviceIds[dev]);
        const auto accent = is808 ? col::a808 : col::a909;
        const auto ink = is808 ? col::ink : col::inkDark; // gold faceplate needs dark print
        panels.push_back ({ r, kDeviceLabels[dev], is808 ? "ANALOG RHYTHM MACHINE" : "HYBRID RHYTHM MACHINE",
                            is808 ? col::greenMetal : col::goldMetal, accent });
        toggle ({ r.getX() + 300, r.getY() + 3, 40, 16 }, id + ".on", "ON", col::led);

        auto* grid = add<DrumGrid> (juce::Rectangle<int> { r.getX() + 8, r.getY() + 28, DrumGrid::kCellX + 16 * DrumGrid::kCellW, 12 * DrumGrid::kRowH + 4 },
                                    proc, dev, accent, ink);
        dynamic.push_back (grid);

        // Per-lane knobs, laid out on the grid rows (lane order = drumLanes(dev)).
        struct LaneKnobs { std::vector<std::pair<const char*, const char*>> knobs; const char* alt; };
        const std::vector<LaneKnobs> lanes808 = {
            { { { "ac.level", "LEVEL" } }, nullptr },
            { { { "bd.level", "LEVEL" }, { "bd.tone", "TONE" }, { "bd.decay", "DECAY" } }, nullptr },
            { { { "sd.level", "LEVEL" }, { "sd.tone", "TONE" }, { "sd.snappy", "SNAPPY" } }, nullptr },
            { { { "lt.level", "LEVEL" }, { "lt.tuning", "TUNING" } }, "lt.alt" },
            { { { "mt.level", "LEVEL" }, { "mt.tuning", "TUNING" } }, "mt.alt" },
            { { { "ht.level", "LEVEL" }, { "ht.tuning", "TUNING" } }, "ht.alt" },
            { { { "rs.level", "LEVEL" } }, "rs.alt" },
            { { { "cp.level", "LEVEL" } }, "cp.alt" },
            { { { "cb.level", "LEVEL" } }, nullptr },
            { { { "cy.level", "LEVEL" }, { "cy.tone", "TONE" }, { "cy.decay", "DECAY" } }, nullptr },
            { { { "oh.level", "LEVEL" }, { "oh.decay", "DECAY" } }, nullptr },
            { { { "ch.level", "LEVEL" } }, nullptr } };
        const std::vector<LaneKnobs> lanes909 = {
            { { { "ac.level", "LEVEL" }, { "ac.flam", "FLAM" } }, nullptr },
            { { { "bd.tune", "TUNE" }, { "bd.level", "LEVEL" }, { "bd.attack", "ATTACK" }, { "bd.decay", "DECAY" } }, nullptr },
            { { { "sd.tune", "TUNE" }, { "sd.tone", "TONE" }, { "sd.snappy", "SNAPPY" }, { "sd.level", "LEVEL" } }, nullptr },
            { { { "lt.tune", "TUNE" }, { "lt.decay", "DECAY" }, { "lt.level", "LEVEL" } }, nullptr },
            { { { "mt.tune", "TUNE" }, { "mt.decay", "DECAY" }, { "mt.level", "LEVEL" } }, nullptr },
            { { { "ht.tune", "TUNE" }, { "ht.decay", "DECAY" }, { "ht.level", "LEVEL" } }, nullptr },
            { { { "rs.level", "LEVEL" } }, nullptr },
            { { { "cp.level", "LEVEL" } }, nullptr },
            { { { "hh.level", "LEVEL" }, { "hh.chDecay", "CH DEC" } }, nullptr },
            { { { "hh.ohDecay", "OH DEC" } }, nullptr },
            { { { "cy.crLevel", "LEVEL" }, { "cy.crTune", "TUNE" } }, nullptr },
            { { { "cy.rdLevel", "LEVEL" }, { "cy.rdTune", "TUNE" } }, nullptr } };
        const auto& lanes = is808 ? lanes808 : lanes909;
        for (int lane = 0; lane < kDrumLanes; ++lane)
        {
            const int y = r.getY() + 28 + lane * DrumGrid::kRowH;
            int x = r.getX() + 8 + DrumGrid::kLabelW;
            for (auto& [k, label] : lanes[(size_t) lane].knobs)
            {
                auto* kn = knob ({ x, y, 20, 18 }, id + "." + k, label, accent, ink, false);
                kn->slider.setMouseDragSensitivity (140);
                x += 22;
            }
            if (lanes[(size_t) lane].alt != nullptr)
                toggle ({ x + 2, y + 2, 30, 14 }, id + "." + lanes[(size_t) lane].alt, "ALT", accent);
        }

        auto* pp = add<PatternPanel> (juce::Rectangle<int> { r.getX() + 612, r.getY() + 30, 228, 70 }, proc, dev, accent, ink,
                                      is808 ? col::greenMetal : col::goldMetal);
        dynamic.push_back (pp);

        paintBox ({ r.getX() + 612, r.getY() + 108, 228, 120 }, [ink, is808] (juce::Graphics& g, juce::Rectangle<float> b) {
            g.setColour (ink.withAlpha (0.65f));
            g.setFont (font (9.0f));
            juce::String help = "Click a cell to set / clear, drag to paint.\nClick an instrument name to preview it.\n"
                                "Hover a knob for its name, double-click to reset.";
            if (! is808)
                help << "\nRight- or shift-click cycles ON > ACCENT > FLAM.";
            g.drawFittedText (help, b.toNearestInt(), juce::Justification::topLeft, 8);
        });
    }

    void buildMixer (juce::Rectangle<int> r)
    {
        panels.push_back ({ r, "MIXER", "", col::goldMetal, juce::Colour (0xff9aa0a8) });
        const int stripW = 78;
        for (int d = 0; d <= NumDevices; ++d)
        {
            const int x = r.getX() + 8 + d * stripW;
            const int y = r.getY() + 26;
            const bool master = d == NumDevices;
            const juce::String mid = master ? juce::String() : "mixer." + juce::String (kDeviceIds[d]) + ".";
            paintBox ({ x, y, stripW - 6, 14 }, [d, master] (juce::Graphics& g, juce::Rectangle<float> b) {
                g.setColour (col::inkDark);
                g.setFont (font (9.5f, true));
                g.drawText (master ? "MASTER" : kDeviceLabels[d], b, juce::Justification::centred);
            });
            if (! master)
            {
                toggle ({ x + 10, y + 18, stripW - 26, 16 }, mid + "dist", "DIST", col::dist);
                knob ({ x + 14, y + 38, 44, 50 }, mid + "pan", "PAN", juce::Colour (0xffe8e8e8), col::inkDark);
                knob ({ x + 14, y + 90, 44, 50 }, mid + "delay", "DELAY", col::delay, col::inkDark);
                add<Fader> (juce::Rectangle<int> { x + 10, y + 146, 30, 178 }, state, mid + "level");
                toggle ({ x + 6, y + 330, 32, 18 }, mid + "mute", "M", juce::Colour (0xffd9a12a));
                toggle ({ x + 40, y + 330, 32, 18 }, mid + "solo", "S", juce::Colour (0xff3dbb4c));
            }
            else
            {
                knob ({ x + 14, y + 90, 44, 50 }, "mixer.delayReturn", "DLY RET", col::delay, col::inkDark);
                add<Fader> (juce::Rectangle<int> { x + 10, y + 146, 30, 178 }, state, "mixer.master.level");
            }
            auto* m = add<Meter> (juce::Rectangle<int> { x + 44, y + 150, 12, 170 }, *proc.engine, d);
            meters.push_back (m);
        }
    }

    void buildFx()
    {
        const int x = 866, w = 406;
        // Delay
        {
            juce::Rectangle<int> r (x, 470, w, 104);
            panels.push_back ({ r, "DELAY", "TEMPO SYNCED", col::blueMetal, col::delay });
            toggle ({ r.getRight() - 50, r.getY() + 3, 42, 16 }, "fx.delay.on", "ON", col::delay);
            knob ({ x + 16, r.getY() + 28, 56, 66 }, "fx.delay.steps", "STEPS", col::delay);
            knob ({ x + 84, r.getY() + 28, 56, 66 }, "fx.delay.feedback", "FEEDBACK", col::delay);
            knob ({ x + 152, r.getY() + 28, 56, 66 }, "fx.delay.pan", "PAN", col::delay);
            toggle ({ x + 222, r.getY() + 50, 70, 20 }, "fx.delay.triplet", "TRIPLET", col::delay);
            paintBox ({ x + 300, r.getY() + 34, 96, 50 }, [this] (juce::Graphics& g, juce::Rectangle<float> b) {
                const int steps = (int) std::lround (proc.param ("fx.delay.steps")->getValue() * 31.0f) + 1;
                g.setColour (juce::Colours::black);
                g.fillRoundedRectangle (b, 4);
                g.setColour (col::lcd);
                g.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
                g.drawText (juce::String (steps), b.withTrimmedBottom (14), juce::Justification::centred);
                g.setFont (font (8.5f, true));
                g.drawText ("STEPS", b.removeFromBottom (14), juce::Justification::centred);
            });
        }
        // Distortion
        {
            juce::Rectangle<int> r (x, 580, w, 84);
            panels.push_back ({ r, "DISTORTION", "INSERT PER CHANNEL (MIXER DIST)", col::darkRed, col::dist });
            toggle ({ r.getRight() - 50, r.getY() + 3, 42, 16 }, "fx.dist.on", "ON", col::dist);
            knob ({ x + 16, r.getY() + 26, 50, 54 }, "fx.dist.amount", "AMOUNT", col::dist);
            knob ({ x + 80, r.getY() + 26, 50, 54 }, "fx.dist.shape", "SHAPE", col::dist);
            paintBox ({ x + 150, r.getY() + 30, 240, 44 }, [] (juce::Graphics& g, juce::Rectangle<float> b) {
                g.setColour (col::ink.withAlpha (0.85f));
                g.setFont (font (9.0f));
                g.drawFittedText ("SHAPE at minimum = hard clip,\nabove = wave folder.", b.toNearestInt(), juce::Justification::centredLeft, 3);
            });
        }
        // Compressor
        {
            juce::Rectangle<int> r (x, 670, w, 112);
            panels.push_back ({ r, "COMPRESSOR", "", col::orangeMetal, col::comp });
            // Songs can leave the compressor "unrouted"; touching ON or TARGET routes it again.
            auto routeComp = [this] {
                auto* p = proc.param ("fx.comp.routed");
                if (p->getValue() < 0.5f)
                {
                    p->beginChangeGesture();
                    p->setValueNotifyingHost (1.0f);
                    p->endChangeGesture();
                }
            };
            auto* compOn = toggle ({ r.getRight() - 50, r.getY() + 3, 42, 16 }, "fx.comp.on", "ON", col::comp);
            compOn->onClick = [compOn, routeComp] { if (compOn->getToggleState()) routeComp(); };
            knob ({ x + 16, r.getY() + 28, 56, 66 }, "fx.comp.amount", "AMOUNT", col::comp);
            knob ({ x + 84, r.getY() + 28, 56, 66 }, "fx.comp.threshold", "THRESHOLD", col::comp);
            auto* compTarget = add<Choice> (juce::Rectangle<int> { x + 156, r.getY() + 34, 104, 22 }, state, "fx.comp.target");
            compTarget->onUserChange = routeComp;
            paintBox ({ x + 156, r.getY() + 60, 104, 14 }, [] (juce::Graphics& g, juce::Rectangle<float> b) {
                g.setColour (col::ink.withAlpha (0.85f));
                g.setFont (font (8.5f, true));
                g.drawText ("TARGET", b, juce::Justification::centredLeft);
            });
            paintBox ({ x + 272, r.getY() + 32, 120, 62 }, [this] (juce::Graphics& g, juce::Rectangle<float> b) {
                g.setColour (juce::Colours::black);
                g.fillRoundedRectangle (b, 4);
                const float red = -proc.engine->compReduction.load();
                g.setColour (col::inkMuted);
                g.setFont (font (8.5f, true));
                g.drawText ("GAIN REDUCTION", b.removeFromTop (16), juce::Justification::centred);
                auto bar = b.reduced (8, 6).removeFromTop (12);
                g.setColour (col::comp.withAlpha (0.15f));
                g.fillRect (bar);
                g.setColour (col::comp);
                g.fillRect (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, red / 24.0f)));
                g.setColour (col::lcd);
                g.setFont (font (12.0f, true));
                g.drawText (juce::String (red > 0.05f ? -red : 0.0f, 1) + " dB", b.removeFromBottom (20), juce::Justification::centred);
            });
        }
        // Pattern controlled filter
        {
            juce::Rectangle<int> r (x, 788, w, 166);
            panels.push_back ({ r, "PATTERN FILTER", "PCF", col::greenMetal, col::pcf });
            toggle ({ r.getRight() - 50, r.getY() + 3, 42, 16 }, "fx.pcf.on", "ON", col::pcf);
            const char* ids[] = { "fx.pcf.freq", "fx.pcf.reso", "fx.pcf.amount", "fx.pcf.decay", "fx.pcf.wave" };
            const char* labels[] = { "FREQ", "RESO", "AMOUNT", "DECAY", "WAVE" };
            for (int k = 0; k < 5; ++k)
                knob ({ x + 12 + k * 52, r.getY() + 28, 50, 60 }, ids[k], labels[k], col::pcf);
            add<Choice> (juce::Rectangle<int> { x + 280, r.getY() + 30, 54, 22 }, state, "fx.pcf.mode");
            add<Choice> (juce::Rectangle<int> { x + 338, r.getY() + 30, 60, 22 }, state, "fx.pcf.target");
            paintBox ({ x + 280, r.getY() + 54, 118, 12 }, [] (juce::Graphics& g, juce::Rectangle<float> b) {
                g.setColour (col::ink.withAlpha (0.85f));
                g.setFont (font (8.0f, true));
                g.drawText ("MODE      TARGET", b, juce::Justification::centredLeft);
            });
            paintBox ({ x + 12, r.getY() + 94, w - 24, 64 }, [this] (juce::Graphics& g, juce::Rectangle<float> b) {
                g.setColour (juce::Colours::black);
                g.fillRoundedRectangle (b, 4);
                const int wave = (int) std::lround (proc.param ("fx.pcf.wave")->getValue() * (kNumPcfWaves - 1));
                const int target = (int) std::lround (proc.param ("fx.pcf.target")->getValue() * 4.0f);
                const int step = target > 0 ? proc.engine->uiStep[target - 1].load() : -1;
                auto info = b.removeFromRight (90);
                auto bars = b.reduced (6, 6);
                const float bw = bars.getWidth() / 32.0f;
                for (int i = 0; i < 32; ++i)
                {
                    const float v = kPcfWaves[wave][i];
                    const bool cur = step >= 0 && step % 16 == i / 2;
                    if (v == kNoTrig)
                    {
                        g.setColour (col::pcf.withAlpha (0.12f));
                        g.fillRect (bars.getX() + i * bw + 1, bars.getBottom() - 2, bw - 2, 2.0f);
                        continue;
                    }
                    const float hgt = std::max (2.0f, v * bars.getHeight());
                    g.setColour (cur ? juce::Colours::white : col::pcf);
                    g.fillRect (bars.getX() + i * bw + 1, bars.getBottom() - hgt, bw - 2, hgt);
                }
                g.setColour (col::lcd);
                g.setFont (font (11.0f, true));
                g.drawText ("WAVE " + juce::String (wave + 1), info.removeFromTop (30), juce::Justification::centred);
                const float hz = proc.engine->pcfHz.load();
                g.drawText (hz > 0 ? juce::String (juce::roundToInt (hz)) + " Hz" : juce::String ("OFF"), info, juce::Justification::centred);
            });
        }
    }

    X0xProcessor& proc;
    APVTS& state;
    juce::OwnedArray<juce::Component> owned;
    std::vector<Panel> panels;
    std::vector<juce::Component*> dynamic;
    std::vector<Meter*> meters;
    juce::TextButton* playButton = nullptr;
    std::unique_ptr<APVTS::SliderAttachment> tempoAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    struct HotSpot : juce::Button // invisible click area
    {
        HotSpot() : juce::Button ("about") {}
        void paintButton (juce::Graphics&, bool, bool) override {}
    } logoButton;
    AboutOverlay about;
    ModeShade* offShade[NumDevices] {};
    std::vector<ModeShade*> midiShade[NumDevices];

    void buildShades()
    {
        const juce::Rectangle<int> areas[NumDevices] = { { 8, 60, 850, 186 }, { 8, 252, 850, 186 }, { 8, 444, 850, 252 },
                                                         { 8, 702, 850, 252 } };
        for (int d = 0; d < NumDevices; ++d)
        {
            const auto r = areas[d];
            offShade[d] = add<ModeShade> (r, true, juce::String (kDeviceLabels[d]) + "  -  OFF (SINGLE MODE)");
            if (isBass (d))
            {
                const juce::String hint = "C1 (MIDI 36) = pattern note C  -  overlapping notes slide  -  velocity 100+ = accent";
                midiShade[d].push_back (add<ModeShade> (juce::Rectangle<int> { r.getX() + 336, r.getY() + 28, 44 + 16 * 29, 152 },
                                                        false, "PLAYED BY MIDI NOTES", hint));
                midiShade[d].push_back (add<ModeShade> (juce::Rectangle<int> { r.getX() + 12, r.getY() + 98, 318, 70 }, false,
                                                        "NO PATTERNS"));
            }
            else
            {
                const juce::String hint = "GM drum notes: BD C1  RS C#1  SD D1  CP D#1  LT F1  CH F#1  MT A1  OH A#1  HT C2  "
                                          + juce::String (d == R808 ? "CY C#2  CB G#2" : "CR C#2  RD D#2")
                                          + "\nvelocity 120+ = accent" + (d == R909 ? ", 100-119 / 127 = voice accent" : "");
                midiShade[d].push_back (add<ModeShade> (juce::Rectangle<int> { r.getX() + 8 + DrumGrid::kCellX, r.getY() + 28,
                                                                               16 * DrumGrid::kCellW, 12 * DrumGrid::kRowH + 4 },
                                                        false, "PLAYED BY MIDI NOTES", hint));
                midiShade[d].push_back (add<ModeShade> (juce::Rectangle<int> { r.getX() + 612, r.getY() + 30, 228, 70 }, false,
                                                        "NO PATTERNS"));
            }
        }
        updateShades();
    }

    void saveMidiMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, "Notes (for x0x single mode or other synths)");
        m.addItem (2, "Patterns (for x0x in RACK mode)");
        juce::Component::SafePointer<RackContent> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe] (int r) {
            if (safe != nullptr && r > 0)
                safe->saveMidiFile (r == 1 ? MidiExportKind::Notes : MidiExportKind::Patterns);
        });
    }

    void saveMidiFile (MidiExportKind kind)
    {
        const auto name = juce::File::createLegalFileName (proc.songTitle.upToFirstOccurrenceOf ("  -  ", false, false)).trim();
        chooser = std::make_unique<juce::FileChooser> ("Song als MIDI speichern",
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                                                           .getChildFile ((name.isEmpty() ? juce::String ("x0x") : name)
                                                                          + (kind == MidiExportKind::Notes ? " (notes).mid" : " (patterns).mid")),
                                                       "*.mid");
        juce::Component::SafePointer<RackContent> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safe, kind] (const juce::FileChooser& fc) {
                                  if (safe == nullptr || fc.getResult() == juce::File())
                                      return;
                                  if (safe->proc.exportSongMidi (fc.getResult().withFileExtension (".mid"), kind) == juce::File())
                                      juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                              "MIDI-Export", "Die Datei konnte nicht geschrieben werden.");
                              });
    }

    void chooseRbs()
    {
        chooser = std::make_unique<juce::FileChooser> ("ReBirth-Song importieren", juce::File(), "*.rbs;*.RBS");
        juce::Component::SafePointer<RackContent> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe] (const juce::FileChooser& fc) {
                                  if (safe == nullptr || fc.getResult() == juce::File())
                                      return;
                                  const auto err = safe->proc.importRbs (fc.getResult());
                                  if (err.isNotEmpty())
                                      juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                              "RBS-Import fehlgeschlagen", err);
                                  safe->repaint();
                              });
    }
};

} // namespace rb::ui

//==============================================================================
X0xEditor::X0xEditor (X0xProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    lnf = std::make_unique<rb::ui::RackLookAndFeel>();
    setLookAndFeel (lnf.get());
    content = std::make_unique<rb::ui::RackContent> (p);
    content->setSize (rb::ui::RackContent::kWidth, kBaseH);
    addAndMakeVisible (*content);

    setResizable (true, true);
    setResizeLimits (kBaseW / 2, kBaseH / 2, kBaseW * 2, kBaseH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) kBaseW / kBaseH);
    setSize ((int) (kBaseW * 0.85), (int) (kBaseH * 0.85));
    startTimerHz (30);
}

X0xEditor::~X0xEditor()
{
    stopTimer();
    content.reset();
    setLookAndFeel (nullptr);
}

void X0xEditor::paint (juce::Graphics& g)
{
    g.fillAll (rb::ui::col::bg);
}

void X0xEditor::resized()
{
    const float scale = (float) getWidth() / (float) kBaseW;
    content->setTransform (juce::AffineTransform::scale (scale));
}

void X0xEditor::timerCallback()
{
    static_cast<rb::ui::RackContent*> (content.get())->tick();
}
