#include "PluginEditor.h"

namespace
{
using rt::kAccent; using rt::kAccent2; using rt::kPanel; using rt::kPanelHi; using rt::kLine; using rt::kText; using rt::kDim;

juce::Font bold (float h) { return juce::Font (juce::FontOptions (h, juce::Font::bold)); }
juce::Font plain (float h) { return juce::Font (juce::FontOptions (h)); }

bool isAudioFile (const juce::File& f)
{
    static const juce::StringArray exts { ".wav", ".aif", ".aiff", ".flac", ".mp3", ".m4a", ".ogg", ".wma" };
    return exts.contains (f.getFileExtension().toLowerCase());
}
}

// -------------------------------------------------------------- look & feel
RoboticLookAndFeel::RoboticLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, rt::kBg0);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff18213a));
    setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff24345c));
    setColour (juce::TextButton::textColourOffId, kText);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::ComboBox::textColourId, kText);
    setColour (juce::ComboBox::arrowColourId, kDim);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff121a2e));
    setColour (juce::PopupMenu::textColourId, kText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff27406e));
    setColour (juce::Slider::textBoxTextColourId, kText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ToggleButton::textColourId, kText);
    setColour (juce::Label::textColourId, kDim);
}

juce::Font RoboticLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return bold (13.0f); }
juce::Font RoboticLookAndFeel::getComboBoxFont (juce::ComboBox&) { return plain (13.5f); }

void RoboticLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    juce::Colour base = b.getToggleState() ? kAccent.withAlpha (0.28f) : juce::Colour (0xff17203a);
    if (over) base = base.brighter (0.14f);
    if (down) base = base.darker (0.2f);
    if (! b.isEnabled()) base = base.withAlpha (0.4f);
    g.setColour (base);
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (b.getToggleState() ? kAccent.withAlpha (0.8f) : kLine);
    g.drawRoundedRectangle (r, 9.0f, 1.0f);
}

void RoboticLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    const auto area = b.getLocalBounds().toFloat();
    const float ph = 18.0f, pw = 34.0f;
    const juce::Rectangle<float> pill (area.getX() + 1.0f, area.getCentreY() - ph * 0.5f, pw, ph);
    const bool on = b.getToggleState();

    g.setColour (on ? kAccent : juce::Colour (0xff2a3552));
    g.fillRoundedRectangle (pill, ph * 0.5f);
    const float kx = on ? pill.getRight() - ph + 2.0f : pill.getX() + 2.0f;
    g.setColour (juce::Colours::white.withAlpha (over ? 1.0f : 0.92f));
    g.fillEllipse (kx, pill.getY() + 2.0f, ph - 4.0f, ph - 4.0f);

    g.setColour (kText);
    g.setFont (plain (13.0f));
    g.drawText (b.getButtonText(), (int) pill.getRight() + 10, 0, b.getWidth() - (int) pill.getRight() - 10, b.getHeight(),
                juce::Justification::centredLeft, true);
}

void RoboticLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool down, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (down ? juce::Colour (0xff1f2a48) : juce::Colour (0xff151d33));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (box.hasKeyboardFocus (true) ? kAccent : kLine);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    juce::Path p;
    const float cx = (float) w - 15.0f, cy = (float) h * 0.5f;
    p.startNewSubPath (cx - 4.0f, cy - 2.0f);
    p.lineTo (cx, cy + 2.5f);
    p.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (kDim);
    g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void RoboticLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float, float,
                                           juce::Slider::SliderStyle, juce::Slider& s)
{
    const float cy = (float) y + (float) height * 0.5f;
    const juce::Rectangle<float> track ((float) x, cy - 2.5f, (float) width, 5.0f);
    g.setColour (juce::Colour (0xff1e2845));
    g.fillRoundedRectangle (track, 2.5f);

    const auto filled = track.withRight (juce::jlimit (track.getX(), track.getRight(), sliderPos));
    g.setGradientFill (juce::ColourGradient (kAccent, track.getX(), 0.0f, kAccent2, track.getRight(), 0.0f, false));
    g.fillRoundedRectangle (filled, 2.5f);

    const bool hot = s.isMouseOverOrDragging();
    g.setColour (kAccent.withAlpha (hot ? 0.35f : 0.18f));
    g.fillEllipse (sliderPos - 10.0f, cy - 10.0f, 20.0f, 20.0f);
    g.setColour (juce::Colours::white);
    g.fillEllipse (sliderPos - 6.5f, cy - 6.5f, 13.0f, 13.0f);
}

