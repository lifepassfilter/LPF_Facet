// LPF Facet — granular buffer engine with grains that repeat exactly
// Original implementation. Header-only; depends only on juce_audio_basics.
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace facet
{

constexpr int    kMaxGrains        = 96;
constexpr int    kNumMods          = 3;
constexpr int    kMaxVoices        = 8;
constexpr double kMaxWindowSeconds = 10.0;
constexpr double kBufferSeconds    = 18.0;   // window + headroom for long / pitched grains
constexpr int    kNumPeakBins      = 512;
constexpr int    kControlRate      = 32;     // samples per control tick

enum BufferMode { ModeCapture = 0, ModeFreeze, ModeFile };
enum ScanMode   { ScanStatic = 0, ScanForward, ScanReverse, ScanPingPong, ScanOneShot };
enum GrainDir   { DirForward = 0, DirReverse, DirAlternate };
enum ModShape   { ShapeSine = 0, ShapeTriangle, ShapeSawUp, ShapeSawDown, ShapeSquare, ShapeStep, ShapeDrift };
enum ModRate    { RateHz = 0, RateSync, RateGrain, RateLink };
enum ModDest
{
    DestNone = 0, DestPosition, DestSize, DestDensity, DestPitch, DestSpray, DestPan,
    DestScanSpeed, DestShape, DestFreqShift, DestSliceDuty, DestFeedback, DestMix, NumDests
};

inline juce::StringArray bufferModeNames() { return { "Capture", "Freeze", "File" }; }
inline juce::StringArray scanModeNames()   { return { "Static", "Forward", "Reverse", "Ping-Pong", "One-Shot" }; }
inline juce::StringArray grainDirNames()   { return { "Forward", "Reverse", "Alternate" }; }
inline juce::StringArray modShapeNames()   { return { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Step", "Drift" }; }
inline juce::StringArray modRateNames()    { return { "Hz", "Sync", "Grain", "Link" }; }
inline juce::StringArray modDestNames()
{
    return { "None", "Position", "Size", "Density", "Pitch", "Spray", "Pan",
             "Scan Speed", "Shape", "Freq Shift", "Slice Duty", "Feedback", "Mix" };
}
inline juce::StringArray divisionNames()   { return { "1/1", "1/2", "1/4", "1/8.", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/64" }; }
inline double divisionBeats (int i)
{
    static const double d[] = { 4.0, 2.0, 1.0, 0.75, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 0.0625 };
    return d[juce::jlimit (0, 9, i)];
}
inline juce::StringArray slicePatternNames()
{
    return { "Straight", "Offbeat", "Onbeat", "Tresillo", "Gallop", "Sparse", "Stutter", "Seeded" };
}

//==============================================================================
// Deterministic hashing: the same (index, seed) always gives the same value.
inline uint32_t hash32 (uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}
inline float hash01 (int64_t index, int seed, int salt)
{
    const uint32_t k = hash32 ((uint32_t) seed * 0x85ebca6bU + (uint32_t) salt * 0xc2b2ae35U + 0x27d4eb2fU);
    const uint32_t h = hash32 ((uint32_t) index * 0x9E3779B9U ^ k ^ (uint32_t) (index >> 32));
    return (float) (h >> 8) * (1.0f / 16777216.0f);
}
inline int64_t posMod (int64_t a, int64_t m) { const int64_t r = a % m; return r < 0 ? r + m : r; }
inline double  frac   (double x)             { return x - std::floor (x); }

//==============================================================================
struct ModParams
{
    int   shape = ShapeSine, rateMode = RateHz, division = 4, dest = DestNone;
    float rateHz = 1.0f, depth = 0.0f;
};

struct Params
{
    // buffer
    int   bufferMode = ModeCapture;
    float windowSec  = 4.0f;
    int   scanMode   = ScanStatic;
    float scanSpeed  = 1.0f;     // windows traversed per window-length of time (1 = real time)
    float feedback   = 0.0f;
    // grains
    float position   = 0.75f;    // 0..1 inside region (1 = newest audio in Capture)
    float sizeMs     = 120.0f;
    float density    = 12.0f;    // grains / s (free mode)
    bool  sync       = false;
    int   division   = 6;        // grain clock when synced
    float spray      = 0.0f;
    int   patternLen = 8;        // grain pattern repeats every N grains
    int   seed       = 1;
    float pitch      = 0.0f;     // semitones
    float fine       = 0.0f;     // cents
    int   grainDir   = DirForward;
    float shape      = 1.0f;     // 0 = hard/rect, 1 = Hann
    float panSpread  = 0.3f;
    // shift (grain pitch stepping) + frequency shifter
    float shiftStep  = 0.0f;     // semitones added per pattern step
    int   shiftSteps = 1;
    float freqShift  = 0.0f;     // Hz
    // slice
    bool  sliceOn    = false;
    int   sliceDiv   = 6;
    int   slicePattern = 0;
    float sliceDuty  = 0.6f;
    bool  sliceReset = false;
    // dynamics
    float compThresh = -12.0f, compRatio = 1.0f, compAttackMs = 10.0f, compReleaseMs = 120.0f, compMakeup = 0.0f;
    // out
    float mix = 1.0f, outGainDb = 0.0f;
    // instrument
    int   rootNote = 60;
    float attackMs = 5.0f, releaseMs = 300.0f;

    std::array<ModParams, kNumMods> mods;
};

struct Transport
{
    bool   playing = false;
    bool   hasPpq  = false;
    double bpm     = 120.0;
    double ppq     = 0.0;
};

//==============================================================================
class Engine
{
public:
    Engine()
    {
        for (auto& p : capPeaks)  p.store (0.0f);
        for (auto& p : filePeaks) p.store (0.0f);
        for (auto& g : grainView) g.store (-1.0f);
        for (auto& m : modView)   m.store (0.0f);
    }

    //--------------------------------------------------------------------------
    void prepare (double sampleRate, int maxBlock, bool instrumentMode)
    {
        const juce::SpinLock::ScopedLockType sl (bufferLock);
        sr      = sampleRate;
        isSynth = instrumentMode;
        bufLen  = (int) std::ceil (kBufferSeconds * sr);
        cap.setSize (2, bufLen);
        cap.clear();
        dry.setSize (2, juce::jmax (1, maxBlock));
        writePos = 0;
        for (auto& p : capPeaks) p.store (0.0f);

        for (auto& g : grains) g.active = false;
        for (auto& v : voices) v = Voice();
        if (! isSynth) startVoice (0, -1, 1.0f);

        for (auto& m : modPhase) m = 0.0;
        gate = 1.0f; fbL = fbR = 0.0f; compEnv = 0.0f; limGain = 1.0f;
        fsPhase = 0.0; for (auto& h : hilbert) h.reset();
        ppq = 0.0; lastSliceStep = -1; wasPlaying = false; lastMode = -1;

        if (pendingFile.getNumSamples() > 0)
            installFile (pendingFile, pendingFileRate);
    }

    // Message thread. Resamples to the session rate and swaps in under the lock.
    void loadFile (const juce::AudioBuffer<float>& src, double srcRate)
    {
        pendingFile.makeCopyOf (src);
        pendingFileRate = srcRate;
        if (sr > 0.0) installFile (src, srcRate);
    }

    bool hasFile() const { return fileLen.load() > 0; }

    //--------------------------------------------------------------------------
    void process (juce::AudioBuffer<float>& io, int numInputChannels,
                  const juce::MidiBuffer& midi, const Transport& tr, const Params& p)
    {
        const juce::SpinLock::ScopedTryLockType sl (bufferLock);
        const int n = io.getNumSamples();
        if (! sl.isLocked() || bufLen == 0) { io.clear(); return; }

        const int nch = io.getNumChannels();
        if (dry.getNumSamples() < n) dry.setSize (2, n, false, false, true);
        if (numInputChannels > 0 && nch > 0)
        {
            dry.copyFrom (0, 0, io, 0, 0, n);
            dry.copyFrom (1, 0, io, juce::jmin (1, nch - 1), 0, n);
        }
        else dry.clear();

        // ---- transport -----------------------------------------------------
        bpm = tr.bpm > 1.0 ? tr.bpm : 120.0;
        const bool hostPlaying = tr.playing && tr.hasPpq;
        bool resync = false;
        if (hostPlaying)
        {
            if (! wasPlaying || std::abs (tr.ppq - ppq) > 0.02) resync = true;
            ppq = tr.ppq;
        }
        wasPlaying = hostPlaying;
        const double ppqInc = bpm / 60.0 / sr;

        if (p.bufferMode != lastMode)   // source changed: old grains point to the wrong buffer
        {
            for (auto& g : grains) g.active = false;
            lastMode = p.bufferMode;
        }

        float* outL = io.getWritePointer (0);
        float* outR = nch > 1 ? io.getWritePointer (1) : nullptr;
        const float* inL = dry.getReadPointer (0);
        const float* inR = dry.getReadPointer (1);

        auto midiIt = midi.begin();
        const float outGain = juce::Decibels::decibelsToGain (p.outGainDb);

        for (int i = 0; i < n; ++i)
        {
            // MIDI (instrument)
            for (; midiIt != midi.end() && (*midiIt).samplePosition <= i; ++midiIt)
                handleMidi ((*midiIt).getMessage(), p);

            if ((i % kControlRate) == 0)
                controlTick (p, hostPlaying, resync && i == 0);

            // ---- slice clock -------------------------------------------------
            const double sliceBeats = divisionBeats (p.sliceDiv);
            const int64_t sliceStep = (int64_t) std::floor (ppq / sliceBeats);
            const double  sliceFrac = ppq / sliceBeats - (double) sliceStep;
            const bool    newSlice  = sliceStep != lastSliceStep;
            lastSliceStep = sliceStep;
            const bool bit = ((slicePatternBits (p.slicePattern, p.seed) >> (int) posMod (sliceStep, 16)) & 1u) != 0;
            if (p.sliceOn && p.sliceReset && newSlice && bit)
                for (auto& v : voices) if (v.active) { v.scanPhase = 0.0; v.forceTrigger = true; }
            const float gateTarget = ! p.sliceOn ? 1.0f : ((bit && sliceFrac < cur.sliceDuty) ? 1.0f : 0.0f);
            gate += (gateTarget - gate) * gateCoef;

            // ---- write input into rolling buffer ----------------------------
            if (p.bufferMode == ModeCapture)
            {
                const float wl = inL[i] + fbL, wr = inR[i] + fbR;
                cap.setSample (0, writePos, wl);
                cap.setSample (1, writePos, wr);
                const int bin = (int) ((int64_t) writePos * kNumPeakBins / bufLen);
                const float pk = juce::jmax (std::abs (wl), std::abs (wr));
                if (bin != lastPeakBin) { capPeaks[(size_t) bin].store (pk); lastPeakBin = bin; }
                else if (pk > capPeaks[(size_t) bin].load()) capPeaks[(size_t) bin].store (pk);
                if (++writePos >= bufLen) writePos = 0;
            }

            // ---- voices: grain clock + scan --------------------------------
            for (int vi = 0; vi < kMaxVoices; ++vi)
            {
                auto& v = voices[(size_t) vi];
                if (! v.active) continue;
                if (isSynth)
                {
                    const float target = v.releasing ? 0.0f : 1.0f;
                    v.env += (target - v.env) * (v.releasing ? relCoef : atkCoef);
                    if (v.releasing && v.env < 1.0e-4f) { v.active = false; continue; }
                }
                v.clock += cur.clockInc;
                const int64_t idx = (int64_t) std::floor (v.clock);
                if (idx != v.lastIndex || v.forceTrigger)
                {
                    spawnGrain (vi, idx, p);
                    v.lastIndex = idx;
                    v.forceTrigger = false;
                    leadVoice = vi;
                }
                v.scanPhase += cur.scanInc;
            }

            // ---- grains ------------------------------------------------------
            float wetL = 0.0f, wetR = 0.0f;
            const Source src = currentSource (p.bufferMode);
            if (src.len > 4)
            {
                for (auto& g : grains)
                {
                    if (! g.active) continue;
                    const float vEnv = voices[(size_t) g.voice].active ? voices[(size_t) g.voice].env : 0.0f;
                    if (vEnv <= 0.0f || g.wrap != src.len) { g.active = false; continue; }

                    const float x   = (float) g.age / (float) g.len;
                    const float env = window (x, g.taper) * g.gain * vEnv;
                    float sL, sR;
                    readHermite (src, g.pos, sL, sR);
                    wetL += sL * env * g.gl;
                    wetR += sR * env * g.gr;

                    g.pos += g.rate;
                    if (g.pos >= g.wrap) g.pos -= g.wrap;
                    else if (g.pos < 0.0) g.pos += g.wrap;
                    if (++g.age >= g.len) g.active = false;
                }
            }

            // ---- frequency shifter (Hilbert, single sideband) --------------
            if (std::abs (cur.freqShift) > 0.5f)
            {
                const double c = std::cos (fsPhase * juce::MathConstants<double>::twoPi);
                const double s = std::sin (fsPhase * juce::MathConstants<double>::twoPi);
                wetL = hilbert[0].shift (wetL, c, s);
                wetR = hilbert[1].shift (wetR, c, s);
                fsPhase = frac (fsPhase + cur.freqShift / sr);
            }

            wetL *= gate;
            wetR *= gate;

            // feedback back into the capture buffer (layering / stacking)
            fbL = std::tanh (wetL * cur.feedback);
            fbR = std::tanh (wetR * cur.feedback);

            // ---- dynamics: compressor + safety limiter ----------------------
            {
                const float pk = juce::jmax (std::abs (wetL), std::abs (wetR));
                compEnv += (pk - compEnv) * (pk > compEnv ? compAtk : compRel);
                float gDb = p.compMakeup;
                const float lvl = juce::Decibels::gainToDecibels (compEnv, -120.0f);
                if (p.compRatio > 1.0f && lvl > p.compThresh)
                    gDb -= (lvl - p.compThresh) * (1.0f - 1.0f / p.compRatio);
                const float g = juce::Decibels::decibelsToGain (gDb);
                wetL *= g; wetR *= g;
            }

            float oL = inL[i] * (1.0f - cur.mix) + wetL * cur.mix;
            float oR = inR[i] * (1.0f - cur.mix) + wetR * cur.mix;
            oL *= outGain; oR *= outGain;

            const float opk = juce::jmax (std::abs (oL), std::abs (oR));
            const float lt  = opk > 0.97f ? 0.97f / opk : 1.0f;
            limGain = lt < limGain ? lt : limGain + (1.0f - limGain) * limRel;
            oL *= limGain; oR *= limGain;

            outL[i] = oL;
            if (outR != nullptr) outR[i] = oR;

            ppq += ppqInc;
        }

        for (int c = 2; c < nch; ++c) io.clear (c, 0, n);
        publishView (p);
    }

    //--------------------------------------------------------------------------
    // Display state (read by the editor, written by the audio thread)
    std::array<std::atomic<float>, kNumPeakBins> capPeaks, filePeaks;
    std::array<std::atomic<float>, kMaxGrains>   grainView;   // region fraction, or -1
    std::array<std::atomic<float>, kNumMods>     modView;
    std::atomic<float> viewRegionStart { 0.0f }, viewRegionLen { 1.0f }; // fractions of wrap length
    std::atomic<float> viewScanPos { 0.0f }, viewPosition { 0.5f }, viewSize { 0.1f };
    std::atomic<int>   fileLen { 0 };
    std::atomic<float> viewRegionSeconds { 4.0f };
    std::atomic<bool>  viewHostPlaying { false };

private:
    //--------------------------------------------------------------------------
    struct Voice
    {
        bool    active = false, releasing = false, forceTrigger = false;
        int     note = -1;
        float   velocity = 1.0f, env = 0.0f;
        double  clock = 0.0, scanPhase = 0.0;
        int64_t lastIndex = -1;
        int64_t age = 0;
    };

    struct Grain
    {
        bool   active = false;
        double pos = 0.0, rate = 1.0;
        int    age = 0, len = 1, wrap = 1, voice = 0;
        float  gl = 1.0f, gr = 1.0f, gain = 1.0f, taper = 1.0f;
    };

    struct Source { const float* l; const float* r; int len; };

    struct Current
    {
        float  sizeSec = 0.1f, density = 10.0f, pitch = 0.0f, spray = 0.0f, pan = 0.0f;
        float  taper = 1.0f, freqShift = 0.0f, sliceDuty = 0.5f, feedback = 0.0f, mix = 1.0f;
        float  position = 0.5f, overlapNorm = 1.0f;
        double clockInc = 0.0, scanInc = 0.0;
    };

    // Two cascades of 2nd-order allpasses giving ~90 degrees of phase difference
    // over most of the audio band (coefficients: O. Niemitalo).
    struct Hilbert
    {
        static constexpr double a[4] = { 0.6923878, 0.9360654322959, 0.9882295226860, 0.9987488452737 };
        static constexpr double b[4] = { 0.4021921162426, 0.8561710882420, 0.9722909545651, 0.9952884791278 };
        double ax1[4] {}, ax2[4] {}, ay1[4] {}, ay2[4] {};
        double bx1[4] {}, bx2[4] {}, by1[4] {}, by2[4] {};
        double delayedA = 0.0;

        void reset() { *this = Hilbert(); }

        static double ap (double x, double c, double& x1, double& x2, double& y1, double& y2)
        {
            const double y = c * c * (x + y2) - x2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return y;
        }
        float shift (float in, double cosw, double sinw)
        {
            double ya = in, yb = in;
            for (int k = 0; k < 4; ++k) ya = ap (ya, a[k], ax1[k], ax2[k], ay1[k], ay2[k]);
            for (int k = 0; k < 4; ++k) yb = ap (yb, b[k], bx1[k], bx2[k], by1[k], by2[k]);
            const double re = delayedA; delayedA = ya;
            return (float) (re * cosw - yb * sinw);
        }
    };

    //--------------------------------------------------------------------------
    void installFile (const juce::AudioBuffer<float>& src, double srcRate)
    {
        const int maxLen = (int) std::ceil (kBufferSeconds * sr);
        const double ratio = srcRate / sr;
        const int outLen = juce::jmin (maxLen, (int) std::floor (src.getNumSamples() / ratio));
        juce::AudioBuffer<float> tmp (2, juce::jmax (1, outLen));
        tmp.clear();
        for (int c = 0; c < 2; ++c)
        {
            const int sc = juce::jmin (c, src.getNumChannels() - 1);
            if (sc < 0) break;
            juce::LagrangeInterpolator interp;
            interp.process (ratio, src.getReadPointer (sc), tmp.getWritePointer (c), outLen);
        }
        std::array<float, kNumPeakBins> pk {};
        for (int s = 0; s < outLen; ++s)
        {
            const int bin = (int) ((int64_t) s * kNumPeakBins / juce::jmax (1, outLen));
            pk[(size_t) bin] = juce::jmax (pk[(size_t) bin], std::abs (tmp.getSample (0, s)), std::abs (tmp.getSample (1, s)));
        }

        const juce::SpinLock::ScopedLockType sl (bufferLock);
        fileBuf = std::move (tmp);
        fileLen.store (outLen);
        for (int b = 0; b < kNumPeakBins; ++b) filePeaks[(size_t) b].store (pk[(size_t) b]);
        for (auto& g : grains) g.active = false;
    }

    Source currentSource (int mode) const
    {
        if (mode == ModeFile)
            return { fileBuf.getReadPointer (0), fileBuf.getReadPointer (1), fileLen.load() };
        return { cap.getReadPointer (0), cap.getReadPointer (1), bufLen };
    }

    // region inside the source: start index (circular) and length, in samples
    void region (int mode, double& start, double& len) const
    {
        if (mode == ModeFile) { start = 0.0; len = (double) fileLen.load(); return; }
        len   = juce::jlimit (0.05, kMaxWindowSeconds, (double) lastWindowSec) * sr;
        start = (double) writePos - len;
        if (start < 0.0) start += bufLen;
    }

    static float window (float x, float taper)
    {
        const float a = juce::jlimit (0.01f, 0.5f, taper * 0.5f);
        if (x < a)         return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * x / a);
        if (x > 1.0f - a)  return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (1.0f - x) / a);
        return 1.0f;
    }

    static void readHermite (const Source& s, double pos, float& outL, float& outR)
    {
        const int   i1 = (int) pos;
        const float t  = (float) (pos - i1);
        const int   i0 = i1 == 0 ? s.len - 1 : i1 - 1;
        const int   i2 = i1 + 1 >= s.len ? i1 + 1 - s.len : i1 + 1;
        const int   i3 = i2 + 1 >= s.len ? i2 + 1 - s.len : i2 + 1;
        auto h = [t] (float y0, float y1, float y2, float y3)
        {
            const float c1 = 0.5f * (y2 - y0);
            const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * t + c2) * t + c1) * t + y1;
        };
        outL = h (s.l[i0], s.l[i1], s.l[i2], s.l[i3]);
        outR = h (s.r[i0], s.r[i1], s.r[i2], s.r[i3]);
    }

    static uint32_t slicePatternBits (int pattern, int seed)
    {
        switch (pattern)
        {
            case 0: return 0xFFFFu;
            case 1: return 0xAAAAu;
            case 2: return 0x5555u;
            case 3: return (1u << 0) | (1u << 3) | (1u << 6) | (1u << 8) | (1u << 11) | (1u << 14);
            case 4: return 0xDDDDu;
            case 5: return (1u << 0) | (1u << 5) | (1u << 10) | (1u << 13);
            case 6: return 0x030Fu;
            default:
            {
                uint32_t bits = 1u;
                for (int s = 1; s < 16; ++s) if (hash01 (s, seed, 77) > 0.45f) bits |= (1u << s);
                return bits;
            }
        }
    }

    //--------------------------------------------------------------------------
    void startVoice (int vi, int note, float vel)
    {
        auto& v = voices[(size_t) vi];
        v = Voice();
        v.active = true; v.note = note; v.velocity = vel;
        v.env = isSynth ? 0.0f : 1.0f;
        v.clock = 0.0; v.lastIndex = -1; v.forceTrigger = true;
    }

    void handleMidi (const juce::MidiMessage& m, const Params&)
    {
        if (! isSynth) return;
        if (m.isNoteOn())
        {
            int slot = -1;
            for (int i = 0; i < kMaxVoices && slot < 0; ++i) if (! voices[(size_t) i].active) slot = i;
            if (slot < 0)
            {
                float lowest = 2.0f;
                for (int i = 0; i < kMaxVoices; ++i)
                    if (voices[(size_t) i].releasing && voices[(size_t) i].env < lowest) { lowest = voices[(size_t) i].env; slot = i; }
            }
            if (slot < 0)
            {
                int64_t oldest = -1;
                for (int i = 0; i < kMaxVoices; ++i)
                    if (voices[(size_t) i].age > oldest) { oldest = voices[(size_t) i].age; slot = i; }
            }
            for (auto& v : voices) ++v.age;
            startVoice (slot, m.getNoteNumber(), m.getFloatVelocity());
        }
        else if (m.isNoteOff())
        {
            for (auto& v : voices) if (v.active && v.note == m.getNoteNumber() && ! v.releasing) v.releasing = true;
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            for (auto& v : voices) v.releasing = true;
        }
    }

    //--------------------------------------------------------------------------
    static float modShape (int shape, double phase, int seed, int salt)
    {
        const double f = frac (phase);
        const int64_t cyc = (int64_t) std::floor (phase);
        switch (shape)
        {
            case ShapeSine:     return (float) std::sin (f * juce::MathConstants<double>::twoPi);
            case ShapeTriangle: return (float) (1.0 - 4.0 * std::abs (f - 0.5));
            case ShapeSawUp:    return (float) (2.0 * f - 1.0);
            case ShapeSawDown:  return (float) (1.0 - 2.0 * f);
            case ShapeSquare:   return f < 0.5 ? 1.0f : -1.0f;
            case ShapeStep:     return hash01 (cyc, seed, salt) * 2.0f - 1.0f;
            case ShapeDrift:
            {
                const float a = hash01 (cyc, seed, salt) * 2.0f - 1.0f;
                const float b = hash01 (cyc + 1, seed, salt) * 2.0f - 1.0f;
                const float w = (float) (0.5 - 0.5 * std::cos (f * juce::MathConstants<double>::pi));
                return a + (b - a) * w;
            }
            default: return 0.0f;
        }
    }

    void controlTick (const Params& p, bool hostPlaying, bool resync)
    {
        lastWindowSec = p.windowSec;
        const double dtBlock = kControlRate / sr;
        const double baseSizeSec = juce::jmax (0.003, p.sizeMs / 1000.0);

        std::array<float, NumDests> ms {};
        for (int k = 0; k < kNumMods; ++k)
        {
            const auto& m = p.mods[(size_t) k];
            const double divB = divisionBeats (m.division);
            switch (m.rateMode)
            {
                case RateSync:
                    if (hostPlaying) modPhase[(size_t) k] = ppq / divB;
                    else             modPhase[(size_t) k] += (bpm / 60.0 / divB) * dtBlock;
                    break;
                case RateGrain: modPhase[(size_t) k] = voices[(size_t) leadVoice].clock; break;
                case RateLink:  modPhase[(size_t) k] += (m.rateHz / baseSizeSec) * dtBlock; break;
                default:        modPhase[(size_t) k] += m.rateHz * dtBlock; break;
            }
            const float v = modShape (m.shape, modPhase[(size_t) k], p.seed, 100 + k) * m.depth;
            modView[(size_t) k].store (v);
            if (m.dest > DestNone && m.dest < NumDests) ms[(size_t) m.dest] += v;
        }

        cur.position  = p.position + ms[DestPosition] * 0.5f;
        cur.sizeSec   = (float) juce::jlimit (0.003, 4.0, baseSizeSec * std::pow (2.0, 2.0 * ms[DestSize]));
        cur.density   = juce::jlimit (0.1f, 400.0f, p.density * std::pow (2.0f, 2.0f * ms[DestDensity]));
        cur.pitch     = p.pitch + p.fine / 100.0f + ms[DestPitch] * 12.0f;
        cur.spray     = juce::jlimit (0.0f, 1.0f, p.spray + ms[DestSpray] * 0.5f);
        cur.pan       = ms[DestPan];
        cur.taper     = juce::jlimit (0.0f, 1.0f, p.shape + ms[DestShape] * 0.5f);
        cur.freqShift = p.freqShift + ms[DestFreqShift] * 500.0f;
        cur.sliceDuty = juce::jlimit (0.02f, 1.0f, p.sliceDuty + ms[DestSliceDuty] * 0.5f);
        cur.feedback  = juce::jlimit (0.0f, 0.98f, p.feedback + ms[DestFeedback] * 0.5f);
        cur.mix       = juce::jlimit (0.0f, 1.0f, p.mix + ms[DestMix] * 0.5f);

        const double densMul = std::pow (2.0, 2.0 * ms[DestDensity]);
        const double divB    = divisionBeats (p.division);
        double interval;
        if (p.sync) { cur.clockInc = (bpm / 60.0 / sr) / divB * densMul; interval = divB * 60.0 / bpm / densMul; }
        else        { cur.clockInc = cur.density / sr;                      interval = 1.0 / cur.density; }
        cur.overlapNorm = (float) (1.0 / std::sqrt (juce::jmax (1.0, (double) cur.sizeSec / interval)));

        const double scanSpeed = p.scanSpeed + ms[DestScanSpeed] * 2.0;
        const double winSec = p.bufferMode == ModeFile ? juce::jmax (0.01, fileLen.load() / sr)
                                                       : juce::jlimit (0.05, kMaxWindowSeconds, (double) p.windowSec);
        cur.scanInc = scanSpeed / (winSec * sr);

        // Re-anchor the effect voice to the song position, so every playback
        // from the same spot produces the same grain sequence.
        if (resync && ! isSynth)
        {
            auto& v = voices[0];
            const double secs = ppq * 60.0 / bpm;
            v.clock = p.sync ? ppq / divB : secs * cur.density;
            v.lastIndex = (int64_t) std::floor (v.clock) - 1;
            double anchor = secs;
            if (p.sliceOn && p.sliceReset)
            {
                const double sb = divisionBeats (p.sliceDiv);
                anchor = (ppq - std::floor (ppq / sb) * sb) * 60.0 / bpm;
            }
            v.scanPhase = anchor * scanSpeed / winSec;
        }

        atkCoef  = (float) (1.0 - std::exp (-1.0 / (juce::jmax (0.5f, p.attackMs)  * 0.001 * sr)));
        relCoef  = (float) (1.0 - std::exp (-1.0 / (juce::jmax (1.0f, p.releaseMs) * 0.001 * sr)));
        compAtk  = (float) (1.0 - std::exp (-1.0 / (juce::jmax (0.1f, p.compAttackMs)  * 0.001 * sr)));
        compRel  = (float) (1.0 - std::exp (-1.0 / (juce::jmax (1.0f, p.compReleaseMs) * 0.001 * sr)));
        gateCoef = (float) (1.0 - std::exp (-1.0 / (0.002 * sr)));
        limRel   = (float) (1.0 - std::exp (-1.0 / (0.08 * sr)));
    }

    double scanOffset (const Voice& v, int scanMode) const
    {
        const double ph = v.scanPhase;
        switch (scanMode)
        {
            case ScanForward:  return frac (ph);
            case ScanReverse:  return -frac (ph);
            case ScanPingPong: { const double t = frac (ph * 0.5) * 2.0; return t <= 1.0 ? t : 2.0 - t; }
            case ScanOneShot:  return juce::jlimit (0.0, 1.0, ph);
            default:           return 0.0;
        }
    }

    void spawnGrain (int vi, int64_t idx, const Params& p)
    {
        const auto& v = voices[(size_t) vi];
        double rStart, rLen;
        region (p.bufferMode, rStart, rLen);
        const Source src = currentSource (p.bufferMode);
        if (rLen < 8.0 || src.len < 8) return;

        const int     patLen = juce::jlimit (1, 64, p.patternLen);
        const int64_t step   = posMod (idx, patLen);

        double pos = cur.position + scanOffset (v, p.scanMode)
                   + (hash01 (step, p.seed, 1) * 2.0f - 1.0f) * cur.spray;
        if (p.scanMode == ScanOneShot) pos = juce::jlimit (0.0, 1.0, pos);
        else                            pos = frac (pos);

        const int len = juce::jmax (16, (int) (cur.sizeSec * sr));
        float semis = cur.pitch + p.shiftStep * (float) posMod (step, juce::jmax (1, p.shiftSteps));
        if (isSynth && v.note >= 0) semis += (float) (v.note - p.rootNote);
        const bool rev = p.grainDir == DirReverse || (p.grainDir == DirAlternate && (step & 1));
        const double speed = std::pow (2.0, semis / 12.0);

        // In Capture mode keep grains behind the write head.
        if (p.bufferMode == ModeCapture)
        {
            const double margin = rev ? 64.0 : juce::jmax (0.0, (speed - 1.0) * len) + 64.0;
            pos = juce::jmin (pos, juce::jmax (0.0, 1.0 - margin / rLen));
        }

        double start = rStart + pos * rLen;
        while (start >= src.len) start -= src.len;

        // pick a free slot, or steal the grain closest to finishing
        int slot = -1; float best = -1.0f;
        for (int g = 0; g < kMaxGrains; ++g)
        {
            if (! grains[(size_t) g].active) { slot = g; break; }
            const float done = (float) grains[(size_t) g].age / (float) grains[(size_t) g].len;
            if (done > best) { best = done; slot = g; }
        }

        float pan = juce::jlimit (-1.0f, 1.0f, p.panSpread * (hash01 (step, p.seed, 2) * 2.0f - 1.0f) + cur.pan);
        const float th = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;

        auto& g  = grains[(size_t) slot];
        g.active = true;
        g.pos    = start;
        g.rate   = rev ? -speed : speed;
        g.age    = 0;
        g.len    = len;
        g.wrap   = src.len;
        g.voice  = vi;
        g.gl     = std::cos (th) * juce::MathConstants<float>::sqrt2;
        g.gr     = std::sin (th) * juce::MathConstants<float>::sqrt2;
        g.gain   = cur.overlapNorm * (isSynth ? v.velocity : 1.0f);
        g.taper  = cur.taper;
    }

    void publishView (const Params& p)
    {
        double rStart, rLen;
        region (p.bufferMode, rStart, rLen);
        const int wrap = currentSource (p.bufferMode).len;
        if (wrap <= 0 || rLen <= 0.0) return;
        viewRegionSeconds.store ((float) (rLen / sr));
        viewHostPlaying.store (wasPlaying);
        viewRegionStart.store ((float) (rStart / wrap));
        viewRegionLen.store ((float) (rLen / wrap));
        for (int g = 0; g < kMaxGrains; ++g)
        {
            const auto& gr = grains[(size_t) g];
            if (! gr.active) { grainView[(size_t) g].store (-1.0f); continue; }
            double d = gr.pos - rStart;
            if (d < 0.0) d += wrap;
            grainView[(size_t) g].store (d <= rLen ? (float) (d / rLen) : -1.0f);
        }
        viewScanPos.store ((float) frac (cur.position + scanOffset (voices[(size_t) leadVoice], p.scanMode)));
        viewPosition.store (cur.position);
        viewSize.store (cur.sizeSec);
    }

    //--------------------------------------------------------------------------
    double sr = 0.0, bpm = 120.0, ppq = 0.0;
    bool   isSynth = false, wasPlaying = false;
    int    bufLen = 0, writePos = 0, lastPeakBin = -1, lastMode = -1, leadVoice = 0;
    float  lastWindowSec = 4.0f;
    int64_t lastSliceStep = -1;

    juce::AudioBuffer<float> cap, fileBuf, dry, pendingFile;
    double pendingFileRate = 44100.0;
    juce::SpinLock bufferLock;

    std::array<Voice, kMaxVoices> voices;
    std::array<Grain, kMaxGrains> grains;
    std::array<double, kNumMods>  modPhase {};
    Current cur;

    float gate = 1.0f, gateCoef = 0.01f, fbL = 0.0f, fbR = 0.0f;
    float compEnv = 0.0f, compAtk = 0.01f, compRel = 0.001f, limGain = 1.0f, limRel = 0.001f;
    float atkCoef = 0.01f, relCoef = 0.001f;
    double fsPhase = 0.0;
    std::array<Hilbert, 2> hilbert;
};

} // namespace facet
