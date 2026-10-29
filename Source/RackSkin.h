// Procedural 19" rack skin: brushed red-metallic faceplates with rust,
// rusty Phillips screws in the rack ears and punched rack rails.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace rb::ui::skin
{
// --- noise -------------------------------------------------------------------
inline float hash (int x, int y, int seed)
{
    uint32_t h = (uint32_t) x * 374761393u + (uint32_t) y * 668265263u + (uint32_t) seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float) ((h ^ (h >> 16)) & 0xffffff) / (float) 0xffffff;
}

inline float valueNoise (float x, float y, int seed)
{
    const int ix = (int) std::floor (x), iy = (int) std::floor (y);
    float fx = x - (float) ix, fy = y - (float) iy;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    const float a = hash (ix, iy, seed), b = hash (ix + 1, iy, seed);
    const float c = hash (ix, iy + 1, seed), d = hash (ix + 1, iy + 1, seed);
    return juce::jmap (fy, juce::jmap (fx, a, b), juce::jmap (fx, c, d));
}

inline float fbm (float x, float y, int seed, int octaves = 5)
{
    float sum = 0, amp = 0.5f, norm = 0;
    for (int o = 0; o < octaves; ++o)
    {
        sum += amp * valueNoise (x, y, seed + o * 17);
        norm += amp;
        x *= 2.03f;
        y *= 2.03f;
        amp *= 0.5f;
    }
    return sum / norm;
}

inline float smoothstep (float a, float b, float x)
{
    const float t = juce::jlimit (0.0f, 1.0f, (x - a) / (b - a));
    return t * t * (3 - 2 * t);
}

// --- textures ------------------------------------------------------------------
struct Textures
{
    juce::Image brush, rust; // brush: neutral light/dark streaks laid over any metal colour
    float scale = 2.0f;

    Textures (int w, int h)
    {
        const int W = (int) (w * scale), H = (int) (h * scale);
        brush = juce::Image (juce::Image::ARGB, W, H, true);
        rust = juce::Image (juce::Image::ARGB, W, H, true);
        juce::Image::BitmapData bb (brush, juce::Image::BitmapData::writeOnly);
        juce::Image::BitmapData rb (rust, juce::Image::BitmapData::writeOnly);

        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const float px = x / scale, py = y / scale;

                // brushed metal: long horizontal streaks + fine grain
                const float streak = valueNoise (px * 0.004f, py * 0.9f, 3) * 0.6f + valueNoise (px * 0.03f, py * 2.7f, 5) * 0.4f;
                const float v = (streak - 0.5f) * 2.0f + (hash (x, y, 9) - 0.5f) * 0.35f;
                const float alpha = juce::jmin (0.2f, std::abs (v) * 0.2f);
                bb.setPixelColour (x, y, (v > 0 ? juce::Colours::white : juce::Colours::black).withAlpha (alpha));

                // rust: dense, irregular corrosion (only ever drawn around screws)
                const float cloud = fbm (px * 0.05f, py * 0.05f, 21, 4);
                const float detail = fbm (px * 0.22f, py * 0.22f, 33, 3);
                const float a = smoothstep (0.3f, 0.6f, cloud * 0.7f + detail * 0.45f);
                if (a <= 0.01f)
                    continue;
                const float tone = valueNoise (px * 0.15f, py * 0.15f, 57);
                const auto c1 = juce::Colour (0xff3d1c0c), c2 = juce::Colour (0xff8a3e14), c3 = juce::Colour (0xffc0672a);
                auto c = tone < 0.5f ? c1.interpolatedWith (c2, tone * 2) : c2.interpolatedWith (c3, (tone - 0.5f) * 2);
                c = c.withMultipliedBrightness (0.75f + 0.5f * detail);
                rb.setPixelColour (x, y, c.withAlpha (juce::jlimit (0.0f, 1.0f, a)));
            }
    }

    static const Textures& get (int w, int h)
    {
        static const Textures t (w, h);
        return t;
    }
};

// --- drawing helpers -----------------------------------------------------------
inline void drawScrew (juce::Graphics& g, juce::Point<float> c, float r, int seed)
{
    juce::Random rnd (seed);
    // countersink shadow
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (c.x - r - 1.5f, c.y - r - 0.5f, (r + 1.5f) * 2, (r + 1.5f) * 2);

    // rust run-off below the screw
    const float drip = 6.0f + rnd.nextFloat() * 14.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xaa7a3412), c.x, c.y, juce::Colour (0x007a3412), c.x, c.y + r + drip, false));
    g.fillEllipse (c.x - r * 0.55f, c.y, r * 1.1f, r + drip);

    // steel head
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd9d6cf), c.x - r * 0.5f, c.y - r * 0.6f,
                                             juce::Colour (0xff57534c), c.x + r * 0.6f, c.y + r * 0.8f, true));
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);

    // rust blotches on the head
    for (int i = 0; i < 7; ++i)
    {
        const float a = rnd.nextFloat() * juce::MathConstants<float>::twoPi;
        const float d = rnd.nextFloat() * r * 0.75f;
        const float s = r * (0.25f + rnd.nextFloat() * 0.45f);
        const auto col = juce::Colour (0xff6e2f0f).interpolatedWith (juce::Colour (0xffb5581d), rnd.nextFloat());
        g.setColour (col.withAlpha (0.35f + rnd.nextFloat() * 0.45f));
        g.fillEllipse (c.x + std::cos (a) * d - s * 0.5f, c.y + std::sin (a) * d - s * 0.5f, s, s);
    }

    // Phillips cross
    const float ang = rnd.nextFloat() * juce::MathConstants<float>::pi;
    juce::Path cross;
    cross.addRectangle (-r * 0.62f, -r * 0.13f, r * 1.24f, r * 0.26f);
    cross.addRectangle (-r * 0.13f, -r * 0.62f, r * 0.26f, r * 1.24f);
    cross.applyTransform (juce::AffineTransform::rotation (ang).translated (c.x, c.y));
    g.setColour (juce::Colour (0xff1a120c));
    g.fillPath (cross);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.strokePath (cross, juce::PathStrokeType (0.6f), juce::AffineTransform::translation (0.5f, 0.5f));

    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 0.8f);
}

