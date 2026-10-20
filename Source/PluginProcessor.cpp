#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr double kMaxRecordSeconds = 300.0;

class WorkerThread : public juce::Thread
{
public:
    explicit WorkerThread (AtlasToneProcessor& o) : juce::Thread ("RoboticTuneWorker"), owner (o) {}
    void run() override
    {
        while (! threadShouldExit())
        {
            owner.workerTick();
            wait (25);
        }
    }
private:
    AtlasToneProcessor& owner;
};
}

// ------------------------------------------------------------- parameters
juce::AudioProcessorValueTreeState::ParameterLayout AtlasToneProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    StringArray keys;
    for (int i = 0; i < 12; ++i) keys.add (atlas::noteNames()[i]);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key", keys, 0));

    StringArray scales;
    for (auto& s : atlas::scaleDefs()) scales.add (s.name);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", scales, 0));

    l.add (std::make_unique<AudioParameterBool> (ParameterID { "snap", 1 }, "Snap to Scale", true));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "variation", 1 }, "Variation",
                                                  NormalisableRange<float> (0.0f, 2.0f, 0.01f), 1.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "glide", 1 }, "Glide",
                                                  NormalisableRange<float> (0.0f, 150.0f, 1.0f), 25.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "leadGain", 1 }, "Lead Level",
                                                  NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "harmGain", 1 }, "Harmony Level",
                                                  NormalisableRange<float> (-24.0f, 6.0f, 0.1f), -6.0f));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Original (A/B)", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "harmInterval", 1 }, "Harmony Interval",
                                                   StringArray { "+3rd", "+5th", "-3rd", "-5th", "+Octave", "-Octave" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "trillRate", 1 }, "Trill Rate",
                                                   StringArray { "1/8", "1/16", "1/32", "1/64" }, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "trillInt", 1 }, "Trill Interval",
                                                   StringArray { "+1 semitone", "+2 semitones", "+1 scale step",
                                                                 "-1 semitone", "-2 semitones", "-1 scale step" }, 2));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "robotic", 1 }, "Robotic",
                                                  NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "tracking", 1 }, "Note Tracking", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "autoKey", 1 }, "Auto Detect Key", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "follow", 1 }, "Follow Playhead", true));
    return l;
}

AtlasToneProcessor::AtlasToneProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ROBOTICTUNE", createLayout())
{
    apvts.addParameterListener ("variation", this);
    apvts.addParameterListener ("glide", this);
    apvts.addParameterListener ("robotic", this);
    worker = std::make_unique<WorkerThread> (*this);
    worker->startThread();
}

AtlasToneProcessor::~AtlasToneProcessor()
{
    abortRender = true;
    apvts.removeParameterListener ("variation", this);
    apvts.removeParameterListener ("glide", this);
    apvts.removeParameterListener ("robotic", this);
    worker->stopThread (10000);
}

bool AtlasToneProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo()) && o == l.getMainInputChannelSet();
}

atlas::Scale AtlasToneProcessor::currentScale() const
{
    return atlas::makeScale ((int) apvts.getRawParameterValue ("key")->load(),
                             (int) apvts.getRawParameterValue ("scale")->load());
}

bool AtlasToneProcessor::snapEnabled() const { return apvts.getRawParameterValue ("snap")->load() > 0.5f; }
bool AtlasToneProcessor::trackingEnabled() const { return apvts.getRawParameterValue ("tracking")->load() > 0.5f; }
bool AtlasToneProcessor::autoKeyEnabled() const { return apvts.getRawParameterValue ("autoKey")->load() > 0.5f; }

void AtlasToneProcessor::parameterChanged (const juce::String&, float) { markDirty(); }

void AtlasToneProcessor::markDirty()
{
    abortRender = true;
    lastEditMs = juce::Time::getMillisecondCounter();
    renderDirty = true;
}

void AtlasToneProcessor::setParam (const char* id, int value)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) value));
}

// ------------------------------------------------------------------ audio
void AtlasToneProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    recCap = (int) (kMaxRecordSeconds * sr);
}

