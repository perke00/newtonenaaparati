#pragma once
#include "PluginProcessor.h"
#include "Ui.h"
#include <map>
#include <set>

class PianoRoll : public juce::Component, private juce::Timer
{
public:
    enum class Tool { Draw, Select, Scissors, Trill };

    explicit PianoRoll (AtlasToneProcessor& p);
    ~PianoRoll() override { stopTimer(); }

    void setTool (Tool t);
    Tool getTool() const { return tool; }
    std::vector<int> getSelection() const { return { selection.begin(), selection.end() }; }
    void fitToNotes();

    std::function<void (Tool)> onToolRequested;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;

    static constexpr int keysW = 56;
    static constexpr int rulerH = 30;

    float rowH() const { return (float) (getHeight() - rulerH) / (float) (viewHigh - viewLow); }
    float secToX (double s) const { return (float) keysW + (float) ((s - viewStartSec) * pxPerSec); }
    double xToSec (float x) const { return viewStartSec + (double) (x - (float) keysW) / pxPerSec; }
    float pitchToY (float p) const { return (float) rulerH + (float) (viewHigh - (double) p) * rowH(); }
    float yToPitch (float y) const { return (float) viewHigh - (y - (float) rulerH) / rowH(); }
    int xToFrame (float x, const Analysis& a) const;
    juce::Rectangle<float> noteRect (const Note& n, const Analysis& a) const;
    int hitTest (const std::vector<Note>& ns, const Analysis& a, juce::Point<float> p) const;
    float pitchFromY (float y) const;
    void paintAt (int f0, int f1, float pitch);
    void freeDrawStep (int fa, int fb, float pitch);   // note tracking off: draw notes by hand

    AtlasToneProcessor& proc;
    Tool tool = Tool::Draw;
    std::set<int> selection;

    double viewStartSec = 0.0;
    double pxPerSec = 80.0;
    double viewLow = 48.0, viewHigh = 84.0;

    enum class Drag { None, Paint, Create, Move, ResizeL, ResizeR, Band, Pan, FreeDraw, Seek };
    Drag drag = Drag::None;
    int lastFrame = 0, createStart = 0, activeId = 0;
    float dragStartPitch = 0.0f;
    int drawNoteId = -1, drawMinF = 0, drawMaxF = 0;
    float drawPitch = 0.0f;
    std::map<int, float> origTarget;
    juce::Point<float> bandStart, bandEnd, mousePos, panStart;
    double panStartSec = 0.0, panStartLow = 0.0, panStartHigh = 0.0;
    int lastNotesVer = -1, lastAnalysisVer = -1, lastRenderVer = -1;
    bool lastPlaying = false;
};
