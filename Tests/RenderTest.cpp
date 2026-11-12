// Offline test: runs the engine on a synthetic signal, writes WAVs,
// and checks that two renders from the same song position are identical.
#include <juce_audio_formats/juce_audio_formats.h>
#include "FacetEngine.h"

static juce::AudioBuffer<float> makeSource (double sr, double secs)
{
    juce::AudioBuffer<float> b (2, (int) (sr * secs));
    juce::Random rnd (42);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = i / sr;
        const double f = 110.0 * std::pow (2.0, std::fmod (t, 2.0) * 2.0);   // rising arpeggio-ish sweep
        float s = 0.4f * (float) std::sin (juce::MathConstants<double>::twoPi * f * t);
        if (std::fmod (t, 0.5) < 0.02) s += 0.5f * (rnd.nextFloat() * 2.0f - 1.0f);  // clicks / transients
        b.setSample (0, i, s);
        b.setSample (1, i, s * 0.8f);
    }
    return b;
}

static juce::AudioBuffer<float> render (facet::Params p, bool synth, const juce::AudioBuffer<float>& src,
                                        double sr, double secs, bool useFile)
{
    facet::Engine e;
    const int block = 256;
    e.prepare (sr, block, synth);
    if (useFile) e.loadFile (src, sr);

    const int total = (int) (sr * secs);
    juce::AudioBuffer<float> out (2, total);
    juce::AudioBuffer<float> io (2, block);
    facet::Transport tr; tr.playing = true; tr.hasPpq = true; tr.bpm = 120.0;

    for (int pos = 0; pos < total; pos += block)
    {
        const int n = juce::jmin (block, total - pos);
        io.setSize (2, n, false, false, true);
        for (int c = 0; c < 2; ++c)
            io.copyFrom (c, 0, src, c, pos % (src.getNumSamples() - n), n);
        juce::MidiBuffer midi;
        if (synth)
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
        e.process (io, synth ? 0 : 2, midi, tr, p);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, io, c, 0, n);
    }
    return out;
}

static void write (const juce::AudioBuffer<float>& b, double sr, const juce::String& name)
{
    juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile (name);
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

static bool check (const juce::AudioBuffer<float>& b, const char* label)
{
    float peak = 0.0f; double rms = 0.0; bool finite = true;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float s = b.getSample (c, i);
            if (! std::isfinite (s)) finite = false;
            peak = juce::jmax (peak, std::abs (s));
            rms += (double) s * s;
        }
    rms = std::sqrt (rms / (2.0 * b.getNumSamples()));
    const bool ok = finite && peak <= 1.0f && rms > 1.0e-3;
    std::printf ("%-28s peak %.3f  rms %.4f  finite %d  -> %s\n", label, peak, rms, (int) finite, ok ? "OK" : "FAIL");
    return ok;
}

int main()
{
    const double sr = 48000.0;
    const auto src = makeSource (sr, 6.0);
    bool ok = true;

    // 1) capture + rhythmic pattern + modulators + shift + slice + feedback
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

    const auto a = render (p, false, src, sr, 8.0, false);
    const auto b = render (p, false, src, sr, 8.0, false);
    ok &= check (a, "fx capture (full chain)");
    bool identical = true;
    for (int c = 0; c < 2 && identical; ++c)
        for (int i = 0; i < a.getNumSamples(); ++i)
            if (a.getSample (c, i) != b.getSample (c, i)) { identical = false; break; }
    std::printf ("%-28s %s\n", "deterministic re-render", identical ? "OK (bit-identical)" : "FAIL");
    ok &= identical;
    write (a, sr, "facet_fx_capture.wav");

    // 2) file mode, free density, forward scan, reverse grains, high pitch
    facet::Params q;
    q.bufferMode = facet::ModeFile; q.sizeMs = 250.0f; q.density = 30.0f; q.spray = 0.1f;
    q.scanMode = facet::ScanForward; q.scanSpeed = 0.25f; q.grainDir = facet::DirReverse; q.pitch = 7.0f;
    q.panSpread = 0.8f;
    const auto c = render (q, false, src, sr, 6.0, true);
    ok &= check (c, "fx file / scan / reverse");
    write (c, sr, "facet_fx_file.wav");

    // 3) instrument: notes play the file, one-shot scan
    facet::Params s = q;
    s.grainDir = facet::DirForward; s.pitch = 0.0f; s.scanMode = facet::ScanOneShot; s.scanSpeed = 1.0f;
    s.attackMs = 10.0f; s.releaseMs = 200.0f;
    const auto d = render (s, true, src, sr, 6.0, true);
    ok &= check (d, "instrument (MIDI notes)");
    write (d, sr, "facet_instrument.wav");

    write (src, sr, "facet_source.wav");
    std::printf ("\n%s\n", ok ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return ok ? 0 : 1;
}