void AtlasToneProcessor::armRecording (bool shouldArm)
{
    if (! shouldArm) { armed = false; return; }
    if (recording.load()) return;

    previewPlaying = false;
    abortRender = true;
    {
        std::lock_guard<std::mutex> lg (workMutex);
        if ((int) recBuf.size() < recCap) recBuf.assign ((size_t) recCap, 0.0f);
        {
            juce::ScopedLock sl (dataLock);
            analysis.reset();
            notes.clear();
            undoStack.clear();
            redoStack.clear();
            takeName = "Recording";
        }
        {
            juce::SpinLock::ScopedLockType sl (outLock);
            leadOut.clear();
            harmOut.clear();
            origOut.clear();
        }
    }
    hasTake = false;
    takeLength = 0;
    recWriteMax = 0;
    ++notesVersion;
    ++analysisVersion;
    armed.store (true, std::memory_order_release);
}

void AtlasToneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();

    bool playing = false;
    juce::int64 pos = 0;
    if (auto* ph = getPlayHead())
        if (auto p = ph->getPosition())
        {
            playing = p->getIsPlaying();
            if (auto t = p->getTimeInSamples()) pos = *t;
            else if (auto s = p->getTimeInSeconds()) pos = (juce::int64) (*s * sampleRate);
            if (auto b = p->getBpm()) hostBpm = *b;
        }
    hostPos = pos;
    hostPlaying = playing;

    const bool wantRec = armed.load (std::memory_order_acquire);

    // ---- recording: the vocal passes through to the output while it is captured
    if (wantRec && playing && ! recBuf.empty())
    {
        if (! recording.load())
        {
            takeOrigin = pos;
            takeSampleRate = sampleRate;
            recWriteMax = 0;
            recording = true;
        }
        const juce::int64 org = takeOrigin.load();
        int wmax = recWriteMax.load();
        for (int i = 0; i < n; ++i)
        {
            const juce::int64 idx = pos + i - org;
            if (idx < 0 || idx >= (juce::int64) recBuf.size()) continue;
            float s = 0.0f;
            for (int ch = 0; ch < numCh; ++ch) s += buffer.getReadPointer (ch)[i];
            recBuf[(size_t) idx] = s / (float) std::max (1, numCh);
            if (idx + 1 > wmax) wmax = (int) idx + 1;
        }
        recWriteMax = wmax;
        return;
    }

    if (recording.load())
    {
        recording = false;
        armed = false;
        const int len = recWriteMax.load();
        if (len > 4096)
        {
            takeLength = len;
            needsAnalysis = true;
            analysing = true;
        }
    }

    // ---- playback of the edited take
    const bool bypass = apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    if (playing) previewPlaying = false;   // the host transport always wins

    // built-in preview (host stopped)
    if (previewPlaying.load() && hasTake.load())
    {
        juce::SpinLock::ScopedTryLockType lock (outLock);
        if (lock.isLocked() && ! leadOut.empty())
        {
            const int len = (int) leadOut.size();
            const bool haveHarm = harmOut.size() == leadOut.size();
            const bool haveOrig = origOut.size() == leadOut.size();
            const float lg = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("leadGain")->load());
            const float hg = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("harmGain")->load());
            juce::int64 pp = previewPos.load();
            for (int i = 0; i < n; ++i)
            {
                const juce::int64 idx = pp + i;
                float v = 0.0f;
                if (idx >= 0 && idx < len)
                    v = bypass ? (haveOrig ? origOut[(size_t) idx] : 0.0f)
                               : leadOut[(size_t) idx] * lg + (haveHarm ? harmOut[(size_t) idx] * hg : 0.0f);
                for (int ch = 0; ch < numCh; ++ch) buffer.getWritePointer (ch)[i] = v;
            }
            pp += n;
            if (pp >= len) { previewPlaying = false; pp = 0; }
            previewPos = pp;
        }
        return;
    }

    if (playing && hasTake.load() && ! bypass)
    {
        juce::SpinLock::ScopedTryLockType lock (outLock);
        if (lock.isLocked() && ! leadOut.empty())
        {
            const int len = (int) leadOut.size();
            const bool haveHarm = harmOut.size() == leadOut.size();
            const float lg = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("leadGain")->load());
            const float hg = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("harmGain")->load());
            const juce::int64 org = takeOrigin.load();
            for (int i = 0; i < n; ++i)
            {
                const juce::int64 idx = pos + i - org;
                if (idx < 0 || idx >= len) continue;
                const float v = leadOut[(size_t) idx] * lg + (haveHarm ? harmOut[(size_t) idx] * hg : 0.0f);
                for (int ch = 0; ch < numCh; ++ch) buffer.getWritePointer (ch)[i] = v;
            }
        }
    }
}

