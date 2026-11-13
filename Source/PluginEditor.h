// Facet (Life Pass Filter) — editor. Original angular "anti-grav HUD" look; accessibility first:
// themes, colour-blind palettes, readable fonts, UI size, keyboard control,
// screen-reader titles/descriptions/announcements, plain-language hints.
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/Descriptions.h"

namespace facetui
{
class Content;

//==============================================================================
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit LookAndFeel (Settings& s) : settings (s) { refresh(); }
    void refresh();
    const Theme& theme() const { return t; }
    Settings& settings;

    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override        { return settings.body (14.0f); }
    juce::Font getLabelFont (juce::Label& l) override              { return l.getFont().getHeight() > 0 ? settings.body (l.getFont().getHeight()) : settings.body (13.0f); }
    juce::Font getPopupMenuFont() override                         { return settings.body (15.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return settings.display (15.0f); }
    void drawPopupMenuBackground (juce::Graphics&, int, int) override;

private:
    Theme t;
};

//==============================================================================
// Mixin: report focus so the hint bar and focus ring follow the keyboard.
template <class Base>
class Focusable : public Base
{
public:
    using Base::Base;
    std::function<void (bool)> onFocusChange;
    void focusGained (juce::Component::FocusChangeType c) override { Base::focusGained (c); if (onFocusChange) onFocusChange (true); }
    void focusLost   (juce::Component::FocusChangeType c) override { Base::focusLost (c);   if (onFocusChange) onFocusChange (false); }
};

class Control : public juce::Component
{
public:
    Control (const juce::String& paramID, const juce::String& label, const juce::String& a11y = {});
    void mouseEnter (const juce::MouseEvent&) override;
    void paintOverChildren (juce::Graphics&) override;
protected:
    void focusChanged (bool);
    void showHint();
    juce::String id, labelText, hint, a11yTitle;
    bool focused = false;
};

class Knob : public Control, private juce::Timer
{
public:
    Knob (FacetProcessor&, const juce::String& paramID, const juce::String& label, const juce::String& a11y = {}, bool compact = false);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    class S : public Focusable<juce::Slider>
    {
    public:
        std::function<void()> onMenu, onLearn;
        bool keyPressed (const juce::KeyPress&) override;
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu() && onMenu) onMenu();
            else juce::Slider::mouseDown (e);
        }
    };
    void timerCallback() override;
    void showMenu();
    FacetProcessor& proc;
    S slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    bool learning = false, compact = false;
    juce::String lastMapping;
};

// A labelled push button inside a section grid (Edit, Copy main…)
class Action : public Control
{
public:
    Action (const juce::String& id, const juce::String& label, const juce::String& buttonText,
            const juce::String& a11y, const juce::String& description);
    void resized() override;
    void paint (juce::Graphics&) override;
    Focusable<juce::TextButton> button;
};

