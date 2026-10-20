#include "PianoRoll.h"

namespace
{
const juce::Colour kBg (0xff070a13), kBandIn (0xff101830), kBandOut (0xff0a0f1c), kGrid (0xff18223c), kGridOct (0xff2a3658);
const juce::Colour kPanel (0xff0d1424), kText (0xffaab5cf);
const juce::Colour kLead = rt::kAccent, kHarm = rt::kAccent2, kOut = rt::kRed;

bool isBlackKey (int midi) { const int pc = ((midi % 12) + 12) % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }
}

PianoRoll::PianoRoll (AtlasToneProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    startTimerHz (30);
}

void PianoRoll::setTool (Tool t)
{
    tool = t;
    setMouseCursor (t == Tool::Select ? juce::MouseCursor::NormalCursor : juce::MouseCursor::CrosshairCursor);
    repaint();
}

int PianoRoll::xToFrame (float x, const Analysis& a) const
{
    return (int) std::floor ((xToSec (x) * a.sampleRate + a.hop * 0.5) / (double) a.hop);
}

juce::Rectangle<float> PianoRoll::noteRect (const Note& n, const Analysis& a) const
{
    const float x0 = secToX (a.frameSec ((double) n.start));
    const float x1 = secToX (a.frameSec ((double) n.end));
    const float h = std::max (6.0f, rowH() * 0.78f);
    return { x0, pitchToY (n.target) - h * 0.5f, std::max (3.0f, x1 - x0), h };
}

int PianoRoll::hitTest (const std::vector<Note>& ns, const Analysis& a, juce::Point<float> p) const
{
    for (int i = (int) ns.size() - 1; i >= 0; --i)
        if (noteRect (ns[(size_t) i], a).expanded (0.0f, 2.0f).contains (p)) return i;
    return -1;
}

float PianoRoll::pitchFromY (float y) const
{
    const float p = yToPitch (y);
    return proc.snapEnabled() ? proc.currentScale().snap (p) : p;
}

void PianoRoll::fitToNotes()
{
    const auto a = proc.getAnalysis();
    const auto ns = proc.getNotes();
    if (! a) return;

    float lo = 200.0f, hi = -200.0f;
    int first = 0, last = a->numFrames();
    if (! ns.empty())
    {
        first = ns.front().start; last = ns.front().end;
        for (auto& n : ns)
        {
            lo = std::min ({ lo, n.target, n.center });
            hi = std::max ({ hi, n.target, n.center });
            first = std::min (first, n.start);
            last = std::max (last, n.end);
        }
    }
    else
    {
        // free draw with no notes yet: frame the detected vocal pitch instead
        bool any = false;
        for (int f = 0; f < a->numFrames(); ++f)
            if (a->f0[(size_t) f] > 0.0f) { lo = std::min (lo, a->f0[(size_t) f]); hi = std::max (hi, a->f0[(size_t) f]); any = true; }
        if (! any) return;
    }
    lo -= 4.0f; hi += 4.0f;
    if (hi - lo < 18.0f) { const float mid = 0.5f * (hi + lo); lo = mid - 9.0f; hi = mid + 9.0f; }
    viewLow = std::max (0.0f, lo);
    viewHigh = std::min (127.0f, hi);
    const double dur = std::max (4.0, a->frameSec ((double) last) - a->frameSec ((double) first));
    pxPerSec = juce::jlimit (10.0, 400.0, (double) (getWidth() - keysW - 30) / dur);
    viewStartSec = std::max (0.0, a->frameSec ((double) first) - 0.3);
}

void PianoRoll::timerCallback()
{
    bool needRepaint = false;
    const int nv = proc.notesVersion.load(), av = proc.analysisVersion.load(), rv = proc.renderVersion.load();
    if (av != lastAnalysisVer)
    {
        lastAnalysisVer = av;
        selection.clear();
        if (proc.hasTake.load()) fitToNotes();
        needRepaint = true;
    }
    if (nv != lastNotesVer || rv != lastRenderVer) { lastNotesVer = nv; lastRenderVer = rv; needRepaint = true; }

    const bool playing = proc.hostPlaying.load() || proc.previewPlaying.load();
    const bool following = proc.apvts.getRawParameterValue ("follow")->load() > 0.5f;
    if (playing && (proc.hasTake.load() || proc.recording.load()))
    {
        const double t = proc.playheadSeconds();
        const float x = secToX (t);
        if (following && (x > (float) getWidth() - 40.0f || x < (float) keysW)) viewStartSec = t - 1.0;
        needRepaint = true;
    }
    if (playing != lastPlaying) { lastPlaying = playing; needRepaint = true; }
    if (proc.armed.load() || proc.recording.load() || proc.analysing.load()) needRepaint = true;
    if (needRepaint) repaint();
}