// ----------------------------------------------------------------- worker
void AtlasToneProcessor::workerTick()
{
    if (loadAnalysisPending.exchange (false)) doAnalysis (false);
    if (needsAnalysis.exchange (false))       doAnalysis (true);

    if (renderDirty.load() && juce::Time::getMillisecondCounter() - lastEditMs.load() >= 100)
    {
        renderDirty = false;
        doRender();
    }
}

void AtlasToneProcessor::doAnalysis (bool segment)
{
    std::lock_guard<std::mutex> lg (workMutex);
    analysing = true;
    const int N = takeLength.load();
    if (N < 2048 || recBuf.size() < (size_t) N)
    {
        hasTake = false;
        analysing = false;
        return;
    }

    auto a = atlas::analyse (recBuf.data(), N, takeSampleRate.load());
    std::vector<Note> fresh;
    if (segment && trackingEnabled()) fresh = atlas::segmentNotes (*a, nextId);   // tracking off = free draw, start empty

    {
        juce::ScopedLock sl (dataLock);
        analysis = a;
        if (segment)
        {
            notes = fresh;
            undoStack.clear();
            redoStack.clear();
        }
    }
    {
        juce::SpinLock::ScopedLockType sl (outLock);
        origOut.assign (recBuf.begin(), recBuf.begin() + (long) N);
    }
    hasTake = true;
    analysing = false;
    ++analysisVersion;
    ++notesVersion;
    if (segment && ! fresh.empty() && autoKeyEnabled()) detectAndSetKey();
    markDirty();
}

void AtlasToneProcessor::detectAndSetKey()
{
    const auto ks = atlas::detectKey (getNotes());
    if (ks.first < 0) return;
    juce::WeakReference<AtlasToneProcessor> weak (this);
    const int k = ks.first, s = ks.second;
    juce::MessageManager::callAsync ([weak, k, s]
    {
        if (auto* p = weak.get())
        {
            p->setParam ("key", k);
            p->setParam ("scale", s);
        }
    });
}

void AtlasToneProcessor::doRender()
{
    std::lock_guard<std::mutex> lg (workMutex);
    if (armed.load() || recording.load() || ! hasTake.load()) return;
    const auto a = getAnalysis();
    const int N = takeLength.load();
    if (! a || N < 2048 || recBuf.size() < (size_t) N) return;

    const auto ns = getNotes();
    // Robotic = 100 %: every note is one flat pitch and notes jump instantly (hard-tune steps).
    const float robotic = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("robotic")->load() * 0.01f);
    const float var = apvts.getRawParameterValue ("variation")->load() * (1.0f - robotic);
    const float glide = apvts.getRawParameterValue ("glide")->load() * (1.0f - robotic);

    abortRender = false;
    std::vector<float> lead, harm;
    atlas::renderLayer (recBuf.data(), N, *a, ns, 0, var, glide, true, lead, abortRender);
    if (abortRender.load()) return;

    bool anyHarm = false;
    for (auto& nt : ns) if (nt.layer == 1) { anyHarm = true; break; }
    if (anyHarm)
    {
        atlas::renderLayer (recBuf.data(), N, *a, ns, 1, var, glide, false, harm, abortRender);
        if (abortRender.load()) return;
    }

    {
        juce::SpinLock::ScopedLockType sl (outLock);
        leadOut.swap (lead);
        harmOut.swap (harm);
    }
    ++renderVersion;
}

// ------------------------------------------------------- import / export
juce::String AtlasToneProcessor::getTakeName() const
{
    juce::ScopedLock sl (dataLock);
    return takeName;
}

