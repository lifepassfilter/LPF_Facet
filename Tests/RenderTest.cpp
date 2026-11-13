// Offline test: runs the engine on a synthetic signal, writes WAVs, and checks
// levels, determinism, play modes and the multichannel output formats.
#include <juce_audio_formats/juce_audio_formats.h>
#include "FacetEngine.h"

static juce::AudioBuffer<float> makeSource (double sr, double secs)
{
    juce::AudioBuffer<float> b (2, (int) (sr * secs));
    juce::Random rnd (42);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = i / sr;
        const double f = 110.0 * std::pow (2.0, std::fmod (t, 2.0) * 2.0);   // rising sweep
        float s = 0.4f * (float) std::sin (juce::MathConstants<double>::twoPi * f * t);
        if (std::fmod (t, 0.5) < 0.02) s += 0.5f * (rnd.nextFloat() * 2.0f - 1.0f);  // transients
        b.setSample (0, i, s);
        b.setSample (1, i, s * 0.8f);
    }
    return b;
}

struct Result
{
    juce::AudioBuffer<float> audio;
    float minEl = 0.0f, maxEl = 0.0f;
};

// channels: output width given by the "host". notes: send MIDI notes.
static Result render (const facet::Params& p, const juce::AudioBuffer<float>& src, double sr, double secs,
                      bool useFile, int channels = 2, bool notes = false, bool feedInput = true)
{
    facet::Engine e;
    const int block = 256;
    e.prepare (sr, block);
    if (useFile) e.loadFile (src, sr);

    const int total = (int) (sr * secs);
    Result r;
    r.audio.setSize (channels, total);
    r.audio.clear();
    juce::AudioBuffer<float> io (channels, block);
    facet::Transport tr; tr.playing = true; tr.hasPpq = true; tr.bpm = 120.0;

    for (int pos = 0; pos < total; pos += block)
    {
        const int n = juce::jmin (block, total - pos);
        io.setSize (channels, n, false, false, true);
        io.clear();
        if (feedInput)
            for (int c = 0; c < 2; ++c)
                io.copyFrom (c, 0, src, c, pos % (src.getNumSamples() - n), n);
        juce::MidiBuffer midi;
        if (notes)
        {
            const int beatLen = (int) (sr * 0.5);
            for (int i = 0; i < n; ++i)
            {
                const int s = pos + i;
                if (s % beatLen == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 60 + ((s / beatLen) % 4) * 3, (juce::uint8) 100), i);
                if (s % beatLen == beatLen / 2) midi.addEvent (juce::MidiMessage::allNotesOff (1), i);
            }
        }
        tr.ppq = (pos / sr) * tr.bpm / 60.0;
        e.process (io, feedInput ? 2 : 0, midi, tr, p);
        for (int c = 0; c < channels; ++c) r.audio.copyFrom (c, pos, io, c, 0, n);
        for (auto& el : e.grainEl)
        {
            r.minEl = juce::jmin (r.minEl, el.load());
            r.maxEl = juce::jmax (r.maxEl, el.load());
        }
    }
    return r;
}

static void write (const juce::AudioBuffer<float>& b, double sr, const juce::String& name)
{
    juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile (name);
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr,
                                                                     (unsigned int) b.getNumChannels(), 24, {}, 0));
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

static double rmsOf (const juce::AudioBuffer<float>& b, int ch)
{
    double s = 0.0;
    for (int i = 0; i < b.getNumSamples(); ++i) s += (double) b.getSample (ch, i) * b.getSample (ch, i);
    return std::sqrt (s / b.getNumSamples());
}

static bool check (const juce::AudioBuffer<float>& b, const char* label, int channelsThatMustSound = 2)
{
    float peak = 0.0f; bool finite = true;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float s = b.getSample (c, i);
            if (! std::isfinite (s)) finite = false;
            peak = juce::jmax (peak, std::abs (s));
        }
    int sounding = 0;
    for (int c = 0; c < b.getNumChannels(); ++c) sounding += rmsOf (b, c) > 1.0e-4 ? 1 : 0;
    const bool ok = finite && peak <= 1.0f && sounding >= channelsThatMustSound;
    std::printf ("%-34s peak %.3f  channels sounding %2d/%-2d  -> %s\n", label, peak, sounding, b.getNumChannels(), ok ? "OK" : "FAIL");
    return ok;
}

static bool identical (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < a.getNumChannels(); ++c)
        for (int i = 0; i < a.getNumSamples(); ++i)
            if (a.getSample (c, i) != b.getSample (c, i)) return false;
    return true;
}

