#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

namespace atlas
{
// One note block on the piano roll. Time is in analysis frames (see Analysis::hop).
struct Note
{
    int id = 0;
    int start = 0;          // first frame (inclusive)
    int end = 0;            // last frame (exclusive)
    float center = 60.0f;   // detected pitch of the sung note (midi, float)
    float target = 60.0f;   // pitch the note is moved to
    int layer = 0;          // 0 = lead, 1 = harmony
};

struct Analysis
{
    int hop = 256;
    double sampleRate = 44100.0;
    int numSamples = 0;
    std::vector<float> f0;     // midi pitch per frame, <= 0 means unvoiced
    std::vector<float> peak;   // waveform peak per frame

    int numFrames() const { return (int) f0.size(); }
    // time (seconds) of the left edge of a frame
    double frameSec (double f) const { return (f * hop - hop * 0.5) / sampleRate; }
};

inline float hzToMidi (float hz) { return 69.0f + 12.0f * std::log2 (hz / 440.0f); }
inline float midiToHz (float m)  { return 440.0f * std::pow (2.0f, (m - 69.0f) / 12.0f); }

inline const char* const* noteNames()
{
    static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n;
}

inline juce::String midiName (int midi)
{
    return juce::String (noteNames()[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
}

struct ScaleDef { const char* name; std::vector<int> steps; };

inline const std::vector<ScaleDef>& scaleDefs()
{
    static const std::vector<ScaleDef> d = {
        { "Major",            { 0, 2, 4, 5, 7, 9, 11 } },
        { "Minor",            { 0, 2, 3, 5, 7, 8, 10 } },
        { "Harmonic Minor",   { 0, 2, 3, 5, 7, 8, 11 } },
        { "Melodic Minor",    { 0, 2, 3, 5, 7, 9, 11 } },
        { "Dorian",           { 0, 2, 3, 5, 7, 9, 10 } },
        { "Phrygian",         { 0, 1, 3, 5, 7, 8, 10 } },
        { "Lydian",           { 0, 2, 4, 6, 7, 9, 11 } },
        { "Mixolydian",       { 0, 2, 4, 5, 7, 9, 10 } },
        { "Major Pentatonic", { 0, 2, 4, 7, 9 } },
        { "Minor Pentatonic", { 0, 3, 5, 7, 10 } },
        { "Blues",            { 0, 3, 5, 6, 7, 10 } },
        { "Chromatic",        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } }
    };
    return d;
}

struct Scale
{
    int key = 0;
    std::vector<int> steps { 0, 2, 4, 5, 7, 9, 11 };

    bool contains (int midi) const
    {
        const int pc = (((midi - key) % 12) + 12) % 12;
        for (int s : steps) if (s == pc) return true;
        return false;
    }

    // nearest note of the scale
    float snap (float p) const
    {
        const int base = (int) std::lround (p);
        float best = (float) base, bd = 1.0e9f;
        for (int m = base - 7; m <= base + 7; ++m)
            if (contains (m))
            {
                const float d = std::abs ((float) m - p);
                if (d < bd) { bd = d; best = (float) m; }
            }
        return best;
    }

    bool isInKey (float p, float tol = 0.3f) const { return std::abs (snap (p) - p) <= tol; }

    // move p by a number of scale degrees (2 = a third, 4 = a fifth, 7 = octave)
    float shiftSteps (float p, int stepsToMove) const
    {
        int m = (int) std::lround (snap (p));
        const int dir = stepsToMove >= 0 ? 1 : -1;
        int count = std::abs (stepsToMove);
        while (count > 0) { m += dir; if (contains (m)) --count; }
        return (float) m;
    }
};

inline Scale makeScale (int key, int scaleIdx)
{
    Scale s;
    s.key = juce::jlimit (0, 11, key);
    const auto& defs = scaleDefs();
    s.steps = defs[(size_t) juce::jlimit (0, (int) defs.size() - 1, scaleIdx)].steps;
    return s;
}
} // namespace atlas