// ------------------------------------------------------------------ buttons
void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();
    const float a = isEnabled() ? 1.0f : 0.35f;

    if (on)
    {
        g.setColour (accent.withAlpha (0.28f));
        g.fillEllipse (r.expanded (1.5f));
    }
    g.setColour ((on ? accent.withAlpha (0.22f) : juce::Colour (0xff17203a)).brighter (over ? 0.14f : 0.0f).withMultipliedAlpha (a));
    g.fillEllipse (r);
    g.setColour ((on ? accent : kLine).withMultipliedAlpha (a));
    g.drawEllipse (r, 1.2f);

    const auto ib = r.reduced (r.getWidth() * 0.27f);
    const auto c = (on || icon == rt::Icon::Record ? accent : kText).withMultipliedAlpha (a);
    rt::drawIcon (g, on ? iconOn : icon, down ? ib.reduced (1.0f) : ib, c, 1.8f);
}

void ToolButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();

    if (on)
    {
        g.setGradientFill (juce::ColourGradient (kAccent.withAlpha (0.30f), r.getX(), 0.0f, kAccent2.withAlpha (0.22f), r.getRight(), 0.0f, false));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (kAccent.withAlpha (0.85f));
        g.drawRoundedRectangle (r, 10.0f, 1.2f);
    }
    else
    {
        g.setColour (juce::Colour (0xff141c32).brighter (over ? 0.14f : 0.0f).darker (down ? 0.2f : 0.0f));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (kLine);
        g.drawRoundedRectangle (r, 10.0f, 1.0f);
    }

    rt::drawIcon (g, icon, { r.getX() + 12.0f, r.getCentreY() - 11.0f, 22.0f, 22.0f }, on ? kAccent : kText.withAlpha (0.8f), 1.9f);

    g.setColour (on ? juce::Colours::white : kText);
    g.setFont (bold (14.0f));
    g.drawText (label, (int) r.getX() + 46, 0, getWidth() - 46 - 40, getHeight(), juce::Justification::centredLeft, false);

    const juce::Rectangle<float> kb (r.getRight() - 30.0f, r.getCentreY() - 10.0f, 20.0f, 20.0f);
    g.setColour (juce::Colours::white.withAlpha (on ? 0.16f : 0.07f));
    g.fillRoundedRectangle (kb, 5.0f);
    g.setColour (on ? juce::Colours::white : kDim);
    g.setFont (bold (11.0f));
    g.drawText (keyHint, kb, juce::Justification::centred, false);
}

void PillButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float a = isEnabled() ? 1.0f : 0.4f;

    if (primary)
    {
        auto c1 = kAccent, c2 = kAccent2;
        if (over) { c1 = c1.brighter (0.15f); c2 = c2.brighter (0.15f); }
        if (down) { c1 = c1.darker (0.2f); c2 = c2.darker (0.2f); }
        g.setGradientFill (juce::ColourGradient (c1.withMultipliedAlpha (a), r.getX(), 0.0f, c2.withMultipliedAlpha (a), r.getRight(), 0.0f, false));
        g.fillRoundedRectangle (r, 10.0f);
    }
    else
    {
        g.setColour (juce::Colour (0xff17203a).brighter (over ? 0.14f : 0.0f).darker (down ? 0.2f : 0.0f).withMultipliedAlpha (a));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (kLine);
        g.drawRoundedRectangle (r, 10.0f, 1.0f);
    }

    const auto fg = (primary ? juce::Colour (0xff04101c) : kText).withMultipliedAlpha (a);
    const auto f = bold (13.5f);
    const float is = juce::jmin (r.getHeight() - 14.0f, 20.0f);
    const float tw = rt::textWidth (f, label);
    const float total = (hasIcon ? is + 8.0f : 0.0f) + tw;
    float x = r.getCentreX() - total * 0.5f;

    if (hasIcon)
    {
        rt::drawIcon (g, icon, { x, r.getCentreY() - is * 0.5f, is, is }, primary ? fg : kAccent.withMultipliedAlpha (a), 2.0f);
        x += is + 8.0f;
    }
    g.setColour (fg);
    g.setFont (f);
    g.drawText (label, (int) x, 0, (int) tw + 6, getHeight(), juce::Justification::centredLeft, false);
}