class Choice : public Control
{
public:
    Choice (FacetProcessor&, const juce::String& paramID, const juce::String& label, const juce::String& a11y = {});
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    Focusable<juce::ComboBox> box;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

class Toggle : public Control
{
public:
    Toggle (FacetProcessor&, const juce::String& paramID, const juce::String& label, const juce::String& a11y = {});
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    Focusable<juce::ToggleButton> btn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

class ModMeter : public juce::Component, private juce::Timer
{
public:
    ModMeter (FacetProcessor& p, int index);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    FacetProcessor& proc;
    int k;
};

class Section : public juce::Component
{
public:
    Section (juce::String num, juce::String t, int columns, int cellW, int cellH)
        : number (std::move (num)), title (std::move (t)), cols (columns), cw (cellW), ch (cellH)
    {
        setTitle (title);
        setFocusContainerType (FocusContainerType::focusContainer);
        setAccessible (true);
    }
    template <typename C, typename... A> C& add (A&&... args)
    {
        auto* c = new C (std::forward<A> (args)...);
        cells.emplace_back (c);
        addAndMakeVisible (c);
        return *c;
    }
    void setNote (const juce::String& n) { if (n != note) { note = n; repaint(); } }
    void setTitleText (const juce::String& t) { title = t; setTitle (t); repaint(); }
    int preferredWidth() const  { return cols * cw + 20; }
    int preferredHeight() const { return ((int) cells.size() + cols - 1) / cols * ch + 38; }
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    juce::String number, title, note;
    int cols, cw, ch;
    std::vector<std::unique_ptr<juce::Component>> cells;
};

class BufferView : public juce::Component, private juce::Timer
{
public:
    explicit BufferView (FacetProcessor&);
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override;
private:
    void timerCallback() override { repaint(); }
    FacetProcessor& proc;
};

class GrainPad : public juce::Component, private juce::Timer
{
public:
    explicit GrainPad (FacetProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override;
    void focusLost (FocusChangeType) override { repaint(); }
    juce::String valueText() const;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
private:
    void timerCallback() override { repaint(); }
    void set (const juce::MouseEvent&);
    void nudge (float dx, float dy);
    FacetProcessor& proc;
    juce::RangedAudioParameter& px;
    juce::RangedAudioParameter& py;
    std::array<juce::Point<float>, 24> trail {};
    int trailHead = 0;
};

//==============================================================================
// SPACE tab: the room seen from above, the listener in the middle, front at the top.
// Grains appear where they are heard (filled above ear level, hollow below).
// With "Place by: Each head", the numbered head handles can be dragged around.
class RoomView : public juce::Component, private juce::Timer
{
public:
    explicit RoomView (FacetProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
private:
    void timerCallback() override { repaint(); }
    juce::Rectangle<float> circleArea() const;
    juce::Point<float> pointFor (float az, float el) const;
    int headAt (juce::Point<float>) const;
    void setHeadAz (int head, juce::Point<float>);
    FacetProcessor& proc;
    int dragHead = -1;
};

//==============================================================================
// Modal panels: Display & accessibility, Help / quick start
class Panel : public juce::Component
{
public:
    Panel (juce::String t) : title (std::move (t)) { setTitle (title); setFocusContainerType (FocusContainerType::keyboardFocusContainer); setWantsKeyboardFocus (true); }
    std::function<void()> onClose;
    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey && onClose) { onClose(); return true; }
        return false;
    }
    juce::Rectangle<int> card() const { return getLocalBounds().withSizeKeepingCentre (cardW, cardH); }
protected:
    juce::String title;
    int cardW = 560, cardH = 440;
};

class DisplayPanel : public Panel, private Settings::Listener
{
public:
    explicit DisplayPanel (Settings&);
    ~DisplayPanel() override { settings.removeListener (this); }
    void resized() override;
    void paint (juce::Graphics&) override;
    void focusFirst() { themeBox.grabKeyboardFocus(); }
private:
    void displaySettingsChanged() override { sync(); repaint(); }
    void sync();
    Settings& settings;
    juce::ComboBox themeBox, paletteBox, fontBox, scaleBox;
    juce::ToggleButton styledBtn { "Styled headings" }, motionBtn { "Reduce motion" }, hintsBtn { "Show hints bar" };
    juce::TextButton closeBtn { "DONE" };
};

class HelpPanel : public Panel
{
public:
    explicit HelpPanel (Settings&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void focusFirst() { startBtn.grabKeyboardFocus(); }
private:
    Settings& settings;
    juce::ToggleButton onStartBtn { "Show this on start" };
    juce::TextButton startBtn { "START" };
};

//==============================================================================
class Content : public juce::Component, private juce::Timer, private Settings::Listener
{
public:
    Content (FacetProcessor&, Settings&);
    ~Content() override;
    static constexpr int baseW = 1200, baseH = 868;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    void showHint (const juce::String& title, const juce::String& text);
    void announce (const juce::String& text);
    void openDisplay();
    void openHelp();
    void closePanels();

private:
    void timerCallback() override;
    void displaySettingsChanged() override;
    juce::String statusText() const;
    void showTab (int);      // 0 modulation & dynamics, 1 heads, 2 space
    void selectHead (int);

    FacetProcessor& proc;
    Settings& settings;

    juce::Component main;
    BufferView bufferView;
    GrainPad pad;
    Choice modeChoice, playChoice;
    RoomView room;
    juce::TextButton loadBtn { "LOAD AUDIO" }, freezeBtn { "FREEZE" }, helpBtn { "HELP" }, displayBtn { "DISPLAY" };
    std::unique_ptr<juce::FileChooser> chooser;

    Section grain  { "01", "GRAIN", 6, 80, 98 };
    Section buffer { "02", "BUFFER / SCAN", 3, 80, 98 };
    Section shiftS { "03", "SHIFT / SLICE", 4, 80, 98 };
    Section mods   { "04", "MODULATION", 7, 78, 70 };
    Section dyn    { "05", "DYNAMICS / OUT", 4, 80, 98 };
    Section inst   { "06", "NOTES", 3, 76, 98 };
    Section heads  { "07", "HEADS", 8, 98, 52 };
    Section space  { "09", "SPACE", 8, 80, 70 };
    std::array<std::unique_ptr<Section>, facet::kNumHeads> headDetail;
    std::array<Action*, facet::kNumHeads> editButtons {};
    juce::TextButton tabMods { "MODULATION & DYNAMICS" }, tabHeads { "HEADS" }, tabSpace { "SPACE" },
                     linkAllBtn { "LINK ALL" }, unlinkAllBtn { "UNLINK ALL" };
    bool headsView = false;
    int tab = 0;
    int selectedHead = 0;

    DisplayPanel displayPanel;
    HelpPanel helpPanel;

    juce::String hintTitle, hintText, lastStatus;
    int lastMode = -1, lastPlayMode = -1;
};
} // namespace facetui

//==============================================================================
class FacetEditor : public juce::AudioProcessorEditor,
                    public juce::FileDragAndDropTarget,
                    private facetui::Settings::Listener
{
public:
    explicit FacetEditor (FacetProcessor&);
    ~FacetEditor() override;

    void resized() override;
    void paintOverChildren (juce::Graphics&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragging = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override             { dragging = false; repaint(); }

private:
    void displaySettingsChanged() override;
    void applyScale();

    FacetProcessor& proc;
    juce::SharedResourcePointer<facetui::Settings> settings;
    facetui::LookAndFeel lnf;
    facetui::Content content;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FacetEditor)
};