// ------------------------------------------------------------------ paint
void PianoRoll::paint (juce::Graphics& g)
{
    const auto a = proc.getAnalysis();
    const auto ns = proc.getNotes();
    const auto sc = proc.currentScale();
    const float robotic = juce::jlimit (0.0f, 1.0f, proc.apvts.getRawParameterValue ("robotic")->load() * 0.01f);
    const float variation = proc.apvts.getRawParameterValue ("variation")->load() * (1.0f - robotic);
    const int W = getWidth(), H = getHeight();

    {
        juce::Path clip;
        clip.addRoundedRectangle (getLocalBounds().toFloat(), 12.0f);
        g.reduceClipRegion (clip);
    }
    g.fillAll (kBg);

    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (keysW, rulerH, W - keysW, H - rulerH);

        const int lo = (int) std::floor (viewLow), hi = (int) std::ceil (viewHigh);
        for (int n = lo; n <= hi; ++n)
        {
            const float y0 = pitchToY ((float) n + 0.5f), y1 = pitchToY ((float) n - 0.5f);
            g.setColour (sc.contains (n) ? kBandIn : kBandOut);
            g.fillRect ((float) keysW, y0, (float) (W - keysW), y1 - y0);
            g.setColour (n % 12 == 0 ? kGridOct : kGrid);
            g.drawHorizontalLine ((int) y1, (float) keysW, (float) W);
        }

        static const double steps[] = { 0.05, 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 30.0, 60.0 };
        double stepSec = 60.0;
        for (double s : steps) if (s * pxPerSec >= 70.0) { stepSec = s; break; }
        g.setColour (kGrid);
        for (double t = std::floor (viewStartSec / stepSec) * stepSec; secToX (t) < (float) W; t += stepSec)
            g.drawVerticalLine ((int) secToX (t), (float) rulerH, (float) H);

        if (a)
        {
            const int nf = a->numFrames();
            const float midY = (float) rulerH + (float) (H - rulerH) * 0.5f;
            const float amp = (float) (H - rulerH) * 0.34f;
            const int f0 = juce::jlimit (0, nf - 1, xToFrame ((float) keysW, *a) - 1);
            const int f1 = juce::jlimit (0, nf - 1, xToFrame ((float) W, *a) + 1);

            // waveform
            g.setColour (kLead.withAlpha (0.10f));
            for (int f = f0; f <= f1; ++f)
            {
                const float h = std::sqrt (a->peak[(size_t) f]) * amp;
                g.drawVerticalLine ((int) secToX (a->frameSec ((double) f)), midY - h, midY + h);
            }

            // detected pitch contour
            juce::Path contour;
            bool pen = false;
            for (int f = f0; f <= f1; ++f)
            {
                const float p = a->f0[(size_t) f];
                if (p <= 0.0f) { pen = false; continue; }
                const float x = secToX (a->frameSec ((double) f)), y = pitchToY (p);
                if (! pen) { contour.startNewSubPath (x, y); pen = true; } else contour.lineTo (x, y);
            }
            g.setColour (juce::Colours::white.withAlpha (0.26f));
            g.strokePath (contour, juce::PathStrokeType (1.0f));

            // robotic transitions between neighbouring lead notes
            {
                std::vector<const Note*> leads;
                for (auto& n : ns) if (n.layer == 0) leads.push_back (&n);
                std::sort (leads.begin(), leads.end(), [] (const Note* x, const Note* y) { return x->start < y->start; });
                for (size_t i = 0; i + 1 < leads.size(); ++i)
                {
                    const Note& cur = *leads[i];
                    const Note& nxt = *leads[i + 1];
                    if (nxt.start - cur.end > 3) continue;
                    const float x0 = secToX (a->frameSec ((double) cur.end)), x1 = secToX (a->frameSec ((double) nxt.start));
                    const float y0 = pitchToY (cur.target), y1 = pitchToY (nxt.target);
                    juce::Path st;
                    st.startNewSubPath (x0, y0);
                    if (robotic >= 0.5f) { st.lineTo (x1, y0); st.lineTo (x1, y1); }
                    else st.lineTo (x1, y1);
                    g.setColour (rt::kAccent.withAlpha (0.55f));
                    g.strokePath (st, juce::PathStrokeType (1.6f));
                }
            }

            // notes
            for (auto& n : ns)
            {
                const auto r = noteRect (n, *a);
                if (r.getRight() < (float) keysW || r.getX() > (float) W) continue;
                const bool inKey = sc.isInKey (n.target);
                const juce::Colour base = ! inKey ? kOut : (n.layer == 1 ? kHarm : kLead);
                const bool sel = selection.count (n.id) > 0;

                if (sel)
                {
                    g.setColour (base.withAlpha (0.20f));
                    g.fillRoundedRectangle (r.expanded (3.0f), 7.0f);
                }
                g.setGradientFill (juce::ColourGradient (base.brighter (0.25f).withAlpha (sel ? 0.95f : 0.78f), r.getX(), r.getY(),
                                                         base.darker (0.35f).withAlpha (sel ? 0.95f : 0.68f), r.getX(), r.getBottom(), false));
                g.fillRoundedRectangle (r, 5.0f);
                g.setColour (sel ? juce::Colours::white : base.brighter (0.5f).withAlpha (0.9f));
                g.drawRoundedRectangle (r, 5.0f, sel ? 1.8f : 1.0f);

                // target contour inside the note (flat when Robotic is high)
                juce::Path tp;
                bool tpen = false;
                for (int f = std::max (0, n.start); f < std::min (nf, n.end); ++f)
                {
                    const float src = a->f0[(size_t) f] > 0.0f ? a->f0[(size_t) f] : n.center;
                    const float t = n.target + variation * (src - n.center);
                    const float x = secToX (a->frameSec ((double) f)), y = pitchToY (t);
                    if (! tpen) { tp.startNewSubPath (x, y); tpen = true; } else tp.lineTo (x, y);
                }
                g.setColour (juce::Colours::white.withAlpha (0.9f));
                g.strokePath (tp, juce::PathStrokeType (1.4f));

                if (r.getWidth() > 30.0f && r.getHeight() > 9.0f)
                {
                    g.setColour (juce::Colour (0xff04101c).withAlpha (0.9f));
                    g.setFont (juce::jmin (13.0f, r.getHeight() - 2.0f));
                    g.drawText (atlas::midiName ((int) std::lround (n.target)), r.reduced (5.0f, 0.0f), juce::Justification::topLeft, false);
                }
            }
        }

        if (drag == Drag::Band)
        {
            const auto br = juce::Rectangle<float> (bandStart, bandEnd);
            g.setColour (rt::kAccent.withAlpha (0.08f));
            g.fillRect (br);
            g.setColour (rt::kAccent.withAlpha (0.6f));
            g.drawRect (br, 1.0f);
        }

        if (tool == Tool::Scissors && isMouseOver())
        {
            g.setColour (rt::kAccent.withAlpha (0.8f));
            const float dashes[] = { 4.0f, 3.0f };
            g.drawDashedLine (juce::Line<float> (mousePos.x, (float) rulerH, mousePos.x, (float) H), dashes, 2, 1.2f);
            rt::drawIcon (g, rt::Icon::Scissors, { mousePos.x + 6.0f, mousePos.y - 26.0f, 22.0f, 22.0f }, juce::Colours::white, 1.8f);
        }

        if (! a)
        {
            const auto area = getLocalBounds().withTrimmedLeft (keysW).withTrimmedTop (rulerH).toFloat();
            const auto card = juce::Rectangle<float> (440.0f, 150.0f).withCentre (area.getCentre());
            g.setColour (rt::kPanelHi.withAlpha (0.85f));
            g.fillRoundedRectangle (card, 16.0f);
            g.setColour (rt::kLine);
            g.drawRoundedRectangle (card.reduced (0.5f), 16.0f, 1.0f);

            juce::String title = "Drop a vocal here";
            juce::String sub = "or press IMPORT VOCAL.  You can also record: press REC and play the track from FL Studio.";
            if (proc.armed.load())     { title = "ARMED";      sub = "Press PLAY in FL Studio and let the vocal run to the end, then stop."; }
            if (proc.recording.load()) { title = "RECORDING";  sub = "Stop the playback in FL Studio when the vocal is finished."; }
            if (proc.analysing.load()) { title = "Analysing";  sub = "Reading the pitch of your vocal..."; }

            rt::drawIcon (g, proc.armed.load() || proc.recording.load() ? rt::Icon::Record : rt::Icon::Import,
                          card.withTrimmedBottom (90.0f).withTrimmedTop (14.0f), rt::kAccent, 2.2f);
            g.setColour (rt::kText);
            g.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
            g.drawText (title, card.withTrimmedTop (62.0f).withHeight (30.0f), juce::Justification::centred, false);
            g.setColour (rt::kDim);
            g.setFont (13.0f);
            g.drawFittedText (sub, card.reduced (24.0f, 0.0f).withTrimmedTop (94.0f).toNearestInt(), juce::Justification::centredTop, 2);
        }
        else if (! proc.trackingEnabled())
        {
            const auto badge = juce::Rectangle<float> (190.0f, 24.0f).withRightX ((float) W - 12.0f).withY ((float) rulerH + 10.0f);
            g.setColour (rt::kAccent2.withAlpha (0.25f));
            g.fillRoundedRectangle (badge, 12.0f);
            g.setColour (rt::kAccent2.brighter (0.4f));
            g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
            g.drawText ("FREE DRAW  -  tracking off", badge, juce::Justification::centred, false);
        }

        if (proc.hasTake.load() || proc.recording.load())
        {
            const float x = secToX (proc.playheadSeconds());
            if (x >= (float) keysW && x <= (float) W)
            {
                g.setColour (juce::Colours::white.withAlpha (0.85f));
                g.drawVerticalLine ((int) x, (float) rulerH, (float) H);
            }
        }
    }

    // piano keys
    g.setColour (kPanel);
    g.fillRect (0, rulerH, keysW, H - rulerH);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (0, rulerH, keysW, H - rulerH);
        const int lo = (int) std::floor (viewLow), hi = (int) std::ceil (viewHigh);
        for (int n = lo; n <= hi; ++n)
        {
            const float y0 = pitchToY ((float) n + 0.5f), y1 = pitchToY ((float) n - 0.5f);
            g.setColour (isBlackKey (n) ? juce::Colour (0xff141a2b) : juce::Colour (0xff27304a));
            g.fillRoundedRectangle (2.0f, y0 + 0.5f, (float) keysW - 5.0f, y1 - y0 - 1.0f, 2.5f);
            if (rowH() >= 13.0f || n % 12 == 0)
            {
                g.setColour (sc.contains (n) ? juce::Colours::white.withAlpha (0.88f) : kText.withAlpha (0.40f));
                g.setFont (juce::jmin (11.0f, rowH() - 1.0f));
                g.drawText (atlas::midiName (n), 7, (int) y0, keysW - 10, (int) (y1 - y0), juce::Justification::centredLeft, false);
            }
        }
    }

    // ruler
    g.setColour (kPanel);
    g.fillRect (0, 0, W, rulerH);
    g.setColour (rt::kLine);
    g.drawHorizontalLine (rulerH - 1, 0.0f, (float) W);
    {
        static const double steps[] = { 0.05, 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 30.0, 60.0 };
        double stepSec = 60.0;
        for (double s : steps) if (s * pxPerSec >= 70.0) { stepSec = s; break; }
        g.setColour (kText);
        g.setFont (11.0f);
        for (double t = std::floor (viewStartSec / stepSec) * stepSec; secToX (t) < (float) W; t += stepSec)
        {
            const float x = secToX (t);
            if (x < (float) keysW || t < 0.0) continue;
            const int total = (int) std::floor (t * 10.0 + 0.5);
            g.drawText (juce::String (total / 600) + ":" + juce::String (total / 10 % 60).paddedLeft ('0', 2)
                        + (stepSec < 1.0 ? "." + juce::String (total % 10) : juce::String()),
                        (int) x + 4, 0, 60, rulerH, juce::Justification::centredLeft, false);
            g.drawVerticalLine ((int) x, (float) rulerH - 9.0f, (float) rulerH);
        }

        if (proc.hasTake.load())
        {
            const float x = secToX (proc.playheadSeconds());
            if (x >= (float) keysW && x <= (float) W)
            {
                juce::Path tri;
                tri.addTriangle (x - 6.0f, 6.0f, x + 6.0f, 6.0f, x, (float) rulerH - 3.0f);
                g.setColour (rt::kAccent);
                g.fillPath (tri);
            }
        }
    }

    if (proc.recording.load())
    {
        g.setColour (kOut);
        g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
        const double sec = (double) proc.recWriteMax.load() / std::max (1.0, proc.takeSampleRate.load());
        g.drawText (juce::String::formatted ("REC  %02d:%04.1f", (int) (sec / 60.0), std::fmod (sec, 60.0)),
                    W - 160, 0, 150, rulerH, juce::Justification::centredRight, false);
    }

    g.setColour (rt::kLine);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 12.0f, 1.0f);
}

