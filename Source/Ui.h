#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <cmath>

// Shared look: colours, icons, small helpers.
namespace rt
{
inline const juce::Colour kBg0 { 0xff060912 }, kBg1 { 0xff0c1222 }, kPanel { 0xff0f1626 }, kPanelHi { 0xff161f36 },
                          kLine { 0xff222d48 }, kText { 0xffdfe6f7 }, kDim { 0xff7f8cab },
                          kAccent { 0xff2fd4ff }, kAccent2 { 0xff8b5cf6 }, kRed { 0xffff4d6d }, kGreen { 0xff35e0a1 };

inline float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

enum class Icon { Draw, Select, Scissors, Trill, Record, Import, Export, Undo, Redo, Fit, Settings, Play, Stop };

// Vector icons, all drawn in a square that is centred in `bounds`.
inline void drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> bounds, juce::Colour c, float stroke = 1.8f)
{
    const float s = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto r = juce::Rectangle<float> (s, s).withCentre (bounds.getCentre());
    auto P = [&] (float x, float y) { return juce::Point<float> (r.getX() + x * s, r.getY() + y * s); };
    const juce::PathStrokeType st (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path p;
    g.setColour (c);

    auto poly = [&] (std::initializer_list<juce::Point<float>> pts, bool close)
    {
        bool first = true;
        for (auto& q : pts)
        {
            if (first) { p.startNewSubPath (q); first = false; }
            else p.lineTo (q);
        }
        if (close) p.closeSubPath();
    };
    auto circle = [&] (float cx, float cy, float rad)
    {
        p.addEllipse (r.getX() + (cx - rad) * s, r.getY() + (cy - rad) * s, 2.0f * rad * s, 2.0f * rad * s);
    };

    switch (icon)
    {
        case Icon::Draw:
            poly ({ P (0.14f, 0.86f), P (0.22f, 0.58f), P (0.60f, 0.20f), P (0.80f, 0.40f), P (0.42f, 0.78f) }, true);
            poly ({ P (0.22f, 0.58f), P (0.42f, 0.78f) }, false);
            g.strokePath (p, st);
            break;

        case Icon::Select:
            poly ({ P (0.28f, 0.12f), P (0.28f, 0.82f), P (0.44f, 0.67f), P (0.56f, 0.90f),
                    P (0.67f, 0.85f), P (0.55f, 0.62f), P (0.77f, 0.60f) }, true);
            g.fillPath (p);
            break;

        case Icon::Scissors:
            circle (0.27f, 0.77f, 0.13f);
            circle (0.73f, 0.77f, 0.13f);
            poly ({ P (0.33f, 0.66f), P (0.82f, 0.10f) }, false);
            poly ({ P (0.67f, 0.66f), P (0.18f, 0.10f) }, false);
            g.strokePath (p, st);
            break;

        case Icon::Trill:
            poly ({ P (0.08f, 0.64f), P (0.28f, 0.64f), P (0.28f, 0.30f), P (0.50f, 0.30f), P (0.50f, 0.72f),
                    P (0.72f, 0.72f), P (0.72f, 0.38f), P (0.92f, 0.38f) }, false);
            g.strokePath (p, st);
            break;

        case Icon::Record:
            circle (0.5f, 0.5f, 0.28f);
            g.fillPath (p);
            break;

        case Icon::Import:
        case Icon::Export:
        {
            const bool in = icon == Icon::Import;
            poly ({ P (0.18f, 0.62f), P (0.18f, 0.84f), P (0.82f, 0.84f), P (0.82f, 0.62f) }, false);
            if (in)
            {
                poly ({ P (0.5f, 0.12f), P (0.5f, 0.66f) }, false);
                poly ({ P (0.30f, 0.47f), P (0.5f, 0.67f), P (0.70f, 0.47f) }, false);
            }
            else
            {
                poly ({ P (0.5f, 0.66f), P (0.5f, 0.14f) }, false);
                poly ({ P (0.30f, 0.34f), P (0.5f, 0.14f), P (0.70f, 0.34f) }, false);
            }
            g.strokePath (p, st);
            break;
        }

        case Icon::Undo:
        case Icon::Redo:
        {
            auto X = [&] (float x) { return icon == Icon::Redo ? 1.0f - x : x; };
            p.startNewSubPath (P (X (0.26f), 0.40f));
            p.quadraticTo (P (X (0.50f), 0.18f), P (X (0.74f), 0.36f));
            p.quadraticTo (P (X (0.92f), 0.56f), P (X (0.62f), 0.80f));
            p.startNewSubPath (P (X (0.46f), 0.22f));
            p.lineTo (P (X (0.26f), 0.40f));
            p.lineTo (P (X (0.44f), 0.58f));
            g.strokePath (p, st);
            break;
        }

        case Icon::Fit:
            poly ({ P (0.15f, 0.38f), P (0.15f, 0.15f), P (0.38f, 0.15f) }, false);
            poly ({ P (0.62f, 0.15f), P (0.85f, 0.15f), P (0.85f, 0.38f) }, false);
            poly ({ P (0.85f, 0.62f), P (0.85f, 0.85f), P (0.62f, 0.85f) }, false);
            poly ({ P (0.38f, 0.85f), P (0.15f, 0.85f), P (0.15f, 0.62f) }, false);
            g.strokePath (p, st);
            break;

        case Icon::Settings:
        {
            circle (0.5f, 0.5f, 0.21f);
            circle (0.5f, 0.5f, 0.07f);
            for (int k = 0; k < 8; ++k)
            {
                const float a = (float) k * juce::MathConstants<float>::pi / 4.0f;
                const float ca = std::cos (a), sa = std::sin (a);
                poly ({ P (0.5f + 0.29f * ca, 0.5f + 0.29f * sa), P (0.5f + 0.40f * ca, 0.5f + 0.40f * sa) }, false);
            }
            g.strokePath (p, juce::PathStrokeType (stroke * 1.15f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            break;
        }

        case Icon::Play:
            poly ({ P (0.30f, 0.16f), P (0.30f, 0.84f), P (0.84f, 0.50f) }, true);
            g.fillPath (p);
            break;

        case Icon::Stop:
            p.addRoundedRectangle (r.getX() + 0.24f * s, r.getY() + 0.24f * s, 0.52f * s, 0.52f * s, 2.5f);
            g.fillPath (p);
            break;
    }
}
} // namespace rt