void DragButton::mouseDrag (const juce::MouseEvent& e)
{
    PillButton::mouseDrag (e);
    if (! dragging && e.getDistanceFromDragStart() > 6 && makeFile)
    {
        dragging = true;
        const auto f = makeFile();
        if (f.existsAsFile())
            juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this,
                                                                        [this] { dragging = false; });
        else
            dragging = false;
    }
}

// ------------------------------------------------------------------ settings
class SettingsPanel : public juce::Component
{
public:
    explicit SettingsPanel (AtlasToneProcessor& p)
    {
        setLookAndFeel (&laf);
        for (auto* b : { &trackBtn, &keyBtn, &followBtn }) addAndMakeVisible (*b);
        trackAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "tracking", trackBtn);
        keyAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "autoKey", keyBtn);
        followAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "follow", followBtn);

        info.setText ("Note tracking ON: notes are detected from the vocal automatically.\n"
                      "Note tracking OFF = free draw: nothing is detected, you draw the melody yourself "
                      "with the Draw tool (drag up and down = hard robotic steps).\n"
                      "Applies to the next import / recording. Use Detect Notes to track the current vocal.",
                      juce::dontSendNotification);
        info.setFont (plain (12.0f));
        info.setColour (juce::Label::textColourId, kDim);
        info.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (info);
        setSize (360, 250);
    }
    ~SettingsPanel() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kPanel);
        g.setColour (kAccent);
        g.setFont (bold (12.0f));
        g.drawText ("SETTINGS", 18, 12, 200, 18, juce::Justification::centredLeft, false);
        g.setColour (kLine);
        g.drawHorizontalLine (36, 14.0f, (float) getWidth() - 14.0f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (18, 0);
        r.removeFromTop (46);
        trackBtn.setBounds (r.removeFromTop (30));
        keyBtn.setBounds (r.removeFromTop (30));
        followBtn.setBounds (r.removeFromTop (30));
        r.removeFromTop (8);
        info.setBounds (r.removeFromTop (110));
    }

private:
    RoboticLookAndFeel laf;
    juce::ToggleButton trackBtn { "Note tracking  (auto-detect notes)" }, keyBtn { "Auto-detect key on import" }, followBtn { "Follow playhead" };
    juce::Label info;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> trackAtt, keyAtt, followAtt;
};

