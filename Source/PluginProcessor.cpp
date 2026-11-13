#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace facet;

namespace
{
    using APF = juce::AudioParameterFloat;
    using APC = juce::AudioParameterChoice;
    using APB = juce::AudioParameterBool;
    using API = juce::AudioParameterInt;
    using NR  = juce::NormalisableRange<float>;

    NR skewed (float lo, float hi, float centre, float step = 0.0f)
    {
        NR r (lo, hi, step);
        r.setSkewForCentre (centre);
        return r;
    }
    juce::ParameterID pid (const juce::String& s) { return { s, 1 }; }
}

juce::AudioProcessorValueTreeState::ParameterLayout FacetProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto add = [&p] (auto* x) { p.emplace_back (x); };

    // buffer
    add (new APC (pid ("playMode"), "Play Mode", playModeNames(), PlayAudio));
    add (new APC (pid ("mode"), "Buffer Mode", bufferModeNames(), ModeCapture));
    add (new APF (pid ("window"), "Window", skewed (0.1f, 10.0f, 2.0f, 0.01f), 4.0f));
    add (new APC (pid ("scanMode"), "Scan Mode", scanModeNames(), ScanStatic));
    add (new APF (pid ("scanSpeed"), "Scan Speed", skewed (0.0f, 4.0f, 1.0f, 0.001f), 1.0f));
    add (new APF (pid ("feedback"), "Feedback", NR (0.0f, 0.95f, 0.001f), 0.0f));

    // grains
    add (new APF (pid ("position"), "Position", NR (0.0f, 1.0f, 0.0001f), 0.75f));
    add (new APF (pid ("size"), "Grain Size", skewed (5.0f, 2000.0f, 150.0f, 0.1f), 120.0f));
    add (new APF (pid ("density"), "Density", skewed (0.5f, 200.0f, 15.0f, 0.01f), 12.0f));
    add (new APB (pid ("sync"), "Sync", false));
    add (new APC (pid ("division"), "Division", divisionNames(), 6));
    add (new APF (pid ("spray"), "Spray", NR (0.0f, 1.0f, 0.001f), 0.0f));
    add (new API (pid ("pattern"), "Pattern Length", 1, 64, 8));
    add (new API (pid ("seed"), "Seed", 1, 999, 1));
    add (new APF (pid ("pitch"), "Pitch", NR (-24.0f, 24.0f, 0.01f), 0.0f));
    add (new APF (pid ("fine"), "Fine", NR (-100.0f, 100.0f, 0.1f), 0.0f));
    add (new APC (pid ("direction"), "Direction", grainDirNames(), DirForward));
    add (new APF (pid ("shape"), "Grain Shape", NR (0.0f, 1.0f, 0.001f), 1.0f));
    add (new APF (pid ("pan"), "Pan Spread", NR (0.0f, 1.0f, 0.001f), 0.3f));

    // shift
    add (new APF (pid ("shiftStep"), "Shift Step", NR (-12.0f, 12.0f, 0.01f), 0.0f));
    add (new API (pid ("shiftSteps"), "Shift Steps", 1, 8, 1));
    {
        NR fr (-1000.0f, 1000.0f, 0.1f);
        fr.setSkewForCentre (0.0f);
        fr.symmetricSkew = true;
        fr.skew = 0.4f;
        add (new APF (pid ("freqShift"), "Freq Shift", fr, 0.0f));
    }

    // slice
    add (new APB (pid ("sliceOn"), "Slice", false));
    add (new APC (pid ("sliceDiv"), "Slice Division", divisionNames(), 6));
    add (new APC (pid ("slicePattern"), "Slice Pattern", slicePatternNames(), 0));
    add (new APF (pid ("sliceDuty"), "Slice Duty", NR (0.05f, 1.0f, 0.001f), 0.6f));
    add (new APB (pid ("sliceReset"), "Slice Resets Scan", false));

    // dynamics
    add (new APF (pid ("compThresh"), "Comp Threshold", NR (-48.0f, 0.0f, 0.1f), -12.0f));
    add (new APF (pid ("compRatio"), "Comp Ratio", skewed (1.0f, 20.0f, 4.0f, 0.01f), 1.0f));
    add (new APF (pid ("compAttack"), "Comp Attack", skewed (0.1f, 100.0f, 10.0f, 0.01f), 10.0f));
    add (new APF (pid ("compRelease"), "Comp Release", skewed (10.0f, 1000.0f, 150.0f, 0.1f), 120.0f));
    add (new APF (pid ("compMakeup"), "Comp Makeup", NR (0.0f, 24.0f, 0.1f), 0.0f));

    // output
    add (new APF (pid ("mix"), "Mix", NR (0.0f, 1.0f, 0.001f), 1.0f));
    add (new APF (pid ("out"), "Output", NR (-24.0f, 12.0f, 0.1f), 0.0f));

    // instrument
    add (new API (pid ("root"), "Root Note", 0, 127, 60));
    add (new APF (pid ("attack"), "Attack", skewed (0.5f, 2000.0f, 50.0f, 0.1f), 5.0f));
    add (new APF (pid ("release"), "Release", skewed (5.0f, 5000.0f, 400.0f, 0.1f), 300.0f));

    // modulators
    for (int k = 1; k <= kNumMods; ++k)
    {
        const juce::String m = "m" + juce::String (k) + "_";
        const juce::String n = "Mod " + juce::String (k) + " ";
        add (new APC (pid (m + "shape"), n + "Shape", modShapeNames(), ShapeSine));
        add (new APC (pid (m + "rate"), n + "Rate Mode", modRateNames(), RateHz));
        add (new APF (pid (m + "hz"), n + "Rate", skewed (0.01f, 40.0f, 1.0f, 0.001f), 0.5f));
        add (new APC (pid (m + "div"), n + "Division", divisionNames(), 2));
        add (new APF (pid (m + "depth"), n + "Depth", NR (-1.0f, 1.0f, 0.001f), 0.0f));
        add (new APC (pid (m + "dest"), n + "Destination", modDestNames(), DestNone));
    }

    // read heads: each reads its own spot (Position + offset) and can go to its own output
    static const float defaultOffsets[] = { 0.0f, 0.25f, 0.5f, 0.75f };
    for (int h = 1; h <= kNumHeads; ++h)
    {
        const juce::String id = "h" + juce::String (h) + "_";
        const juce::String n  = "Head " + juce::String (h) + " ";
        add (new APB (pid (id + "on"), n + "On", h == 1));
        add (new APF (pid (id + "offset"), n + "Offset", NR (0.0f, 1.0f, 0.0001f), defaultOffsets[h - 1]));
        add (new APF (pid (id + "pitch"), n + "Pitch", NR (-24.0f, 24.0f, 0.01f), 0.0f));
        add (new APF (pid (id + "level"), n + "Level", skewed (-48.0f, 6.0f, -12.0f, 0.1f), 0.0f));
        add (new APF (pid (id + "pan"), n + "Pan", NR (-1.0f, 1.0f, 0.001f), 0.0f));
        add (new APC (pid (id + "out"), n + "Stem", headOutputNames(), h - 1));
        static const float defAz[] = { -45.0f, 45.0f, -135.0f, 135.0f };
        add (new APF (pid (id + "az"), n + "Direction", NR (-180.0f, 180.0f, 0.1f), defAz[h - 1]));
        add (new APF (pid (id + "el"), n + "Height", NR (-90.0f, 90.0f, 0.1f), 0.0f));
        // own grain settings (used when Link is off)
        add (new APB (pid (id + "link"), n + "Link", true));
        add (new APF (pid (id + "size"), n + "Size", skewed (5.0f, 2000.0f, 150.0f, 0.1f), 120.0f));
        add (new APF (pid (id + "density"), n + "Density", skewed (0.5f, 200.0f, 15.0f, 0.01f), 12.0f));
        add (new APC (pid (id + "division"), n + "Division", divisionNames(), 6));
        add (new APF (pid (id + "spray"), n + "Spray", NR (0.0f, 1.0f, 0.001f), 0.0f));
        add (new APC (pid (id + "dir"), n + "Direction", grainDirNames(), DirForward));
        add (new APF (pid (id + "shape"), n + "Shape", NR (0.0f, 1.0f, 0.001f), 1.0f));
        add (new APC (pid (id + "scan"), n + "Scan", scanModeNames(), ScanStatic));
        add (new APF (pid (id + "scanSpeed"), n + "Scan Speed", skewed (0.0f, 4.0f, 1.0f, 0.001f), 1.0f));
        add (new API (pid (id + "pattern"), n + "Pattern Length", 1, 64, 8));
        add (new API (pid (id + "seed"), n + "Seed", 1, 999, h));
    }

    // space (same output formats as Sediments)
    add (new APC (pid ("output"), "Output", outputNames(), 0));
    add (new APC (pid ("placeBy"), "Place By", placeByNames(), PlaceCentre));
    add (new APC (pid ("height"), "Height", heightNames(), HeightFlat));
    add (new APF (pid ("width"), "Width", NR (0.0f, 1.0f, 0.001f), 1.0f));
    add (new APF (pid ("scatter"), "Scatter", NR (0.0f, 1.0f, 0.001f), 0.0f));
    {
        NR rr (-360.0f, 360.0f, 0.1f);
        rr.setSkewForCentre (0.0f); rr.symmetricSkew = true; rr.skew = 0.5f;
        add (new APF (pid ("rotate"), "Rotation Speed", rr, 0.0f));
    }
    add (new APB (pid ("frontCentre"), "Speaker 1 In Front", false));
    add (new APB (pid ("useCentre"), "Use Centre Speaker", true));
    add (new APB (pid ("lfe"), "Feed The LFE", false));

    return { p.begin(), p.end() };
}