// ------------------------------------------------------------------ mouse
void PianoRoll::mouseMove (const juce::MouseEvent& e)
{
    mousePos = e.position;
    if (tool == Tool::Scissors) repaint();
}

void PianoRoll::paintAt (int f0, int f1, float pitch)
{
    const int lo = std::min (f0, f1), hi = std::max (f0, f1);
    proc.editNotes ([&] (std::vector<Note>& v)
    {
        for (auto& n : v)
            if (n.layer == 0 && n.end > lo && n.start <= hi)
                n.target = pitch;
    }, false);
}

// Free draw: lays down lead notes under the cursor (replacing what was there).
// Same pitch as the previous step extends the current note, a new pitch starts a new one,
// so dragging up and down paints hard robotic steps.
void PianoRoll::freeDrawStep (int fa, int fb, float pitch)
{
    const int lo = std::min (fa, fb), hi = std::max (fa, fb) + 1;   // hi is exclusive
    const int curId = drawNoteId;
    const bool samePitch = curId >= 0 && std::abs (pitch - drawPitch) < 0.001f;
    const int freshId = samePitch ? -1 : proc.newId();

    proc.editNotes ([&] (std::vector<Note>& v)
    {
        std::vector<Note> extra;
        for (auto& n : v)
        {
            if (n.layer != 0) continue;
            if (samePitch && n.id == curId) continue;
            if (n.end <= lo || n.start >= hi) continue;
            if (n.start < lo && n.end > hi) { Note r = n; r.id = proc.newId(); r.start = hi; n.end = lo; extra.push_back (r); }
            else if (n.start < lo) n.end = lo;
            else if (n.end > hi) n.start = hi;
            else n.end = n.start;   // fully covered, removed below
        }
        v.erase (std::remove_if (v.begin(), v.end(), [] (const Note& n) { return n.layer == 0 && n.end <= n.start; }), v.end());
        v.insert (v.end(), extra.begin(), extra.end());

        if (samePitch)
        {
            for (auto& n : v)
                if (n.id == curId) { n.start = std::min (n.start, lo); n.end = std::max (n.end, hi); }
        }
        else
        {
            Note nn;
            nn.id = freshId; nn.start = lo; nn.end = hi; nn.center = pitch; nn.target = pitch; nn.layer = 0;
            v.push_back (nn);
        }
    }, false);

    if (! samePitch) { drawNoteId = freshId; drawPitch = pitch; }
    drawMinF = std::min (drawMinF, lo);
    drawMaxF = std::max (drawMaxF, hi);
}