// ------------------------------------------------------------------ editor
AtlasToneEditor::AtlasToneEditor (AtlasToneProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), roll (p)
{
    setLookAndFeel (&laf);
    addAndMakeVisible (canvas);
    canvas.onPaint = [this] (juce::Graphics& g) { paintCanvas (g); };
    canvas.addAndMakeVisible (roll);
    roll.onToolRequested = [this] (PianoRoll::Tool t) { selectTool (t); };

    // ---- header
    canvas.addAndMakeVisible (recBtn);
    recBtn.onClick = [this] { proc.armRecording (! (proc.armed.load() || proc.recording.load())); };

    canvas.addAndMakeVisible (importBtn);
    importBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Import vocal",
            juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f != juce::File()) importFromFile (f);
                              });
    };

    canvas.addAndMakeVisible (previewBtn);
    previewBtn.setIconOn (rt::Icon::Stop);
    previewBtn.onClick = [this] { proc.togglePreview(); };

    for (auto* b : { &undoBtn, &redoBtn, &fitBtn, &settingsBtn }) canvas.addAndMakeVisible (*b);
    undoBtn.onClick = [this] { proc.undo(); };
    redoBtn.onClick = [this] { proc.redo(); };
    fitBtn.onClick = [this] { roll.fitToNotes(); };
    settingsBtn.onClick = [this]
    {
        juce::CallOutBox::launchAsynchronously (std::make_unique<SettingsPanel> (proc), settingsBtn.getScreenBounds(), nullptr);
    };

    statusLabel.setFont (bold (13.0f));
    statusLabel.setColour (juce::Label::textColourId, kText);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    canvas.addAndMakeVisible (statusLabel);

    canvas.addAndMakeVisible (bypassBtn);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "bypass", bypassBtn);

    // ---- tools
    for (auto* b : { &drawBtn, &selectBtn, &scissorsBtn, &trillBtn })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (101);
        canvas.addAndMakeVisible (*b);
    }
    drawBtn.setToggleState (true, juce::dontSendNotification);
    drawBtn.onClick     = [this] { selectTool (PianoRoll::Tool::Draw); };
    selectBtn.onClick   = [this] { selectTool (PianoRoll::Tool::Select); };
    scissorsBtn.onClick = [this] { selectTool (PianoRoll::Tool::Scissors); };
    trillBtn.onClick    = [this] { selectTool (PianoRoll::Tool::Trill); };

    // ---- edit
    makeButton (autoSnapBtn,    [this] { proc.autoSnap (roll.getSelection()); });
    makeButton (detectNotesBtn, [this] { proc.detectNotes(); });
    makeButton (mergeBtn,       [this] { proc.mergeNotes (roll.getSelection()); });
    makeButton (deleteBtn,      [this] { proc.deleteNotes (roll.getSelection()); });
    makeButton (harmonyBtn,     [this] { proc.createHarmony (roll.getSelection()); });
    makeButton (clearHarmBtn,   [this] { proc.clearHarmony(); });
    makeButton (clearAllBtn,    [this] { proc.clearNotes(); });
    makeButton (detectKeyBtn,   [this] { proc.detectAndSetKey(); });

    // ---- export back to the FL track
    dragWav.makeFile = [this]
    {
        auto name = proc.getTakeName();
        if (name.isEmpty()) name = "Vocal";
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("RoboticTune");
        dir.createDirectory();
        const auto f = dir.getChildFile (name + "_RoboticTune.wav");
        return proc.writeAudio (f) ? f : juce::File();
    };
    canvas.addAndMakeVisible (dragWav);

    makeButton (saveWavBtn, [this]
    {
        auto name = proc.getTakeName();
        if (name.isEmpty()) name = "Vocal";
        chooser = std::make_unique<juce::FileChooser> ("Save corrected vocal",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (name + "_RoboticTune.wav"), "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f == juce::File()) return;
                                  showMessage (proc.writeAudio (f.withFileExtension ("wav")) ? "Saved " + f.getFileName() : "Nothing to save yet");
                              });
    });

    makeButton (saveMidiBtn, [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Save MIDI",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("RoboticTune.mid"), "*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f != juce::File()) proc.writeMidi (f);
                              });
    });

    dragMidi.makeFile = [this]
    {
        const auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("RoboticTune.mid");
        return proc.writeMidi (f) ? f : juce::File();
    };
    canvas.addAndMakeVisible (dragMidi);

    // ---- right column
    canvas.addAndMakeVisible (snapBtn);
    snapAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "snap", snapBtn);

    addCaption (capKey, "KEY");                      addCombo (keyBox, "key");
    addCaption (capScale, "SCALE");                  addCombo (scaleBox, "scale");
    addCaption (capRobotic, "ROBOTIC  (hard-tune steps, no glide)");  addSlider (roboticSl, "robotic", " %", 0);
    addCaption (capVar, "VARIATION  (natural vibrato)");              addSlider (variationSl, "variation", "", 2);
    addCaption (capGlide, "GLIDE BETWEEN NOTES");    addSlider (glideSl, "glide", " ms", 0);
    addCaption (capLead, "LEAD LEVEL");              addSlider (leadSl, "leadGain", " dB", 1);
    addCaption (capHInt, "HARMONY INTERVAL");        addCombo (harmBox, "harmInterval");
    addCaption (capHLvl, "HARMONY LEVEL");           addSlider (harmSl, "harmGain", " dB", 1);
    addCaption (capTrRate, "TRILL SPEED");           addCombo (trillRateBox, "trillRate");
    addCaption (capTrInt, "TRILL INTERVAL");         addCombo (trillIntBox, "trillInt");

    constrainer.setMinimumSize (1000, 620);
    constrainer.setMaximumSize (2800, 1600);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (1280, 760);
    startTimerHz (10);
}

AtlasToneEditor::~AtlasToneEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

juce::TextButton& AtlasToneEditor::makeButton (juce::TextButton& b, std::function<void()> onClick)
{
    b.onClick = std::move (onClick);
    canvas.addAndMakeVisible (b);
    return b;
}

void AtlasToneEditor::addCaption (juce::Label& l, const juce::String& text)
{
    l.setText (text, juce::dontSendNotification);
    l.setFont (bold (10.5f));
    l.setColour (juce::Label::textColourId, kDim);
    canvas.addAndMakeVisible (l);
}

