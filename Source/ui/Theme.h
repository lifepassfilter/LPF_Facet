// LPF Facet — themes, colour palettes, fonts and the shared display settings.
// Display settings are per user (not per session) and shared by every open instance.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include "BinaryData.h"

namespace facetui
{

struct Theme
{
    juce::Colour bg, panel, panel2, line, text, dim, focus;
    juce::Colour value;   // main signal: your settings
    juce::Colour mod;     // modulation / movement
    juce::Colour warn;    // freeze, limiter, learning
    bool decor = true;    // hatching and chevrons (off in High contrast)
    bool light = false;
};

inline juce::StringArray themeNames()   { return { "Anti-grav (dark)", "Daylight", "High contrast", "Night" }; }
inline juce::StringArray paletteNames() { return { "Standard", "Blue / orange (red-green safe)", "Red / teal (blue-yellow safe)", "Monochrome" }; }
inline juce::StringArray fontNames()    { return { "Atkinson Hyperlegible", "OpenDyslexic", "System font" }; }
inline juce::StringArray scaleNames()   { return { "100 %", "115 %", "130 %", "150 %" }; }
inline int scaleFromIndex (int i)       { static const int s[] = { 100, 115, 130, 150 }; return s[juce::jlimit (0, 3, i)]; }

//==============================================================================
class Settings
{
public:
    Settings()
    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "Facet";
        o.folderName          = "LPF";
        o.filenameSuffix      = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        props = std::make_unique<juce::PropertiesFile> (o);

        theme         = props->getIntValue ("theme", 0);
        palette       = props->getIntValue ("palette", 0);
        font          = props->getIntValue ("font", 0);
        scaleIndex    = props->getIntValue ("scale", 0);
        styled        = props->getBoolValue ("styledHeadings", true);
        reducedMotion = props->getBoolValue ("reducedMotion", false);
        hints         = props->getBoolValue ("hints", true);
        showHelpOnStart = props->getBoolValue ("showHelpOnStart", true);

        auto tf = [] (const char* data, int size) { return juce::Typeface::createSystemTypefaceFor (data, (size_t) size); };
        atkinson     = tf (BinaryData::AtkinsonHyperlegibleRegular_ttf, BinaryData::AtkinsonHyperlegibleRegular_ttfSize);
        atkinsonBold = tf (BinaryData::AtkinsonHyperlegibleBold_ttf,    BinaryData::AtkinsonHyperlegibleBold_ttfSize);
        dyslexic     = tf (BinaryData::OpenDyslexicRegular_otf,         BinaryData::OpenDyslexicRegular_otfSize);
        dyslexicBold = tf (BinaryData::OpenDyslexicBold_otf,            BinaryData::OpenDyslexicBold_otfSize);
        displayFace  = tf (BinaryData::RajdhaniBold_ttf,                BinaryData::RajdhaniBold_ttfSize);
    }

    ~Settings() { save(); }

    int  theme = 0, palette = 0, font = 0, scaleIndex = 0;
    bool styled = true, reducedMotion = false, hints = true, showHelpOnStart = true;