int main()
{
    const double sr = 48000.0;
    const auto src = makeSource (sr, 6.0);
    bool ok = true;

    // 1) Audio mode, capture + rhythmic pattern + modulators + shift + slice + feedback (stereo)
    facet::Params p;
    p.bufferMode = facet::ModeCapture; p.windowSec = 3.0f; p.position = 0.6f;
    p.sizeMs = 90.0f; p.sync = true; p.division = 6; p.spray = 0.3f; p.patternLen = 6; p.seed = 7;
    p.shiftStep = 5.0f; p.shiftSteps = 3; p.grainDir = facet::DirAlternate;
    p.scanMode = facet::ScanPingPong; p.scanSpeed = 0.5f; p.feedback = 0.3f;
    p.sliceOn = true; p.slicePattern = 3; p.sliceReset = true; p.freqShift = 40.0f;
    p.compRatio = 3.0f; p.compThresh = -18.0f; p.compMakeup = 4.0f;
    p.mods[0] = { facet::ShapeDrift, facet::RateSync, 3, facet::DestPosition, 1.0f, 0.3f };
    p.mods[1] = { facet::ShapeTriangle, facet::RateGrain, 4, facet::DestPan, 1.0f, 0.8f };
    p.mods[2] = { facet::ShapeSine, facet::RateLink, 4, facet::DestSize, 0.25f, 0.2f };
    const auto a = render (p, src, sr, 8.0, false);
    const auto b = render (p, src, sr, 8.0, false);
    ok &= check (a.audio, "audio mode, capture (full chain)");
    const bool det = identical (a.audio, b.audio);
    std::printf ("%-34s %s\n", "deterministic re-render", det ? "OK (bit-identical)" : "FAIL");
    ok &= det;
    write (a.audio, sr, "facet_fx_capture.wav");

    // 2) file, free density, forward scan, reverse grains
    facet::Params q;
    q.bufferMode = facet::ModeFile; q.sizeMs = 250.0f; q.density = 30.0f; q.spray = 0.1f;
    q.scanMode = facet::ScanForward; q.scanSpeed = 0.25f; q.grainDir = facet::DirReverse; q.pitch = 7.0f;
    q.panSpread = 0.8f;
    const auto c = render (q, src, sr, 6.0, true);
    ok &= check (c.audio, "audio mode, file / scan / reverse");
    write (c.audio, sr, "facet_fx_file.wav");

    // 3) MIDI mode: silent without notes, plays with notes
    facet::Params m = q;
    m.playMode = facet::PlayMidi; m.grainDir = facet::DirForward; m.pitch = 0.0f;
    m.scanMode = facet::ScanOneShot; m.scanSpeed = 1.0f; m.attackMs = 10.0f; m.releaseMs = 200.0f;
    const auto silent = render (m, src, sr, 2.0, true, 2, false, false);
    const bool isSilent = rmsOf (silent.audio, 0) < 1.0e-6 && rmsOf (silent.audio, 1) < 1.0e-6;
    std::printf ("%-34s %s\n", "MIDI mode silent without notes", isSilent ? "OK" : "FAIL");
    ok &= isSilent;
    const auto d = render (m, src, sr, 6.0, true, 2, true, false);
    ok &= check (d.audio, "MIDI mode (notes)");
    write (d.audio, sr, "facet_midi.wav");

    // 4) Both: continuous texture + notes on top (louder than Audio alone)
    facet::Params bo = q;
    bo.playMode = facet::PlayBoth;
    const auto e1 = render (q, src, sr, 4.0, true, 2, true);
    const auto e2 = render (bo, src, sr, 4.0, true, 2, true);
    const bool more = rmsOf (e2.audio, 0) > rmsOf (e1.audio, 0) * 1.05;
    ok &= check (e2.audio, "both mode");
    std::printf ("%-34s %s\n", "both = texture + notes", more ? "OK" : "FAIL");
    ok &= more;

    // 5) head stems: four heads on four pairs, two unlinked
    {
        facet::Params h = q;
        h.output = facet::kStemsChoice;
        for (int k = 0; k < facet::kNumHeads; ++k) { h.heads[(size_t) k].on = true; h.heads[(size_t) k].bus = k; }
        h.heads[1].link = false; h.heads[1].sizeMs = 40.0f; h.heads[1].density = 60.0f;
        h.heads[3].link = false; h.heads[3].sizeMs = 600.0f; h.heads[3].density = 4.0f; h.heads[3].scanMode = facet::ScanPingPong;
        h.heads[2].pitch = -12.0f;
        const auto s1 = render (h, src, sr, 4.0, true, 8);
        const auto s2 = render (h, src, sr, 4.0, true, 8);
        ok &= check (s1.audio, "head stems (8 ch)", 8);
        const bool same = identical (s1.audio, s2.audio);
        bool differ = false;
        for (int i = 0; i < s1.audio.getNumSamples() && ! differ; ++i)
            if (std::abs (s1.audio.getSample (2, i) - s1.audio.getSample (6, i)) > 1.0e-3f) differ = true;
        std::printf ("%-34s %s\n", "stems deterministic + distinct", same && differ ? "OK" : "FAIL");
        ok &= same && differ;
        write (s1.audio, sr, "facet_stems_8ch.wav");
    }

    // 6) 7.1.4: four heads placed around, buffer position, rotating
    {
        facet::Params s = q;
        s.output = 17;   // 7.1.4
        s.placeBy = facet::PlaceBuffer; s.height = facet::HeightDome; s.rotateSpeed = 45.0f; s.scatter = 0.2f;
        for (int k = 0; k < facet::kNumHeads; ++k) s.heads[(size_t) k].on = true;
        const auto r = render (s, src, sr, 6.0, true, 12);
        ok &= check (r.audio, "7.1.4 (12 ch), dome", 11);    // LFE off: 11 channels sound
        write (r.audio, sr, "facet_714.wav");
    }

    // 7) Ambisonics 3rd order (16 ch), Sphere: grains below and above
    {
        facet::Params s = q;
        s.output = 11;   // Ambisonics 3rd order
        s.placeBy = facet::PlaceStep; s.patternLen = 16; s.height = facet::HeightSphere;
        const auto r = render (s, src, sr, 6.0, true, 16);
        ok &= check (r.audio, "ambisonics 3rd order (16 ch)", 16);
        const bool below = r.minEl < -30.0f, above = r.maxEl > 30.0f;
        std::printf ("%-34s %s (el %.0f..%.0f deg)\n", "sphere reaches below and above", below && above ? "OK" : "FAIL", r.minEl, r.maxEl);
        ok &= below && above;
        write (r.audio, sr, "facet_ambix3.wav");
    }

    write (src, sr, "facet_source.wav");
    std::printf ("\n%s\n", ok ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return ok ? 0 : 1;
}