inline void drawRail (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4b4f), r.getX(), 0, juce::Colour (0xff1c1d20), r.getRight(), 0, false));
    g.fillRect (r);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawVerticalLine ((int) r.getX() + 1, r.getY(), r.getBottom());
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawVerticalLine ((int) r.getRight() - 1, r.getY(), r.getBottom());

    // square cage-nut holes, 3 per rack unit
    const float u = 44.0f, hole = 7.0f;
    const float cx = r.getCentreX();
    for (float y = r.getY() + 4.0f; y < r.getBottom() - hole; y += u)
        for (float off : { 0.0f, 15.9f, 31.8f })
        {
            const float hy = y + off;
            if (hy + hole > r.getBottom())
                break;
            g.setColour (juce::Colour (0xff060607));
            g.fillRect (cx - hole * 0.5f, hy, hole, hole);
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.drawHorizontalLine ((int) (hy + hole), cx - hole * 0.5f, cx + hole * 0.5f);
        }
}

// Irregular rust stain around a screw: noisy blob outlines drawn with decreasing opacity.
inline void drawRustHalo (juce::Graphics& g, const Textures& tex, juce::Point<float> c, float r, int seed)
{
    const auto full = juce::Rectangle<float> (0, 0, tex.rust.getWidth() / tex.scale, tex.rust.getHeight() / tex.scale);
    const std::pair<float, float> rings[] = { { r * 3.4f, 0.3f }, { r * 2.5f, 0.55f }, { r * 1.8f, 0.9f } };
    int k = 0;
    for (auto [rad, op] : rings)
    {
        juce::Path blob;
        const int n = 28;
        for (int i = 0; i < n; ++i)
        {
            const float ang = juce::MathConstants<float>::twoPi * i / n;
            // stains run down a little further than up
            const float rr = rad * (0.7f + 0.6f * valueNoise (i * 0.45f, (float) k, seed)) * (std::sin (ang) > 0 ? 1.25f : 1.0f);
            const juce::Point<float> p (c.x + std::cos (ang) * rr, c.y + std::sin (ang) * rr);
            if (i == 0) blob.startNewSubPath (p); else blob.lineTo (p);
        }
        blob.closeSubPath();
        blob = blob.createPathWithRoundedCorners (rad * 0.4f);
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (blob);
        g.setOpacity (op);
        g.drawImage (tex.rust, full);
        ++k;
    }
}

// Draws one faceplate (incl. rack ears) in its own metallic colour.
inline void drawFace (juce::Graphics& g, const Textures& tex, juce::Rectangle<float> face, juce::Colour base,
                      float earL, float earR, int seed)
{
    juce::Path shape;
    shape.addRoundedRectangle (face, 3.0f);

    // drop shadow
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (face.translated (0, 2.5f), 3.0f);

    // screw positions (two per ear)
    std::vector<juce::Point<float>> screws;
    for (int side = 0; side < 2; ++side)
    {
        const float ew = side == 0 ? earL : earR;
        if (ew <= 0)
            continue;
        const float cx = side == 0 ? face.getX() + ew * 0.5f : face.getRight() - ew * 0.5f;
        screws.push_back ({ cx, face.getY() + 14.0f });
        screws.push_back ({ cx, face.getBottom() - 14.0f });
    }

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (shape);
        const auto full = juce::Rectangle<float> (0, 0, tex.brush.getWidth() / tex.scale, tex.brush.getHeight() / tex.scale);

        // metallic base: light from above, darker towards the bottom
        g.setGradientFill (juce::ColourGradient (base.brighter (0.45f), face.getX(), face.getY(),
                                                 base.darker (0.55f), face.getX(), face.getBottom(), false));
        g.fillRect (face);

        // diagonal glare band
        juce::ColourGradient glare (juce::Colours::white.withAlpha (0.0f), face.getX(), face.getY(),
                                    juce::Colours::white.withAlpha (0.0f), face.getX() + face.getHeight() * 1.2f + 260.0f, face.getBottom(), false);
        glare.addColour (0.42, juce::Colours::white.withAlpha (0.0f));
        glare.addColour (0.5, juce::Colours::white.withAlpha (0.2f));
        glare.addColour (0.58, juce::Colours::white.withAlpha (0.0f));
        g.setGradientFill (glare);
        g.fillRect (face);

        // brushed grain
        g.drawImage (tex.brush, full);

        // rust only around the screws
        juce::Random rnd (seed * 31 + 5);
        for (auto p : screws)
            drawRustHalo (g, tex, p, 6.0f, rnd.nextInt (1000));
    }

    // bevel
    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.drawLine (face.getX() + 3, face.getY() + 0.8f, face.getRight() - 3, face.getY() + 0.8f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.75f));
    g.drawRoundedRectangle (face, 3.0f, 1.2f);

    // ear seams + screws
    for (int side = 0; side < 2; ++side)
    {
        const float ew = side == 0 ? earL : earR;
        if (ew <= 0)
            continue;
        const float x = side == 0 ? face.getX() + ew : face.getRight() - ew;
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawVerticalLine ((int) x, face.getY() + 2, face.getBottom() - 2);
    }
    juce::Random rnd (seed);
    for (auto p : screws)
        drawScrew (g, p, 6.0f, rnd.nextInt());
}

} // namespace rb::ui::skin