void AtlasToneEditor::addCombo (juce::ComboBox& box, const char* id)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        box.addItemList (p->choices, 1);
    canvas.addAndMakeVisible (box);
    comboAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, box));
}

void AtlasToneEditor::addSlider (juce::Slider& s, const char* id, const juce::String& suffix, int decimals)
{
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
    canvas.addAndMakeVisible (s);
    sliderAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, s));

    // the attachment installs its own text functions; replace them so the unit suffix shows
    s.textFromValueFunction = [suffix, decimals] (double v) { return juce::String (v, decimals) + suffix; };
    s.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };
    s.updateText();
}

void AtlasToneEditor::selectTool (PianoRoll::Tool t)
{
    roll.setTool (t);
    drawBtn.setToggleState (t == PianoRoll::Tool::Draw, juce::dontSendNotification);
    selectBtn.setToggleState (t == PianoRoll::Tool::Select, juce::dontSendNotification);
    scissorsBtn.setToggleState (t == PianoRoll::Tool::Scissors, juce::dontSendNotification);
    trillBtn.setToggleState (t == PianoRoll::Tool::Trill, juce::dontSendNotification);
}

void AtlasToneEditor::importFromFile (const juce::File& f)
{
    if (proc.importFile (f)) showMessage ("Imported: " + f.getFileName());
    else                     showMessage ("Could not read that file (use WAV, AIFF, FLAC, MP3)");
}

bool AtlasToneEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (isAudioFile (juce::File (f))) return true;
    return false;
}

void AtlasToneEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& path : files)
    {
        const juce::File f (path);
        if (isAudioFile (f)) { importFromFile (f); break; }
    }
}

void AtlasToneEditor::timerCallback()
{
    recBtn.setToggleState (proc.armed.load() || proc.recording.load(), juce::dontSendNotification);
    previewBtn.setToggleState (proc.previewPlaying.load(), juce::dontSendNotification);
    previewBtn.setEnabled (proc.hasTake.load() && ! proc.hostPlaying.load());
    dragWav.setEnabled (proc.hasTake.load());
    saveWavBtn.setEnabled (proc.hasTake.load());

    juce::String s;
    if (messageTicks > 0)             { s = message; --messageTicks; }
    else if (proc.recording.load())   s = "RECORDING - stop FL playback when the vocal ends";
    else if (proc.armed.load())       s = "ARMED - press PLAY in FL Studio";
    else if (proc.analysing.load())   s = "Analysing vocal...";
    else if (proc.hasTake.load())
    {
        int lead = 0, harm = 0;
        for (auto& n : proc.getNotes()) (n.layer == 0 ? lead : harm)++;
        s = proc.getTakeName() + "  -  " + juce::String (lead) + " notes" + (harm > 0 ? ", " + juce::String (harm) + " harmony" : juce::String());
        if (lead == 0 && ! proc.trackingEnabled()) s += "  -  free draw: draw your notes";
    }
    else s = "Import a vocal (drag & drop) or press REC";
    statusLabel.setText (s, juce::dontSendNotification);
}

