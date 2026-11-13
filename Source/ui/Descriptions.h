// One plain sentence per control: shown in the hint bar and read by screen readers.
#pragma once
#include <juce_core/juce_core.h>
#include <map>

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
        { "h#on",        "Turns this read head on. Each head reads its own spot in the audio." },
        { "h#link",      "On: this head follows the main Grain and Scan settings. Off: it uses its own settings (Edit to change them)." },
        { "h#offset",    "How far this head reads from Position. 0.5 means the other half of the audio." },
        { "h#pitch",     "Transposes only this head, in semitones, on top of the main Pitch." },
        { "h#level",     "Volume of this head." },
        { "h#pan",       "Moves this head left or right." },
        { "h#out",       "With Output set to Head stems: the channel pair this head plays on (1-2, 3-4, 5-6, 7-8)." },
        { "h#az",        "Direction of this head when Place by is Each head. 0 is in front, positive to the right." },
        { "h#el",        "Raises or lowers this head's grains, in degrees, on top of the Height setting." },
        { "h#edit",      "Shows this head's own settings on the right." },
        { "h#copy",      "Copies the main Grain and Scan settings into this head, so you start from the current sound." },
        { "h#size",      "Grain length for this head only." },
        { "h#density",   "Grains per second for this head only, when Sync is off." },
        { "h#division",  "Note length between grains for this head, when Sync is on." },
        { "h#spray",     "Scatter of this head's grains around its reading spot." },
        { "h#dir",       "Plays this head's grains forward, backward, or alternating." },
        { "h#shape",     "Grain edges for this head: hard to soft." },
        { "h#scan",      "How this head's read point travels through the audio." },
        { "h#scanSpeed", "How fast this head's read point travels." },
        { "h#pattern",   "After this many grains, this head's variations start over." },
        { "h#seed",      "This head's fixed variation. Same seed, same result." },
        { "playMode",    "Audio: grains play all the time on what goes through the track. MIDI: notes play the grains. Both: the texture plus notes on top." },
        { "output",      "Output format, the same as Sediments: stereo, rings of 4 to 16 speakers, Ambisonics, 5.1 to 9.1.6, or each head on its own stereo pair." },
        { "placeBy",     "What decides where each grain is heard: in front, one direction per head, its place in the buffer, its pattern step, or its pitch." },
        { "height",      "Up and down: flat, a dome above you, a full sphere above and below, or from pitch or grain size." },
        { "width",       "How far around you the grains spread. 0 keeps them in front." },
        { "scatter",     "Moves each grain away from its place. The scatter repeats with the seed." },
        { "rotate",      "Turns the whole space, in degrees per second. Locked to the song when it plays." },
        { "frontCentre", "On a speaker ring: speaker 1 straight ahead instead of a pair on each side of the front." },
        { "useCentre",   "Surround: send grains to the centre speaker too, or keep it free (for dialogue)." },
        { "lfe",         "Surround: feed the LFE channel with the low end of the grains." },
        { "pad",         "Grain pad: left and right sets Position, up and down sets grain Size. Arrow keys work too." },
        { "buffer",      "The audio the grains are reading. Triangles are grains playing now, the chevron is the read head." },
    };
    juce::String key = id;
    if (id.length() > 2 && juce::CharacterFunctions::isDigit (id[1]) && id.containsChar ('_'))
    {
        const auto suffix = id.fromFirstOccurrenceOf ("_", false, false);
        key = id.startsWith ("h") ? "h#" + suffix : suffix + "#";
    }
    auto it = d.find (key);
    return it != d.end() ? it->second : juce::String();
}
}