void PianoRoll::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    mousePos = e.position;

    if (e.mods.isRightButtonDown() || e.mods.isMiddleButtonDown())
    {
        drag = Drag::Pan;
        panStart = e.position;
        panStartSec = viewStartSec;
        panStartLow = viewLow;
        panStartHigh = viewHigh;
        return;
    }

    // click / drag on the ruler = move the preview playhead
    if (e.position.y < (float) rulerH)
    {
        if (e.position.x >= (float) keysW && proc.hasTake.load() && ! proc.hostPlaying.load())
        {
            drag = Drag::Seek;
            proc.seekPreview (xToSec (e.position.x));
            repaint();
        }
        return;
    }

    const auto a = proc.getAnalysis();
    if (! a) return;
    auto ns = proc.getNotes();
    const int f = xToFrame (e.position.x, *a);
    const float pitch = pitchFromY (e.position.y);
    const int hit = hitTest (ns, *a, e.position);

    switch (tool)
    {
        case Tool::Draw:
        {
            proc.beginEdit();

            if (! proc.trackingEnabled())
            {
                const int fc = juce::jlimit (0, a->numFrames() - 1, f);
                drag = Drag::FreeDraw;
                lastFrame = fc;
                drawNoteId = -1;
                drawMinF = fc;
                drawMaxF = fc + 1;
                freeDrawStep (fc, fc, pitch);
                break;
            }

            bool inLead = false;
            for (auto& n : ns) if (n.layer == 0 && f >= n.start && f < n.end) inLead = true;
            if (inLead)
            {
                drag = Drag::Paint;
                lastFrame = f;
                paintAt (f, f, pitch);
            }
            else
            {
                drag = Drag::Create;
                createStart = f;
                activeId = proc.newId();
                const int id = activeId;
                proc.editNotes ([&] (std::vector<Note>& v)
                {
                    Note n; n.id = id; n.start = f; n.end = f + 1; n.center = pitch; n.target = pitch; n.layer = 0;
                    v.push_back (n);
                }, false);
            }
            break;
        }
        case Tool::Select:
        {
            if (hit >= 0)
            {
                const Note& n = ns[(size_t) hit];
                const auto r = noteRect (n, *a);
                activeId = n.id;
                if (e.mods.isShiftDown()) { if (selection.count (n.id)) selection.erase (n.id); else selection.insert (n.id); }
                else if (! selection.count (n.id)) { selection.clear(); selection.insert (n.id); }

                proc.beginEdit();
                if (std::abs (e.position.x - r.getX()) < 5.0f && r.getWidth() > 14.0f) drag = Drag::ResizeL;
                else if (std::abs (e.position.x - r.getRight()) < 5.0f && r.getWidth() > 14.0f) drag = Drag::ResizeR;
                else
                {
                    drag = Drag::Move;
                    dragStartPitch = yToPitch (e.position.y);
                    origTarget.clear();
                    for (auto& m : ns) if (selection.count (m.id)) origTarget[m.id] = m.target;
                }
            }
            else
            {
                if (! e.mods.isShiftDown()) selection.clear();
                drag = Drag::Band;
                bandStart = bandEnd = e.position;
            }
            break;
        }
        case Tool::Scissors:
            if (hit >= 0) proc.splitNote (ns[(size_t) hit].id, f);
            break;

        case Tool::Trill:
            if (hit >= 0) proc.applyTrill (ns[(size_t) hit].id);
            break;
    }
    repaint();
}