// ------------------------------------------------------------------- paint
void AtlasToneEditor::paintCanvas (juce::Graphics& g)
{
    const int w = canvas.getWidth(), h = canvas.getHeight();

    g.setGradientFill (juce::ColourGradient (rt::kBg1, 0.0f, 0.0f, rt::kBg0, 0.0f, (float) h, false));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (kAccent.withAlpha (0.10f), 140.0f, 0.0f, juce::Colours::transparentBlack, 140.0f, 460.0f, true));
    g.fillRect (0, 0, w, 480);
    g.setGradientFill (juce::ColourGradient (kAccent2.withAlpha (0.08f), (float) w - 120.0f, (float) h, juce::Colours::transparentBlack,
                                             (float) w - 120.0f, (float) h - 420.0f, true));
    g.fillRect (0, h - 480, w, 480);

    // header
    g.setColour (kPanel.withAlpha (0.88f));
    g.fillRect (0, 0, w, 72);
    g.setGradientFill (juce::ColourGradient (kAccent.withAlpha (0.85f), 0.0f, 71.0f, kAccent2.withAlpha (0.85f), (float) w, 71.0f, false));
    g.fillRect (0, 71, w, 1);

    // logo: rounded square with a stepped (robotic) waveform
    const juce::Rectangle<float> lg (18.0f, 12.0f, 48.0f, 48.0f);
    g.setColour (kAccent.withAlpha (0.25f));
    g.fillRoundedRectangle (lg.expanded (3.0f), 16.0f);
    g.setGradientFill (juce::ColourGradient (kAccent, lg.getX(), lg.getY(), kAccent2, lg.getRight(), lg.getBottom(), false));
    g.fillRoundedRectangle (lg, 13.0f);
    juce::Path wave;
    const float lx = lg.getX(), ly = lg.getY();
    wave.startNewSubPath (lx + 9.0f, ly + 31.0f);
    wave.lineTo (lx + 17.0f, ly + 31.0f);
    wave.lineTo (lx + 17.0f, ly + 15.0f);
    wave.lineTo (lx + 25.0f, ly + 15.0f);
    wave.lineTo (lx + 25.0f, ly + 34.0f);
    wave.lineTo (lx + 33.0f, ly + 34.0f);
    wave.lineTo (lx + 33.0f, ly + 21.0f);
    wave.lineTo (lx + 40.0f, ly + 21.0f);
    g.setColour (juce::Colours::white);
    g.strokePath (wave, juce::PathStrokeType (3.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));

    const auto tf = bold (26.0f);
    g.setFont (tf);
    g.setColour (juce::Colours::white);
    g.drawText ("ROBOTIC", 82, 9, 200, 34, juce::Justification::centredLeft, false);
    g.setColour (kAccent);
    g.drawText ("TUNE", 82 + (int) rt::textWidth (tf, "ROBOTIC "), 9, 120, 34, juce::Justification::centredLeft, false);
    g.setColour (kDim);
    g.setFont (plain (12.5f));
    g.drawText ("by ProdByPerke   |   hard-tune vocal editor", 84, 42, 300, 18, juce::Justification::centredLeft, false);

    // panels
    for (auto r : { leftPanel, rightPanel })
    {
        g.setColour (kPanel.withAlpha (0.92f));
        g.fillRoundedRectangle (r.toFloat(), 14.0f);
        g.setColour (kLine);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 14.0f, 1.0f);
    }
    for (auto& s : sections)
    {
        g.setColour (kAccent);
        g.fillRoundedRectangle ((float) s.second.getX(), (float) s.second.getY() + 2.0f, 3.0f, 12.0f, 1.5f);
        g.setColour (kDim);
        g.setFont (bold (11.5f));
        g.drawText (s.first, s.second.withTrimmedLeft (10), juce::Justification::centredLeft, false);
    }

    // footer
    const auto fb = bold (12.5f);
    const auto fp = plain (12.5f);
    const juce::String a = "Engineered by ", b = "wajfay";
    const float wa = rt::textWidth (fp, a), wb = rt::textWidth (fb, b);
    const float x0 = ((float) w - (wa + wb)) * 0.5f;
    g.setFont (fp);
    g.setColour (kDim);
    g.drawText (a, (int) x0, h - 29, (int) wa + 6, 22, juce::Justification::centredLeft, false);
    g.setFont (fb);
    g.setColour (kAccent);
    g.drawText (b, (int) (x0 + wa), h - 29, (int) wb + 10, 22, juce::Justification::centredLeft, false);

    g.setFont (plain (11.0f));
    g.setColour (kDim.withAlpha (0.7f));
    g.drawText ("Robotic Tune  v1.1", 18, h - 29, 200, 22, juce::Justification::centredLeft, false);
    g.drawText ("VST3  |  Windows  |  FL Studio", w - 18 - 260, h - 29, 260, 22, juce::Justification::centredRight, false);
}