//==============================================================================
FacetProcessor::FacetProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::discreteChannels (kMaxOut), true)),
      apvts (*this, nullptr, "FACET", createLayout())
{
    formatManager.registerBasicFormats();
    for (auto& c : ccMap) c.store (-1);
    startTimerHz (60);
}

bool FacetProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // any output width from 1 to 16 channels; the format uses what the track has
    const auto out = layouts.getMainOutputChannelSet();
    if (out.isDisabled() || out.size() > kMaxOut) return false;
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void FacetProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
}

//==============================================================================
void FacetProcessor::readParams (Params& p) const
{
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto i = [&f]   (const char* id) { return (int) std::lround (f (id)); };

    p.playMode   = i ("playMode");
    p.output     = i ("output");    p.placeBy = i ("placeBy");     p.height = i ("height");
    p.width      = f ("width");     p.scatter = f ("scatter");     p.rotateSpeed = f ("rotate");
    p.frontCentre = f ("frontCentre") > 0.5f; p.useCentre = f ("useCentre") > 0.5f; p.lfe = f ("lfe") > 0.5f;
    p.bufferMode = i ("mode");      p.windowSec = f ("window");
    p.scanMode   = i ("scanMode");  p.scanSpeed = f ("scanSpeed");  p.feedback = f ("feedback");
    p.position   = f ("position");  p.sizeMs = f ("size");          p.density = f ("density");
    p.sync       = f ("sync") > 0.5f; p.division = i ("division");  p.spray = f ("spray");
    p.patternLen = i ("pattern");   p.seed = i ("seed");            p.pitch = f ("pitch");
    p.fine       = f ("fine");      p.grainDir = i ("direction");   p.shape = f ("shape");
    p.panSpread  = f ("pan");
    p.shiftStep  = f ("shiftStep"); p.shiftSteps = i ("shiftSteps"); p.freqShift = f ("freqShift");
    p.sliceOn    = f ("sliceOn") > 0.5f; p.sliceDiv = i ("sliceDiv"); p.slicePattern = i ("slicePattern");
    p.sliceDuty  = f ("sliceDuty"); p.sliceReset = f ("sliceReset") > 0.5f;
    p.compThresh = f ("compThresh"); p.compRatio = f ("compRatio");
    p.compAttackMs = f ("compAttack"); p.compReleaseMs = f ("compRelease"); p.compMakeup = f ("compMakeup");
    p.mix = f ("mix"); p.outGainDb = f ("out");
    p.rootNote = i ("root"); p.attackMs = f ("attack"); p.releaseMs = f ("release");

    for (int h = 0; h < kNumHeads; ++h)
    {
        const juce::String id = "h" + juce::String (h + 1) + "_";
        auto& hp = p.heads[(size_t) h];
        hp.on      = apvts.getRawParameterValue (id + "on")->load() > 0.5f;
        hp.offset  = apvts.getRawParameterValue (id + "offset")->load();
        hp.pitch   = apvts.getRawParameterValue (id + "pitch")->load();
        hp.levelDb = apvts.getRawParameterValue (id + "level")->load();
        hp.pan     = apvts.getRawParameterValue (id + "pan")->load();
        hp.bus     = (int) std::lround (apvts.getRawParameterValue (id + "out")->load());
        hp.az      = apvts.getRawParameterValue (id + "az")->load();
        hp.el      = apvts.getRawParameterValue (id + "el")->load();
        auto v = [this, &id] (const char* name) { return apvts.getRawParameterValue (id + name)->load(); };
        hp.link       = v ("link") > 0.5f;
        hp.sizeMs     = v ("size");
        hp.density    = v ("density");
        hp.division   = (int) std::lround (v ("division"));
        hp.spray      = v ("spray");
        hp.grainDir   = (int) std::lround (v ("dir"));
        hp.shape      = v ("shape");
        hp.scanMode   = (int) std::lround (v ("scan"));
        hp.scanSpeed  = v ("scanSpeed");
        hp.patternLen = (int) std::lround (v ("pattern"));
        hp.seed       = (int) std::lround (v ("seed"));
    }

    for (int k = 0; k < kNumMods; ++k)
    {
        const juce::String m = "m" + juce::String (k + 1) + "_";
        auto& mp = p.mods[(size_t) k];
        mp.shape    = (int) std::lround (apvts.getRawParameterValue (m + "shape")->load());
        mp.rateMode = (int) std::lround (apvts.getRawParameterValue (m + "rate")->load());
        mp.rateHz   = apvts.getRawParameterValue (m + "hz")->load();
        mp.division = (int) std::lround (apvts.getRawParameterValue (m + "div")->load());
        mp.depth    = apvts.getRawParameterValue (m + "depth")->load();
        mp.dest     = (int) std::lround (apvts.getRawParameterValue (m + "dest")->load());
    }
}

void FacetProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // MIDI learn / CC mapping
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (! msg.isController()) continue;
        const int cc = msg.getControllerNumber();
        const int learn = learnIndex.exchange (-1);
        if (learn >= 0)
        {
            for (auto& c : ccMap) if (c.load() == learn) c.store (-1);
            ccMap[(size_t) cc].store (learn);
        }
        const int target = ccMap[(size_t) cc].load();
        if (target >= 0)
        {
            const auto scope = ccFifo.write (1);
            if (scope.blockSize1 > 0) ccQueue[(size_t) scope.startIndex1] = { target, msg.getControllerValue() / 127.0f };
        }
    }

    readParams (params);

    Transport tr;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            tr.playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) tr.bpm = *b;
            if (auto q = pos->getPpqPosition()) { tr.ppq = *q; tr.hasPpq = true; }
        }
    }

    const int numIn = getMainBusNumInputChannels();
    auto mainBus = getBusBuffer (buffer, false, 0);
    engine.process (mainBus, numIn, midi, tr, params);
}

//==============================================================================
void FacetProcessor::timerCallback()
{
    const auto scope = ccFifo.read (ccFifo.getNumReady());
    auto apply = [this] (int start, int size)
    {
        auto& ps = getParameters();
        for (int i = 0; i < size; ++i)
        {
            const auto [idx, v] = ccQueue[(size_t) (start + i)];
            if (juce::isPositiveAndBelow (idx, ps.size()))
                ps[idx]->setValueNotifyingHost (v);
        }
    };
    apply (scope.startIndex1, scope.blockSize1);
    apply (scope.startIndex2, scope.blockSize2);
}

