#pragma once
#include "Common.h"

namespace atlas
{
// YIN pitch tracking of the whole take.
std::shared_ptr<Analysis> analyse (const float* x, int n, double sampleRate);

// Split the pitch contour into note blocks.
std::vector<Note> segmentNotes (const Analysis& a, std::atomic<int>& nextId);

float medianRange (const Analysis& a, int startFrame, int endFrame, float fallback);

// Guess the key from the lead notes. Returns { key 0..11, scaleIndex (0 major / 1 minor) }.
std::pair<int, int> detectKey (const std::vector<Note>& notes);

// TD-PSOLA render of one layer. keepOriginal = true for the lead layer
// (unvoiced parts like "s" and breaths stay untouched).
void renderLayer (const float* src, int numSamples, const Analysis& a,
                  const std::vector<Note>& notes, int layer,
                  float variation, float glideMs, bool keepOriginal,
                  std::vector<float>& out, const std::atomic<bool>& abort);
}
