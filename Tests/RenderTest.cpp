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
        e.process (io, synth ? 0 : 2, facet::Engine::AuxOutputs {}, midi, tr, p);
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

    // 4) four heads, each on its own output; head 2 and 4 unlinked with their own settings
    {
        facet::Params h = q;
        for (int k = 0; k < facet::kNumHeads; ++k) { h.heads[(size_t) k].on = true; h.heads[(size_t) k].bus = k; }
        h.heads[1].link = false; h.heads[1].sizeMs = 40.0f; h.heads[1].density = 60.0f; h.heads[1].grainDir = facet::DirForward;
        h.heads[3].link = false; h.heads[3].sizeMs = 600.0f; h.heads[3].density = 4.0f; h.heads[3].scanMode = facet::ScanPingPong;
        h.heads[2].pitch = -12.0f; h.heads[2].pan = -0.5f;

        auto renderMulti = [&] (std::array<juce::AudioBuffer<float>, 4>& outs)
        {
            facet::Engine e;
            const int block = 256, total = (int) (sr * 4.0);
            e.prepare (sr, block, false);
            e.loadFile (src, sr);
            for (auto& o : outs) { o.setSize (2, total); o.clear(); }
            juce::AudioBuffer<float> io (2, block), auxBuf (6, block);
            facet::Transport tr; tr.playing = true; tr.hasPpq = true;
            for (int pos = 0; pos < total; pos += block)
            {
                io.clear();
                facet::Engine::AuxOutputs aux;
                for (int b = 0; b < 3; ++b) aux[(size_t) b] = { auxBuf.getWritePointer (2 * b), auxBuf.getWritePointer (2 * b + 1) };
                tr.ppq = (pos / sr) * 2.0;
                e.process (io, 2, aux, juce::MidiBuffer(), tr, h);
                for (int c = 0; c < 2; ++c)
                {
                    outs[0].copyFrom (c, pos, io, c, 0, block);
                    for (int b = 0; b < 3; ++b) outs[(size_t) b + 1].copyFrom (c, pos, auxBuf, 2 * b + c, 0, block);
                }
            }
        };
        std::array<juce::AudioBuffer<float>, 4> m1, m2;
        renderMulti (m1);
        renderMulti (m2);
        for (int b = 0; b < 4; ++b)
        {
            ok &= check (m1[(size_t) b], ("multi-out bus " + juce::String (b + 1)).toRawUTF8());
            write (m1[(size_t) b], sr, "facet_head" + juce::String (b + 1) + ".wav");
        }
        bool same = true;
        for (int b = 0; b < 4 && same; ++b)
            for (int i = 0; i < m1[0].getNumSamples(); ++i)
                if (m1[(size_t) b].getSample (0, i) != m2[(size_t) b].getSample (0, i)) { same = false; break; }
        bool differ = false;
        for (int i = 0; i < m1[0].getNumSamples() && ! differ; ++i)
            if (std::abs (m1[1].getSample (0, i) - m1[3].getSample (0, i)) > 1.0e-3f) differ = true;
        std::printf ("%-28s %s\n", "multi-out deterministic", same ? "OK" : "FAIL");
        std::printf ("%-28s %s\n", "heads sound different", differ ? "OK" : "FAIL");
        ok &= same && differ;
    }

    write (src, sr, "facet_source.wav");
    std::printf ("\n%s\n", ok ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return ok ? 0 : 1;
}
