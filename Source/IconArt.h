// The x0x application icon: a dark red rack faceplate with rusty screws and
// "x0x" whose 0 is an orange-ringed 303 knob. Drawn per size (small sizes simplified).
#pragma once

#include "RackSkin.h"

namespace rb::icon
{
inline void draw (juce::Graphics& g, float s)
{
    const auto cream = juce::Colour (0xfffff0d0), creamDark = juce::Colour (0xffd2bf9a);
    const auto orange = juce::Colour (0xffffa53a);
    const bool tiny = s < 32.0f, small = s < 64.0f;

    // faceplate
    const auto plate = juce::Rectangle<float> (0, 0, s, s).reduced (s * (tiny ? 0.02f : 0.04f));
    const float radius = s * 0.18f;
    juce::ColourGradient metal (juce::Colour (0xffa23033), 0, plate.getY(), juce::Colour (0xff3a090b), 0, plate.getBottom(), false);
    metal.addColour (0.45, juce::Colour (0xff701719));
    g.setGradientFill (metal);
    g.fillRoundedRectangle (plate, radius);

    if (! small)
    {
        // brushed metal + glare
        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (plate, radius);
        g.reduceClipRegion (clip);
        juce::Random rnd (338);
        for (float y = plate.getY(); y < plate.getBottom(); y += std::max (1.0f, s / 256.0f))
        {
            g.setColour ((rnd.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.03f + rnd.nextFloat() * 0.05f));
            g.fillRect (plate.getX(), y, plate.getWidth(), std::max (0.5f, s / 512.0f));
        }
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.22f), plate.getX(), plate.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), plate.getCentreX(), plate.getCentreY(), true));
        g.fillEllipse (plate.getX() - s * 0.3f, plate.getY() - s * 0.45f, s * 1.1f, s * 0.9f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (plate.reduced (s * 0.006f), radius, std::max (1.0f, s * 0.018f));
    if (! tiny)
    {
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawRoundedRectangle (plate.reduced (s * 0.03f), radius * 0.85f, std::max (0.6f, s * 0.006f));
    }

    // rusty screws
    if (! small)
    {
        const float in = s * 0.14f, r = s * 0.052f;
        int seed = 7;
        for (auto p : { juce::Point<float> (in, in), { s - in, in }, { in, s - in }, { s - in, s - in } })
            rb::ui::skin::drawScrew (g, p, r, seed++);
    }

    // the 303 knob as the "0"
    const juce::Point<float> c (s * 0.5f, s * (tiny ? 0.5f : 0.47f));
    const float kr = s * (tiny ? 0.36f : small ? 0.15f : 0.15f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (c.x - kr * 1.18f, c.y - kr * 1.18f + s * 0.01f, kr * 2.36f, kr * 2.36f);
    juce::Path ring;
    ring.addCentredArc (c.x, c.y, kr, kr, 0.0f, 0.0f, juce::MathConstants<float>::twoPi, true);
    g.setColour (orange);
    g.strokePath (ring, juce::PathStrokeType (kr * (tiny ? 0.3f : 0.24f)));
    const float br = kr * (tiny ? 0.62f : 0.72f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5a5d66), c.x - br, c.y - br, juce::Colour (0xff121316), c.x + br, c.y + br, false));
    g.fillEllipse (c.x - br, c.y - br, br * 2, br * 2);
    const float ang = juce::MathConstants<float>::pi * 0.25f;
    g.setColour (juce::Colours::white.withAlpha (0.92f));
    g.drawLine ({ c.getPointOnCircumference (br * 0.2f, ang), c.getPointOnCircumference (br * 0.92f, ang) },
                std::max (1.0f, br * 0.22f));
    if (tiny)
        return;

    // the two x
    const float xs = s * (small ? 0.1f : 0.085f); // half size of each x
    const float stroke = s * (small ? 0.07f : 0.052f);
    const float dx = s * (small ? 0.31f : 0.3f);
    for (float cx : { c.x - dx, c.x + dx })
    {
        juce::Path x;
        const float sl = xs * 0.22f; // italic slant
        x.startNewSubPath (cx - xs + sl, c.y - xs);
        x.lineTo (cx + xs - sl, c.y + xs);
        x.startNewSubPath (cx + xs + sl, c.y - xs);
        x.lineTo (cx - xs - sl, c.y + xs);
        const juce::PathStrokeType st (stroke, juce::PathStrokeType::mitered, juce::PathStrokeType::butt);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.strokePath (x, st, juce::AffineTransform::translation (s * 0.012f, s * 0.016f));
        g.setGradientFill (juce::ColourGradient (cream, 0, c.y - xs, creamDark, 0, c.y + xs, false));
        g.strokePath (x, st);
    }

    // device LEDs: BASS 1, BASS 2, DRUM 08, DRUM 09
    const juce::Colour leds[] = { juce::Colour (0xff4f8fe8), juce::Colour (0xffff8a2a), juce::Colour (0xff3dbb4c), juce::Colour (0xffe8c24a) };
    const float lw = s * 0.085f, lh = s * 0.035f, gap = s * 0.035f;
    float lx = s * 0.5f - (4 * lw + 3 * gap) * 0.5f;
    const float ly = s * (small ? 0.8f : 0.79f);
    for (auto col : leds)
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (lx - s * 0.006f, ly - s * 0.006f, lw + s * 0.012f, lh + s * 0.012f, lh * 0.5f);
        g.setColour (col);
        g.fillRoundedRectangle (lx, ly, lw, lh, lh * 0.4f);
        lx += lw + gap;
    }
}

inline juce::Image render (int size)
{
    juce::Image img (juce::Image::ARGB, size, size, true);
    juce::Graphics g (img);
    draw (g, (float) size);
    return img;
}
} // namespace rb::icon