// ----------------------------------------------------------------- layout
void AtlasToneEditor::resized()
{
    const int W = getWidth(), H = getHeight();
    scale = juce::jlimit (0.7f, 2.4f, std::min ((float) W / 1240.0f, (float) H / 740.0f));
    const int cw = (int) std::ceil ((float) W / scale), ch = (int) std::ceil ((float) H / scale);
    canvas.setTransform (juce::AffineTransform());
    canvas.setBounds (0, 0, cw, ch);
    canvas.setTransform (juce::AffineTransform::scale (scale));

    // header
    recBtn.setBounds (394, 14, 44, 44);
    importBtn.setBounds (448, 17, 150, 38);
    previewBtn.setBounds (608, 14, 44, 44);
    int x = cw - 16;
    bypassBtn.setBounds (x - 150, 22, 150, 28);
    x -= 162;
    for (auto* b : { &settingsBtn, &fitBtn, &redoBtn, &undoBtn })
    {
        b->setBounds (x - 38, 17, 38, 38);
        x -= 46;
    }
    statusLabel.setBounds (666, 14, std::max (100, x - 666), 44);

    const int top = 80, bottom = ch - 36;
    leftPanel  = { 12, top, 224, bottom - top };
    rightPanel = { cw - 12 - 280, top, 280, bottom - top };
    const int rollX = leftPanel.getRight() + 10;
    roll.setBounds (rollX, top, rightPanel.getX() - 10 - rollX, bottom - top);

    sections.clear();

    // ---- left column
    {
        const int x0 = leftPanel.getX() + 16, w = leftPanel.getWidth() - 32;
        const int half = (w - 8) / 2;
        int y = top + 14;

        sections.push_back ({ "TOOLS", { x0, y, w, 16 } });
        y += 24;
        for (auto* b : { &drawBtn, &selectBtn, &scissorsBtn, &trillBtn })
        {
            b->setBounds (x0, y, w, 42);
            y += 48;
        }
        y += 8;

        sections.push_back ({ "EDIT NOTES", { x0, y, w, 16 } });
        y += 24;
        autoSnapBtn.setBounds (x0, y, half, 32);          detectNotesBtn.setBounds (x0 + half + 8, y, half, 32);  y += 38;
        mergeBtn.setBounds (x0, y, half, 32);             deleteBtn.setBounds (x0 + half + 8, y, half, 32);       y += 38;
        harmonyBtn.setBounds (x0, y, half, 32);           clearHarmBtn.setBounds (x0 + half + 8, y, half, 32);    y += 38;
        clearAllBtn.setBounds (x0, y, w, 32);             y += 46;

        sections.push_back ({ "BACK TO THE FL TRACK", { x0, y, w, 16 } });
        y += 24;
        dragWav.setBounds (x0, y, w, 42);                 y += 48;
        saveWavBtn.setBounds (x0, y, half, 32);           saveMidiBtn.setBounds (x0 + half + 8, y, half, 32);     y += 38;
        dragMidi.setBounds (x0, y, w, 32);
    }

    // ---- right column
    {
        const int x0 = rightPanel.getX() + 18, w = rightPanel.getWidth() - 36;
        const int half = (w - 8) / 2;
        int y = top + 14;

        sections.push_back ({ "KEY & SCALE", { x0, y, w, 16 } });
        y += 24;
        capKey.setBounds (x0, y, 100, 14);                capScale.setBounds (x0 + 108, y, w - 108, 14);
        keyBox.setBounds (x0, y + 15, 100, 30);           scaleBox.setBounds (x0 + 108, y + 15, w - 108, 30);
        y += 54;
        snapBtn.setBounds (x0, y, 130, 28);               detectKeyBtn.setBounds (x0 + 138, y, w - 138, 28);
        y += 44;

        auto sliderRow = [&] (juce::Label& cap, juce::Slider& s)
        {
            cap.setBounds (x0, y, w, 14);
            s.setBounds (x0, y + 15, w, 28);
            y += 50;
        };

        sections.push_back ({ "ROBOTIC ENGINE", { x0, y, w, 16 } });
        y += 24;
        sliderRow (capRobotic, roboticSl);
        sliderRow (capVar, variationSl);
        sliderRow (capGlide, glideSl);
        y += 6;

        sections.push_back ({ "LEVELS & HARMONY", { x0, y, w, 16 } });
        y += 24;
        sliderRow (capLead, leadSl);
        capHInt.setBounds (x0, y, w, 14);
        harmBox.setBounds (x0, y + 15, w, 30);
        y += 52;
        sliderRow (capHLvl, harmSl);
        y += 6;

        sections.push_back ({ "TRILL", { x0, y, w, 16 } });
        y += 24;
        capTrRate.setBounds (x0, y, half, 14);            capTrInt.setBounds (x0 + half + 8, y, half, 14);
        trillRateBox.setBounds (x0, y + 15, half, 30);    trillIntBox.setBounds (x0 + half + 8, y + 15, half, 30);
    }
}