bool AtlasToneProcessor::importFile (const juce::File& file)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples < 4096)
        return false;

    const double fileSr = reader->sampleRate;
    const double hostSr = sampleRate > 0.0 ? sampleRate : fileSr;
    const int srcLen = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (kMaxRecordSeconds * fileSr));
    const int chs = (int) juce::jlimit (1u, 2u, (unsigned int) reader->numChannels);

    juce::AudioBuffer<float> tmp (chs, srcLen);
    tmp.clear();
    reader->read (&tmp, 0, srcLen, 0, true, chs > 1);

    std::vector<float> mono ((size_t) srcLen);
    const float* l = tmp.getReadPointer (0);
    const float* r = chs > 1 ? tmp.getReadPointer (1) : nullptr;
    for (int i = 0; i < srcLen; ++i) mono[(size_t) i] = r != nullptr ? 0.5f * (l[i] + r[i]) : l[i];

    std::vector<float> data;
    if (std::abs (fileSr - hostSr) > 0.5)
    {
        const double ratio = fileSr / hostSr;
        const int outLen = std::max (0, (int) std::floor ((double) srcLen / ratio) - 4);
        data.assign ((size_t) outLen, 0.0f);
        juce::LagrangeInterpolator li;
        li.process (ratio, mono.data(), data.data(), outLen);
    }
    else
        data = std::move (mono);

    recCap = std::max (recCap, (int) (kMaxRecordSeconds * hostSr));
    const int N = std::min ((int) data.size(), recCap);
    if (N < 4096) return false;

    armed = false;
    previewPlaying = false;
    previewPos = 0;
    abortRender = true;
    {
        std::lock_guard<std::mutex> lg (workMutex);
        if ((int) recBuf.size() < recCap) recBuf.assign ((size_t) recCap, 0.0f);
        std::copy (data.begin(), data.begin() + (long) N, recBuf.begin());
        {
            juce::ScopedLock sl (dataLock);
            analysis.reset();
            notes.clear();
            undoStack.clear();
            redoStack.clear();
            takeName = file.getFileNameWithoutExtension();
        }
        {
            juce::SpinLock::ScopedLockType sl (outLock);
            leadOut.clear();
            harmOut.clear();
            origOut.clear();
        }
    }
    hasTake = false;
    takeSampleRate = hostSr;
    takeOrigin = 0;
    takeLength = N;
    recWriteMax = N;
    analysing = true;
    ++notesVersion;
    ++analysisVersion;
    needsAnalysis = true;
    return true;
}

bool AtlasToneProcessor::writeAudio (const juce::File& file)
{
    if (! hasTake.load()) return false;

    // make sure the latest edits are rendered
    if (renderDirty.exchange (false)) doRender();
    else { std::lock_guard<std::mutex> lg (workMutex); }

    std::vector<float> mix;
    {
        juce::SpinLock::ScopedLockType sl (outLock);
        if (leadOut.empty()) return false;
        const float lg = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("leadGain")->load());
        const float hg = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("harmGain")->load());
        const bool haveHarm = harmOut.size() == leadOut.size();
        mix.resize (leadOut.size());
        for (size_t i = 0; i < mix.size(); ++i)
            mix[i] = leadOut[i] * lg + (haveHarm ? harmOut[i] * hg : 0.0f);
    }

    float peak = 0.0f;
    for (float v : mix) peak = std::max (peak, std::abs (v));
    if (peak > 0.98f) { const float k = 0.98f / peak; for (auto& v : mix) v *= k; }

    file.deleteFile();
    std::unique_ptr<juce::FileOutputStream> os (file.createOutputStream());
    if (os == nullptr) return false;

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (os.get(), takeSampleRate.load(), 1u, 24, {}, 0));
    if (writer == nullptr) return false;
    os.release();   // the writer owns the stream now

    const float* chans[] = { mix.data() };
    return writer->writeFromFloatArrays (chans, 1, (int) mix.size());
}

// ----------------------------------------------------------------- preview
void AtlasToneProcessor::togglePreview()
{
    if (previewPlaying.load()) { previewPlaying = false; return; }
    if (! hasTake.load() || hostPlaying.load()) return;
    previewPlaying = true;
}

void AtlasToneProcessor::seekPreview (double seconds)
{
    previewPos = (juce::int64) std::max (0.0, seconds * takeSampleRate.load());
}

double AtlasToneProcessor::playheadSeconds() const
{
    const double sr = std::max (1.0, takeSampleRate.load());
    if (hostPlaying.load()) return (double) (hostPos.load() - takeOrigin.load()) / sr;
    return (double) previewPos.load() / sr;
}

// ------------------------------------------------------------------- notes
void AtlasToneProcessor::detectNotes()
{
    const auto a = getAnalysis();
    if (! a) return;
    const auto fresh = atlas::segmentNotes (*a, nextId);
    editNotes ([&] (std::vector<Note>& v)
    {
        v.erase (std::remove_if (v.begin(), v.end(), [] (const Note& n) { return n.layer == 0; }), v.end());
        v.insert (v.end(), fresh.begin(), fresh.end());
    }, true);
    if (! fresh.empty() && autoKeyEnabled()) detectAndSetKey();
}