void PianoRoll::mouseDrag (const juce::MouseEvent& e)
{
    mousePos = e.position;
    const auto a = proc.getAnalysis();

    if (drag == Drag::Pan)
    {
        const auto d = e.position - panStart;
        viewStartSec = std::max (-1.0, panStartSec - (double) d.x / pxPerSec);
        const double dp = (double) d.y / (double) rowH();
        viewLow = panStartLow + dp;
        viewHigh = panStartHigh + dp;
        repaint();
        return;
    }
    if (drag == Drag::Seek)
    {
        proc.seekPreview (xToSec (e.position.x));
        repaint();
        return;
    }
    if (! a) return;
    const int f = xToFrame (e.position.x, *a);

    switch (drag)
    {
        case Drag::Paint:
            paintAt (lastFrame, f, pitchFromY (e.position.y));
            lastFrame = f;
            break;

        case Drag::FreeDraw:
        {
            const int fc = juce::jlimit (0, a->numFrames() - 1, f);
            freeDrawStep (lastFrame, fc, pitchFromY (e.position.y));
            lastFrame = fc;
            break;
        }
        case Drag::Create:
        {
            const int id = activeId, s = createStart;
            proc.editNotes ([&] (std::vector<Note>& v)
            {
                for (auto& n : v) if (n.id == id) { n.start = std::min (s, f); n.end = std::max (s, f) + 1; }
            }, false);
            break;
        }
        case Drag::Move:
        {
            const float delta = yToPitch (e.position.y) - dragStartPitch;
            const auto sc = proc.currentScale();
            const bool snap = proc.snapEnabled();
            proc.editNotes ([&] (std::vector<Note>& v)
            {
                for (auto& n : v)
                {
                    auto it = origTarget.find (n.id);
                    if (it == origTarget.end()) continue;
                    const float t = it->second + delta;
                    n.target = snap ? sc.snap (t) : t;
                }
            }, false);
            break;
        }
        case Drag::ResizeL:
        case Drag::ResizeR:
        {
            const int id = activeId;
            const bool left = drag == Drag::ResizeL;
            proc.editNotes ([&] (std::vector<Note>& v)
            {
                for (auto& n : v)
                    if (n.id == id)
                    {
                        if (left) n.start = std::min (f, n.end - 2);
                        else      n.end = std::max (f, n.start + 2);
                    }
            }, false);
            break;
        }
        case Drag::Band:
            bandEnd = e.position;
            break;
        default: break;
    }
    repaint();
}

