#pragma once
#include "Common.h"
#include "Dsp.h"
#include <functional>
#include <mutex>

using atlas::Note;
using atlas::Analysis;

class AtlasToneProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener
{
public:
    AtlasToneProcessor();
    ~AtlasToneProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Robotic Tune"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- parameters
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    atlas::Scale currentScale() const;
    bool snapEnabled() const;
    bool trackingEnabled() const;   // false = free draw, no automatic note detection
    bool autoKeyEnabled() const;

    // ---- status shown in the UI
    std::atomic<bool> armed { false }, recording { false }, analysing { false }, hasTake { false };
    std::atomic<juce::int64> hostPos { 0 }, takeOrigin { 0 };
    std::atomic<bool> hostPlaying { false };
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<int> takeLength { 0 }, recWriteMax { 0 };
    std::atomic<int> notesVersion { 0 }, analysisVersion { 0 }, renderVersion { 0 };
    std::atomic<double> takeSampleRate { 44100.0 };

    // ---- built-in preview player (works with the host stopped)
    std::atomic<bool> previewPlaying { false };
    std::atomic<juce::int64> previewPos { 0 };
    void togglePreview();
    void seekPreview (double seconds);
    double playheadSeconds() const;

    // ---- recording
    void armRecording (bool shouldArm);

    // ---- import / export
    bool importFile (const juce::File& file);   // message thread; loads, resamples to host rate, starts analysis
    bool writeAudio (const juce::File& file);   // corrected vocal (lead + harmony) as 24-bit mono WAV
    juce::String getTakeName() const;

    // ---- data access
    std::shared_ptr<const Analysis> getAnalysis() const;
    std::vector<Note> getNotes() const;
    int newId() { return nextId++; }

    // ---- editing (all thread-safe, all trigger a re-render)
    void beginEdit();   // push an undo snapshot (call once at the start of a drag)
    void editNotes (const std::function<void (std::vector<Note>&)>& fn, bool pushUndo);
    void undo();
    void redo();
    void splitNote (int id, int frame);
    void applyTrill (int id);
    void createHarmony (const std::vector<int>& ids);
    void clearHarmony();
    void autoSnap (const std::vector<int>& ids);
    void mergeNotes (const std::vector<int>& ids);
    void deleteNotes (const std::vector<int>& ids);
    void detectAndSetKey();
    void detectNotes();   // run automatic note tracking on the current take
    void clearNotes();

    bool writeMidi (const juce::File& file);

    // called by the worker thread
    void workerTick();

private:
    void parameterChanged (const juce::String& id, float) override;
    void markDirty();
    void doAnalysis (bool segment);
    void doRender();
    void setParam (const char* id, int value);

    double sampleRate = 44100.0;
    int recCap = 0;
    std::vector<float> recBuf;
    std::mutex workMutex;

    juce::CriticalSection dataLock;
    std::shared_ptr<const Analysis> analysis;
    std::vector<Note> notes;
    std::vector<std::vector<Note>> undoStack, redoStack;
    std::atomic<int> nextId { 1 };

    juce::SpinLock outLock;
    std::vector<float> leadOut, harmOut, origOut;
    juce::String takeName;

    std::atomic<bool> needsAnalysis { false }, loadAnalysisPending { false };
    std::atomic<bool> renderDirty { false }, abortRender { false };
    std::atomic<juce::uint32> lastEditMs { 0 };

    std::unique_ptr<juce::Thread> worker;

    JUCE_DECLARE_WEAK_REFERENCEABLE (AtlasToneProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtlasToneProcessor)
};