void AtlasToneProcessor::clearNotes()
{
    editNotes ([] (std::vector<Note>& v) { v.clear(); }, true);
}

// ------------------------------------------------------------ data access
std::shared_ptr<const Analysis> AtlasToneProcessor::getAnalysis() const
{
    juce::ScopedLock sl (dataLock);
    return analysis;
}

std::vector<Note> AtlasToneProcessor::getNotes() const
{
    juce::ScopedLock sl (dataLock);
    return notes;
}

void AtlasToneProcessor::beginEdit()
{
    juce::ScopedLock sl (dataLock);
    undoStack.push_back (notes);
    if (undoStack.size() > 60) undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void AtlasToneProcessor::editNotes (const std::function<void (std::vector<Note>&)>& fn, bool pushUndo)
{
    {
        juce::ScopedLock sl (dataLock);
        if (pushUndo)
        {
            undoStack.push_back (notes);
            if (undoStack.size() > 60) undoStack.erase (undoStack.begin());
            redoStack.clear();
        }
        fn (notes);
        std::sort (notes.begin(), notes.end(), [] (const Note& x, const Note& y) { return x.start < y.start; });
    }
    ++notesVersion;
    markDirty();
}

void AtlasToneProcessor::undo()
{
    {
        juce::ScopedLock sl (dataLock);
        if (undoStack.empty()) return;
        redoStack.push_back (notes);
        notes = undoStack.back();
        undoStack.pop_back();
    }
    ++notesVersion;
    markDirty();
}

void AtlasToneProcessor::redo()
{
    {
        juce::ScopedLock sl (dataLock);
        if (redoStack.empty()) return;
        undoStack.push_back (notes);
        notes = redoStack.back();
        redoStack.pop_back();
    }
    ++notesVersion;
    markDirty();
}

static bool hasId (const std::vector<int>& ids, int id)
{
    return std::find (ids.begin(), ids.end(), id) != ids.end();
}

void AtlasToneProcessor::splitNote (int id, int frame)
{
    const auto a = getAnalysis();
    if (! a) return;
    editNotes ([&] (std::vector<Note>& v)
    {
        for (size_t i = 0; i < v.size(); ++i)
        {
            if (v[i].id != id) continue;
            if (frame - v[i].start < 2 || v[i].end - frame < 2) return;
            Note left = v[i], right = v[i];
            left.end = frame;
            right.start = frame;
            right.id = newId();
            left.center = atlas::medianRange (*a, left.start, left.end, v[i].center);
            right.center = atlas::medianRange (*a, right.start, right.end, v[i].center);
            v[i] = left;
            v.push_back (right);
            return;
        }
    }, true);
}

void AtlasToneProcessor::applyTrill (int id)
{
    const auto a = getAnalysis();
    if (! a) return;
    const auto sc = currentScale();
    const int rate = (int) apvts.getRawParameterValue ("trillRate")->load();
    const int interval = (int) apvts.getRawParameterValue ("trillInt")->load();
    const double bpm = hostBpm.load() > 1.0 ? hostBpm.load() : 120.0;
    const double beats = rate == 0 ? 0.5 : (rate == 1 ? 0.25 : (rate == 2 ? 0.125 : 0.0625));
    const int pieceFrames = std::max (2, (int) std::lround (beats * 60.0 / bpm * a->sampleRate / a->hop));

    editNotes ([&] (std::vector<Note>& v)
    {
        for (size_t i = 0; i < v.size(); ++i)
        {
            if (v[i].id != id) continue;
            const Note base = v[i];
            const int cnt = (base.end - base.start) / pieceFrames;
            if (cnt < 2) return;

            float other = base.target;
            switch (interval)
            {
                case 0: other = base.target + 1.0f; break;
                case 1: other = base.target + 2.0f; break;
                case 2: other = sc.shiftSteps (base.target, 1); break;
                case 3: other = base.target - 1.0f; break;
                case 4: other = base.target - 2.0f; break;
                default: other = sc.shiftSteps (base.target, -1); break;
            }

            std::vector<Note> pieces;
            for (int k = 0; k < cnt; ++k)
            {
                Note p = base;
                p.id = (k == 0) ? base.id : newId();
                p.start = base.start + k * pieceFrames;
                p.end = (k == cnt - 1) ? base.end : p.start + pieceFrames;
                p.center = atlas::medianRange (*a, p.start, p.end, base.center);
                p.target = (k % 2 == 1) ? other : base.target;
                pieces.push_back (p);
            }
            v.erase (v.begin() + (long) i);
            v.insert (v.end(), pieces.begin(), pieces.end());
            return;
        }
    }, true);
}

void AtlasToneProcessor::createHarmony (const std::vector<int>& ids)
{
    const auto sc = currentScale();
    static const int stepTable[6] = { 2, 4, -2, -4, 7, -7 };
    const int steps = stepTable[juce::jlimit (0, 5, (int) apvts.getRawParameterValue ("harmInterval")->load())];

    editNotes ([&] (std::vector<Note>& v)
    {
        std::vector<Note> sources;
        for (auto& n : v)
            if (n.layer == 0 && (ids.empty() || hasId (ids, n.id)))
                sources.push_back (n);

        v.erase (std::remove_if (v.begin(), v.end(), [&] (const Note& h)
        {
            if (h.layer != 1) return false;
            for (auto& s : sources) if (h.start < s.end && h.end > s.start) return true;
            return false;
        }), v.end());

        for (auto& s : sources)
        {
            Note h = s;
            h.id = newId();
            h.layer = 1;
            h.target = sc.shiftSteps (s.target, steps);
            v.push_back (h);
        }
    }, true);
}

void AtlasToneProcessor::clearHarmony()
{
    editNotes ([] (std::vector<Note>& v)
    {
        v.erase (std::remove_if (v.begin(), v.end(), [] (const Note& n) { return n.layer == 1; }), v.end());
    }, true);
}

void AtlasToneProcessor::autoSnap (const std::vector<int>& ids)
{
    const auto sc = currentScale();
    editNotes ([&] (std::vector<Note>& v)
    {
        for (auto& n : v)
            if (n.layer == 0 && (ids.empty() || hasId (ids, n.id)))
                n.target = sc.snap (n.center);
    }, true);
}

void AtlasToneProcessor::mergeNotes (const std::vector<int>& ids)
{
    const auto a = getAnalysis();
    if (! a || ids.size() < 2) return;
    editNotes ([&] (std::vector<Note>& v)
    {
        for (int layer = 0; layer < 2; ++layer)
        {
            std::vector<Note> group;
            for (auto& n : v) if (n.layer == layer && hasId (ids, n.id)) group.push_back (n);
            if (group.size() < 2) continue;
            std::sort (group.begin(), group.end(), [] (const Note& x, const Note& y) { return x.start < y.start; });
            Note m = group.front();
            for (auto& g : group) { m.start = std::min (m.start, g.start); m.end = std::max (m.end, g.end); }
            m.center = atlas::medianRange (*a, m.start, m.end, m.center);
            v.erase (std::remove_if (v.begin(), v.end(), [&] (const Note& n) { return n.layer == layer && hasId (ids, n.id); }), v.end());
            v.push_back (m);
        }
    }, true);
}

void AtlasToneProcessor::deleteNotes (const std::vector<int>& ids)
{
    if (ids.empty()) return;
    editNotes ([&] (std::vector<Note>& v)
    {
        v.erase (std::remove_if (v.begin(), v.end(), [&] (const Note& n) { return hasId (ids, n.id); }), v.end());
    }, true);
}

// ------------------------------------------------------------------- MIDI
bool AtlasToneProcessor::writeMidi (const juce::File& file)
{
    const auto a = getAnalysis();
    const auto ns = getNotes();
    if (! a || ns.empty()) return false;

    const double bpm = hostBpm.load() > 1.0 ? hostBpm.load() : 120.0;
    const int tpq = 960;
    const double originSec = (double) takeOrigin.load() / a->sampleRate;
    auto toTicks = [&] (int frame)
    {
        const double sec = originSec + a->frameSec ((double) frame);
        return std::max (0.0, sec) * bpm / 60.0 * tpq;
    };

    juce::MidiMessageSequence lead, harm;
    auto tempo = juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / bpm));
    tempo.setTimeStamp (0.0);
    lead.addEvent (tempo);

    for (auto& n : ns)
    {
        const int note = juce::jlimit (0, 127, (int) std::lround (n.target));
        auto& seq = n.layer == 0 ? lead : harm;
        const int ch = n.layer + 1;
        seq.addEvent (juce::MidiMessage::noteOn (ch, note, (juce::uint8) 100), toTicks (n.start));
        seq.addEvent (juce::MidiMessage::noteOff (ch, note, (juce::uint8) 0), std::max (toTicks (n.start), toTicks (n.end) - 1.0));
    }
    lead.updateMatchedPairs();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (tpq);
    mf.addTrack (lead);
    if (harm.getNumEvents() > 0) { harm.updateMatchedPairs(); mf.addTrack (harm); }

    file.deleteFile();
    juce::FileOutputStream os (file);
    if (! os.openedOk()) return false;
    return mf.writeTo (os);
}