void PianoRoll::mouseUp (const juce::MouseEvent&)
{
    const auto a = proc.getAnalysis();
    if (a)
    {
        if (drag == Drag::Create)
        {
            const int id = activeId;
            proc.editNotes ([&] (std::vector<Note>& v)
            {
                for (size_t i = 0; i < v.size(); ++i)
                    if (v[i].id == id)
                    {
                        if (v[i].end - v[i].start < 3) { v.erase (v.begin() + (long) i); }
                        else v[i].center = atlas::medianRange (*a, v[i].start, v[i].end, v[i].target);
                        break;
                    }
            }, false);
        }
        else if (drag == Drag::FreeDraw)
        {
            const int lo = drawMinF, hi = drawMaxF;
            proc.editNotes ([&] (std::vector<Note>& v)
            {
                for (auto& n : v)
                    if (n.layer == 0 && n.end > lo && n.start < hi)
                        n.center = atlas::medianRange (*a, n.start, n.end, n.target);
            }, false);
        }
        else if (drag == Drag::ResizeL || drag == Drag::ResizeR)
        {
            const int id = activeId;
            proc.editNotes ([&] (std::vector<Note>& v)
            {
                for (auto& n : v) if (n.id == id) n.center = atlas::medianRange (*a, n.start, n.end, n.center);
            }, false);
        }
        else if (drag == Drag::Band)
        {
            const auto br = juce::Rectangle<float> (bandStart, bandEnd);
            for (auto& n : proc.getNotes())
                if (noteRect (n, *a).intersects (br)) selection.insert (n.id);
        }
    }
    drag = Drag::None;
    repaint();
}

