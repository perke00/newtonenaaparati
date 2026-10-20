#pragma once
#include "PluginProcessor.h"
#include "PianoRoll.h"
#include "Ui.h"

class RoboticLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RoboticLookAndFeel();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
};

// round icon button (undo, redo, fit, settings, record, preview)
class IconButton : public juce::Button
{
public:
    explicit IconButton (rt::Icon i, juce::Colour accentColour = rt::kAccent)
        : juce::Button ("icon"), icon (i), iconOn (i), accent (accentColour) {}
    void setIconOn (rt::Icon i) { iconOn = i; }
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    rt::Icon icon, iconOn;
    juce::Colour accent;
};

// big tool button: icon + name + shortcut key
class ToolButton : public juce::Button
{
public:
    ToolButton (rt::Icon i, const juce::String& text, const juce::String& key)
        : juce::Button (text), icon (i), label (text), keyHint (key) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    rt::Icon icon;
    juce::String label, keyHint;
};

// pill button with optional icon; primary = accent gradient
class PillButton : public juce::Button
{
public:
    PillButton (const juce::String& text, rt::Icon i, bool withIcon, bool isPrimary)
        : juce::Button (text), icon (i), hasIcon (withIcon), primary (isPrimary), label (text) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    rt::Icon icon;
    bool hasIcon, primary;
    juce::String label;
};

// pill button that drags a file out of the plugin into FL Studio
class DragButton : public PillButton
{
public:
    DragButton (const juce::String& text, rt::Icon i, bool primary) : PillButton (text, i, true, primary) {}
    std::function<juce::File()> makeFile;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override { dragging = false; PillButton::mouseUp (e); }
private:
    bool dragging = false;
};

class Canvas : public juce::Component
{
public:
    std::function<void (juce::Graphics&)> onPaint;
    void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
};

class AtlasToneEditor : public juce::AudioProcessorEditor,
                        public juce::FileDragAndDropTarget,
                        private juce::Timer
{
public:
    explicit AtlasToneEditor (AtlasToneProcessor&);
    ~AtlasToneEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { showMessage ("Drop to import this vocal"); }

private:
    void timerCallback() override;
    void paintCanvas (juce::Graphics&);
    void selectTool (PianoRoll::Tool t);
    void importFromFile (const juce::File& f);
    void showMessage (const juce::String& m) { message = m; messageTicks = 25; }
    juce::TextButton& makeButton (juce::TextButton& b, std::function<void()> onClick);
    void addCombo (juce::ComboBox& box, const char* id);
    void addSlider (juce::Slider& s, const char* id, const juce::String& suffix, int decimals);
    void addCaption (juce::Label& l, const juce::String& text);

    AtlasToneProcessor& proc;
    RoboticLookAndFeel laf;
    Canvas canvas;
    PianoRoll roll;
    float scale = 1.0f;

    IconButton recBtn { rt::Icon::Record, rt::kRed }, previewBtn { rt::Icon::Play, rt::kGreen },
               undoBtn { rt::Icon::Undo }, redoBtn { rt::Icon::Redo }, fitBtn { rt::Icon::Fit }, settingsBtn { rt::Icon::Settings };
    PillButton importBtn { "Import Vocal", rt::Icon::Import, true, false };
    juce::Label statusLabel;

    ToolButton drawBtn { rt::Icon::Draw, "Draw", "D" }, selectBtn { rt::Icon::Select, "Select", "V" },
               scissorsBtn { rt::Icon::Scissors, "Scissors", "C" }, trillBtn { rt::Icon::Trill, "Trill", "T" };

    juce::TextButton autoSnapBtn { "Auto Snap" }, detectNotesBtn { "Detect Notes" }, mergeBtn { "Merge" }, deleteBtn { "Delete" },
                     harmonyBtn { "Harmony" }, clearHarmBtn { "Clear Harm." }, clearAllBtn { "Clear All Notes" },
                     saveWavBtn { "Save WAV..." }, saveMidiBtn { "Save MIDI..." }, detectKeyBtn { "Detect Key" };
    DragButton dragWav { "Drag Audio to FL", rt::Icon::Export, true }, dragMidi { "Drag MIDI to FL", rt::Icon::Export, false };
    juce::ToggleButton bypassBtn { "Original (A/B)" }, snapBtn { "Snap to Scale" };

    juce::ComboBox keyBox, scaleBox, harmBox, trillRateBox, trillIntBox;
    juce::Slider roboticSl, variationSl, glideSl, leadSl, harmSl;
    juce::Label capKey, capScale, capRobotic, capVar, capGlide, capLead, capHInt, capHLvl, capTrRate, capTrInt;

    std::vector<std::pair<juce::String, juce::Rectangle<int>>> sections;
    juce::Rectangle<int> leftPanel, rightPanel;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt, snapAtt;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::String message;
    int messageTicks = 0;

    juce::ComponentBoundsConstrainer constrainer;
};