// ------------------------------------------------------------------ state
void AtlasToneProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::MemoryOutputStream out (dest, false);
    juce::String xmlText;
    if (auto xml = apvts.copyState().createXml()) xmlText = xml->toString();

    out.writeInt (0x52544e31);
    out.writeString (xmlText);

    const int N = hasTake.load() ? takeLength.load() : 0;
    const bool ok = N > 0 && recBuf.size() >= (size_t) N;
    out.writeInt (ok ? N : 0);
    out.writeDouble (takeSampleRate.load());
    out.writeInt64 (takeOrigin.load());
    if (ok)
    {
        std::vector<juce::int16> tmp ((size_t) N);
        for (int i = 0; i < N; ++i)
            tmp[(size_t) i] = (juce::int16) juce::jlimit (-32767, 32767, (int) std::lround (recBuf[(size_t) i] * 32767.0f));
        out.write (tmp.data(), (size_t) N * sizeof (juce::int16));
    }

    const auto ns = getNotes();
    out.writeInt ((int) ns.size());
    for (auto& n : ns)
    {
        out.writeInt (n.id);
        out.writeInt (n.start);
        out.writeInt (n.end);
        out.writeFloat (n.center);
        out.writeFloat (n.target);
        out.writeInt (n.layer);
    }
    out.writeInt (nextId.load());
}