int FacetProcessor::paramIndex (const juce::String& id) const
{
    if (auto* p = apvts.getParameter (id))
        return p->getParameterIndex();
    return -1;
}

void FacetProcessor::copyMainToHead (int head)
{
    const juce::String id = "h" + juce::String (head + 1) + "_";
    const std::pair<const char*, const char*> map[] = {
        { "size", "size" }, { "density", "density" }, { "division", "division" }, { "spray", "spray" },
        { "direction", "dir" }, { "shape", "shape" }, { "scanMode", "scan" }, { "scanSpeed", "scanSpeed" },
        { "pattern", "pattern" }, { "seed", "seed" } };
    for (auto& [from, to] : map)
    {
        auto* src = apvts.getParameter (from);
        auto* dst = apvts.getParameter (id + to);
        if (src == nullptr || dst == nullptr) continue;
        dst->beginChangeGesture();
        dst->setValueNotifyingHost (dst->convertTo0to1 (src->convertFrom0to1 (src->getValue())));
        dst->endChangeGesture();
    }
}

void FacetProcessor::setAllLinks (bool linked)
{
    for (int h = 1; h <= kNumHeads; ++h)
        if (auto* prm = apvts.getParameter ("h" + juce::String (h) + "_link"))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (linked ? 1.0f : 0.0f);
            prm->endChangeGesture();
        }
}