void PianoRoll::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isShiftDown())
        viewStartSec = std::max (-1.0, viewStartSec - (double) (w.deltaY + w.deltaX) * 300.0 / pxPerSec);
    else if (e.mods.isAltDown())
    {
        const double mid = (double) yToPitch (e.position.y);
        const double range = juce::jlimit (12.0, 90.0, (viewHigh - viewLow) * (1.0 - (double) w.deltaY * 0.6));
        const double frac = (viewHigh - mid) / (viewHigh - viewLow);
        viewHigh = mid + frac * range;
        viewLow = viewHigh - range;
    }
    else
    {
        const double before = xToSec (e.position.x);
        pxPerSec = juce::jlimit (8.0, 1500.0, pxPerSec * (1.0 + (double) w.deltaY * 0.8));
        viewStartSec = before - (double) (e.position.x - (float) keysW) / pxPerSec;
        if (w.deltaX != 0.0f) viewStartSec -= (double) w.deltaX * 300.0 / pxPerSec;
    }
    repaint();
}

bool PianoRoll::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey)
    {
        proc.deleteNotes (getSelection());
        selection.clear();
        return true;
    }
    if (k == juce::KeyPress::spaceKey) { proc.togglePreview(); return true; }
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)) { proc.undo(); return true; }
    if (k == juce::KeyPress ('y', juce::ModifierKeys::commandModifier, 0)
        || k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)) { proc.redo(); return true; }

    if (onToolRequested)
    {
        const auto c = juce::CharacterFunctions::toLowerCase (k.getTextCharacter());
        if (c == 'd') { onToolRequested (Tool::Draw);     return true; }
        if (c == 'v') { onToolRequested (Tool::Select);   return true; }
        if (c == 'c') { onToolRequested (Tool::Scissors); return true; }
        if (c == 't') { onToolRequested (Tool::Trill);    return true; }
    }
    return false;
}