    struct Listener { virtual ~Listener() = default; virtual void displaySettingsChanged() = 0; };
    void addListener (Listener* l)    { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    void changed()
    {
        save();
        listeners.call ([] (Listener& l) { l.displaySettingsChanged(); });
    }

    float scale() const { return (float) scaleFromIndex (scaleIndex) / 100.0f; }

    // Body text: labels, values, hints. Dyslexic font is wider, so it gets a slightly smaller size.
    juce::Font body (float h, bool bold = false) const
    {
        switch (font)
        {
            case 1:  return juce::Font (juce::FontOptions (bold ? dyslexicBold : dyslexic).withHeight (h * 1.3f)).withExtraKerningFactor (-0.06f);
            case 2:  return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
            default: return juce::Font (juce::FontOptions (bold ? atkinsonBold : atkinson).withHeight (h));
        }
    }

    // Headings: condensed display face, or the body font in bold when "styled headings" is off.
    juce::Font display (float h) const
    {
        if (styled) return juce::Font (juce::FontOptions (displayFace).withHeight (h * 1.12f)).withExtraKerningFactor (0.08f);
        return body (h * 0.9f, true);
    }

    Theme makeTheme() const
    {
        Theme t;
        using C = juce::Colour;
        switch (theme)
        {
            case 1: // Daylight
                t = { C (0xffe6e9ed), C (0xfff6f7f9), C (0xffffffff), C (0xffaab3be), C (0xff0b0f14), C (0xff434d5a), C (0xff000000) };
                t.light = true; break;
            case 2: // High contrast
                t = { C (0xff000000), C (0xff000000), C (0xff000000), C (0xffffffff), C (0xffffffff), C (0xffffffff), C (0xffffff00) };
                t.decor = false; break;
            case 3: // Night: dim, warm, low blue
                t = { C (0xff070505), C (0xff0f0b0a), C (0xff151010), C (0xff2e2421), C (0xffd8c9ba), C (0xff9c8a7b), C (0xffffd9b0) };
                break;
            default: // Anti-grav (dark)
                t = { C (0xff06080c), C (0xff0d121a), C (0xff131a24), C (0xff253142), C (0xffeef3f8), C (0xff8f9caf), C (0xffffffff) };
                break;
        }

        // value / modulation / warning, tuned per palette and per light or dark background
        struct P { juce::uint32 v, m, w; };
        static const P dark[4]  = { { 0xffff8a1f, 0xff2fd6ff, 0xffff3d7f },    // orange / cyan / magenta
                                    { 0xffffb000, 0xff6c95ff, 0xfffe6100 },    // amber / blue
                                    { 0xffff5a5a, 0xff3fd7c6, 0xffffffff },    // red / teal
                                    { 0xffffffff, 0xffa9b4c2, 0xffffffff } };
        static const P lightP[4] = { { 0xffb34700, 0xff006c91, 0xffa8004a },
                                    { 0xff9a5200, 0xff1f4fd1, 0xffb83a00 },
                                    { 0xffb3261e, 0xff00766a, 0xff000000 },
                                    { 0xff000000, 0xff4a5563, 0xff000000 } };
        static const P hc[4]     = { { 0xffffff00, 0xff00ffff, 0xffff66ff },
                                    { 0xffffb000, 0xff8fb0ff, 0xffffffff },
                                    { 0xffff7070, 0xff40ffe8, 0xffffffff },
                                    { 0xffffffff, 0xffffffff, 0xffffffff } };
        static const P night[4]  = { { 0xffff9a3d, 0xffd9b26f, 0xffff6a4d },
                                    { 0xffffb000, 0xffb9a6ff, 0xfffe6100 },
                                    { 0xffff6b5a, 0xff7fd1c2, 0xffffd9b0 },
                                    { 0xffd8c9ba, 0xff9c8a7b, 0xffffffff } };
        const P* set = theme == 1 ? lightP : theme == 2 ? hc : theme == 3 ? night : dark;
        const P& p = set[juce::jlimit (0, 3, palette)];
        t.value = C (p.v); t.mod = C (p.m); t.warn = C (p.w);
        return t;
    }

private:
    void save()
    {
        props->setValue ("theme", theme);
        props->setValue ("palette", palette);
        props->setValue ("font", font);
        props->setValue ("scale", scaleIndex);
        props->setValue ("styledHeadings", styled);
        props->setValue ("reducedMotion", reducedMotion);
        props->setValue ("hints", hints);
        props->setValue ("showHelpOnStart", showHelpOnStart);
        props->saveIfNeeded();
    }

    std::unique_ptr<juce::PropertiesFile> props;
    juce::ListenerList<Listener> listeners;
    juce::Typeface::Ptr atkinson, atkinsonBold, dyslexic, dyslexicBold, displayFace;
};

//==============================================================================
// Shape helpers for the angular HUD look
inline juce::Path chamfer (juce::Rectangle<float> r, float c, bool tl = false, bool tr = true, bool br = false, bool bl = true)
{
    c = juce::jmin (c, r.getWidth() * 0.5f, r.getHeight() * 0.5f);
    juce::Path p;
    p.startNewSubPath (r.getX() + (tl ? c : 0.0f), r.getY());
    p.lineTo (r.getRight() - (tr ? c : 0.0f), r.getY());
    if (tr) p.lineTo (r.getRight(), r.getY() + c);
    p.lineTo (r.getRight(), r.getBottom() - (br ? c : 0.0f));
    if (br) p.lineTo (r.getRight() - c, r.getBottom());
    p.lineTo (r.getX() + (bl ? c : 0.0f), r.getBottom());
    if (bl) p.lineTo (r.getX(), r.getBottom() - c);
    p.lineTo (r.getX(), r.getY() + (tl ? c : 0.0f));
    p.closeSubPath();
    return p;
}

inline void drawChevrons (juce::Graphics& g, juce::Rectangle<float> r, int count, juce::Colour c)
{
    const float h = r.getHeight(), w = h * 0.5f;
    for (int i = 0; i < count; ++i)
    {
        const float x = r.getX() + (float) i * (w + 2.0f);
        juce::Path p;
        p.startNewSubPath (x, r.getY());
        p.lineTo (x + w, r.getCentreY());
        p.lineTo (x, r.getBottom());
        g.setColour (c.withMultipliedAlpha (0.35f + 0.65f * (float) (i + 1) / (float) count));
        g.strokePath (p, juce::PathStrokeType (2.0f));
    }
}

} // namespace facetui