void FacetProcessor::startMidiLearn (const juce::String& id) { learnIndex.store (paramIndex (id)); }

void FacetProcessor::clearMidiLearn (const juce::String& id)
{
    const int idx = paramIndex (id);
    for (auto& c : ccMap) if (c.load() == idx) c.store (-1);
}

bool FacetProcessor::isLearning (const juce::String& id) const
{
    return learnIndex.load() >= 0 && learnIndex.load() == paramIndex (id);
}

juce::String FacetProcessor::midiLearnInfo (const juce::String& id) const
{
    const int idx = paramIndex (id);
    for (int cc = 0; cc < 128; ++cc)
        if (ccMap[(size_t) cc].load() == idx) return "CC " + juce::String (cc);
    return {};
}

//==============================================================================
bool FacetProcessor::loadAudioFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr) return false;

    const auto maxLen = (juce::int64) (kBufferSeconds * reader->sampleRate);
    const int len = (int) juce::jmin (reader->lengthInSamples, maxLen);
    juce::AudioBuffer<float> data ((int) juce::jmax (1u, reader->numChannels), len);
    reader->read (&data, 0, len, 0, true, true);

    engine.loadFile (data, reader->sampleRate);
    loadedFileName = file.getFileName();
    apvts.state.setProperty ("filePath", file.getFullPathName(), nullptr);

    if (auto* m = apvts.getParameter ("mode"))
        m->setValueNotifyingHost (m->convertTo0to1 ((float) ModeFile));
    return true;
}

void FacetProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    juce::ValueTree map ("MIDIMAP");
    auto& ps = getParameters();
    for (int cc = 0; cc < 128; ++cc)
    {
        const int idx = ccMap[(size_t) cc].load();
        if (idx < 0 || idx >= ps.size()) continue;
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ps[idx]))
        {
            juce::ValueTree e ("CC");
            e.setProperty ("cc", cc, nullptr);
            e.setProperty ("param", rp->getParameterID(), nullptr);
            map.appendChild (e, nullptr);
        }
    }
    state.removeChild (state.getChildWithName ("MIDIMAP"), nullptr);
    state.appendChild (map, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void FacetProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);

    for (auto& c : ccMap) c.store (-1);
    const auto map = state.getChildWithName ("MIDIMAP");
    for (const auto& e : map)
    {
        const int cc = e.getProperty ("cc");
        const int idx = paramIndex (e.getProperty ("param").toString());
        if (juce::isPositiveAndBelow (cc, 128) && idx >= 0) ccMap[(size_t) cc].store (idx);
    }
    apvts.replaceState (state);

    const juce::File f (state.getProperty ("filePath").toString());
    if (f.existsAsFile())
    {
        const auto mode = apvts.getRawParameterValue ("mode")->load();
        loadAudioFile (f);
        if (auto* m = apvts.getParameter ("mode"))   // keep the saved mode
            m->setValueNotifyingHost (m->convertTo0to1 (mode));
    }
}

juce::AudioProcessorEditor* FacetProcessor::createEditor() { return new FacetEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FacetProcessor(); }
