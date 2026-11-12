// One plain sentence per control: shown in the hint bar and read by screen readers.
#pragma once
#include <juce_core/juce_core.h>

namespace facetui
{
inline juce::String describe (const juce::String& id)
{
    static const std::map<juce::String, juce::String> d
    {
        { "mode",        "Where grains come from: Capture keeps recording your track, Freeze holds what was recorded, File plays a dropped sample." },
        { "window",      "How many seconds of recent audio the grains can reach." },
        { "scanMode",    "How the read head travels through the audio: stay still, move forward, backward, bounce, or run once per note." },
        { "scanSpeed",   "How fast the read head travels. 1 means the speed of the original recording." },
        { "feedback",    "Sends the result back into the recording, so the sound piles up on itself. Capture mode only." },
        { "position",    "Where in the audio the grains read from. Right is the newest audio." },
        { "size",        "How long each grain lasts. Short is buzzy and textured, long is smeared and tonal." },
        { "density",     "How many grains start every second, when Sync is off." },
        { "sync",        "Starts grains on the song tempo instead of a fixed rate. While the song plays, the result repeats exactly." },
        { "division",    "Note length between grains when Sync is on." },
        { "spray",       "Scatters each grain's read point around Position. The scatter repeats with the pattern, it is not random." },
        { "pattern",     "After this many grains, the variations (spray, pan, shift) start over. Small numbers sound rhythmic." },
        { "seed",        "Picks a different fixed variation. The same seed always sounds the same." },
        { "pitch",       "Transposes every grain, in semitones." },
        { "fine",        "Fine tuning, in cents." },
        { "direction",   "Plays grains forward, backward, or every other grain backward." },
        { "shape",       "Grain edges: low is hard and clicky, high is soft and smooth." },
        { "pan",         "Spreads grains across the stereo field." },
        { "shiftStep",   "Raises each grain in the pattern by this many semitones, which makes rising or falling arpeggios." },
        { "shiftSteps",  "How many pitch steps the shift climbs before starting again." },
        { "freqShift",   "Moves all frequencies up or down by a fixed amount, for metallic and inharmonic tones." },
        { "sliceOn",     "Chops the output into tempo-synced pieces following a pattern." },
        { "sliceDiv",    "Length of each slice." },
        { "slicePattern","Which slices play and which stay silent." },
        { "sliceDuty",   "How much of each slice sounds before it cuts." },
        { "sliceReset",  "Each slice restarts the read head and fires a grain, so every slice starts at the same place." },
        { "compThresh",  "Level above which the compressor starts turning the grains down." },
        { "compRatio",   "How hard the compressor works. 1 means off." },
        { "compAttack",  "How fast the compressor reacts." },
        { "compRelease", "How fast the compressor lets go." },
        { "compMakeup",  "Adds level back after compression." },
        { "mix",         "Balance between your original sound and the grains." },
        { "out",         "Output level. A safety limiter always protects against clipping." },
        { "root",        "The MIDI note that plays the audio at its original pitch." },
        { "attack",      "How fast each note fades in." },
        { "release",     "How long each note fades out after you let go." },
        { "shape#",      "The movement this modulator makes: smooth waves, ramps, steps, or a slow drift." },
        { "rate#",       "What sets the speed: Hz, the song tempo, one cycle per grain, or linked to grain size." },
        { "hz#",         "Speed of the modulator in Hz (in Link mode, a multiplier)." },
        { "div#",        "Note length of one cycle when the rate mode is Sync." },
        { "depth#",      "How much the modulator moves its target. Negative values move it the other way." },
        { "dest#",       "Which setting this modulator moves." },
        { "pad",         "Grain pad: left and right sets Position, up and down sets grain Size. Arrow keys work too." },
        { "buffer",      "The audio the grains are reading. Triangles are grains playing now, the chevron is the read head." },
    };
    juce::String key = id;
    if (id.startsWith ("m") && id.containsChar ('_'))
        key = id.fromFirstOccurrenceOf ("_", false, false) + "#";
    auto it = d.find (key);
    return it != d.end() ? it->second : juce::String();
}
}