void AtlasToneProcessor::setStateInformation (const void* data, int size)
{
    juce::MemoryInputStream in (data, (size_t) size, false);
    if (in.readInt() != 0x52544e31) return;

    const auto xmlText = in.readString();
    if (auto xml = juce::XmlDocument::parse (xmlText))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));

    const int N = in.readInt();
    const double sr = in.readDouble();
    const juce::int64 origin = in.readInt64();

    abortRender = true;
    std::vector<float> loaded;
    if (N > 0)
    {
        std::vector<juce::int16> tmp ((size_t) N);
        in.read (tmp.data(), N * (int) sizeof (juce::int16));
        loaded.resize ((size_t) N);
        for (int i = 0; i < N; ++i) loaded[(size_t) i] = (float) tmp[(size_t) i] / 32767.0f;
    }

    std::vector<Note> ns;
    const int count = in.readInt();
    for (int i = 0; i < count && ! in.isExhausted(); ++i)
    {
        Note n;
        n.id = in.readInt();
        n.start = in.readInt();
        n.end = in.readInt();
        n.center = in.readFloat();
        n.target = in.readFloat();
        n.layer = in.readInt();
        ns.push_back (n);
    }
    const int savedNext = in.readInt();

    {
        std::lock_guard<std::mutex> lg (workMutex);
        if (N > 0)
        {
            const int cap = std::max (N, (int) (kMaxRecordSeconds * (sampleRate > 0 ? sampleRate : sr)));
            recCap = std::max (recCap, cap);
            recBuf.assign ((size_t) recCap, 0.0f);
            std::copy (loaded.begin(), loaded.end(), recBuf.begin());
            takeSampleRate = sr;
            takeOrigin = origin;
            takeLength = N;
        }
        juce::ScopedLock sl (dataLock);
        notes = ns;
        undoStack.clear();
        redoStack.clear();
    }
    if (savedNext > 0) nextId = savedNext;
    ++notesVersion;
    if (N > 0) loadAnalysisPending = true;
}

juce::AudioProcessorEditor* AtlasToneProcessor::createEditor() { return new AtlasToneEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AtlasToneProcessor(); }
