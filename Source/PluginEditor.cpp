#include "PluginEditor.h"

using namespace facetui;

namespace
{
    const Theme& themeOf (juce::Component& c)
    {
        if (auto* l = dynamic_cast<facetui::LookAndFeel*> (&c.getLookAndFeel())) return l->theme();
        static Theme fallback = Settings().makeTheme();
        return fallback;
    }
    Settings* settingsOf (juce::Component& c)
    {
        if (auto* l = dynamic_cast<facetui::LookAndFeel*> (&c.getLookAndFeel())) return &l->settings;
        return nullptr;
    }
    juce::Font bodyFont (juce::Component& c, float h, bool bold = false)
    {
        if (auto* s = settingsOf (c)) return s->body (h, bold);
        return juce::Font (juce::FontOptions (h));
    }
    juce::Font displayFont (juce::Component& c, float h)
    {
        if (auto* s = settingsOf (c)) return s->display (h);
        return juce::Font (juce::FontOptions (h, juce::Font::bold));
    }
    void focusRing (juce::Graphics& g, juce::Rectangle<float> r, const Theme& t)
    {
        g.setColour (t.focus);
        g.strokePath (chamfer (r.reduced (1.0f), 8.0f), juce::PathStrokeType (2.0f));
    }
}

//==============================================================================
// LookAndFeel
//==============================================================================
void LookAndFeel::refresh()
{
    t = settings.makeTheme();
    setColour (juce::ResizableWindow::backgroundColourId, t.bg);
    setColour (juce::Label::textColourId, t.text);
    setColour (juce::ComboBox::textColourId, t.text);
    setColour (juce::ComboBox::backgroundColourId, t.panel2);
    setColour (juce::ComboBox::outlineColourId, t.line);
    setColour (juce::ComboBox::arrowColourId, t.dim);
    setColour (juce::PopupMenu::backgroundColourId, t.panel2);
    setColour (juce::PopupMenu::textColourId, t.text);
    setColour (juce::PopupMenu::headerTextColourId, t.dim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, t.value);
    setColour (juce::PopupMenu::highlightedTextColourId, t.light ? t.panel2 : t.bg);
    setColour (juce::TextButton::buttonColourId, t.panel2);
    setColour (juce::TextButton::textColourOffId, t.text);
    setColour (juce::TextButton::textColourOnId, t.bg);
    setColour (juce::ToggleButton::textColourId, t.text);
    setColour (juce::Slider::textBoxTextColourId, t.dim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::outlineWhenEditingColourId, t.focus);
    setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, t.value.withAlpha (0.4f));
    setColour (juce::TextEditor::textColourId, t.text);
    setColour (juce::TextEditor::backgroundColourId, t.panel2);
    setColour (juce::TextEditor::focusedOutlineColourId, t.focus);
    setColour (juce::CaretComponent::caretColourId, t.value);
    setColour (juce::TooltipWindow::backgroundColourId, t.panel2);
    setColour (juce::TooltipWindow::textColourId, t.text);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float start, float end, juce::Slider& s)
{
    const auto r   = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (5.0f);
    const float rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    const auto c   = r.getCentre();
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (float) s.valueToProportionOfLength (0.0) : 0.0f;
    const float lo = juce::jmin (from, pos), hi = juce::jmax (from, pos);
    const juce::Colour on = s.isEnabled() ? t.value : t.dim;

    // segmented HUD arc
    constexpr int segs = 25;
    const float segAng = (end - start) / (float) segs;
    for (int i = 0; i < segs; ++i)
    {
        const float mid = ((float) i + 0.5f) / (float) segs;
        const bool lit = (mid >= lo && mid <= hi) || (i == 0 && pos <= 0.0f && ! bipolar && false);
        juce::Path seg;
        seg.addCentredArc (c.x, c.y, rad, rad, 0.0f, start + (float) i * segAng + 0.035f, start + (float) (i + 1) * segAng - 0.035f, true);
        g.setColour (lit ? on : t.line);
        g.strokePath (seg, juce::PathStrokeType (lit ? 4.5f : 3.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
    }

    // hub: chamfered square, pointer
    const float hub = rad * 0.52f;
    g.setColour (t.panel2);
    auto hubR = juce::Rectangle<float> (hub * 2.0f, hub * 2.0f).withCentre (c);
    g.fillPath (chamfer (hubR, hub * 0.45f, true, false, true, false));
    g.setColour (t.line);
    g.strokePath (chamfer (hubR, hub * 0.45f, true, false, true, false), juce::PathStrokeType (1.0f));

    const float ang = start + pos * (end - start);
    g.setColour (t.text);
    g.drawLine ({ c.getPointOnCircumference (hub * 0.15f, ang), c.getPointOnCircumference (rad * 0.80f, ang) }, 2.0f);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    const auto path = chamfer (r, 7.0f);
    g.setColour (on ? t.value : (over ? t.line.brighter (0.15f) : t.panel2));
    g.fillPath (path);
    g.setColour (on ? t.value : t.line);
    g.strokePath (path, juce::PathStrokeType (1.0f));

    // state is also shown as a filled / empty square and ON / OFF, never by colour alone
    const juce::Colour ink = on ? (t.light ? t.panel2 : t.bg) : t.text;
    if (b.getButtonText().isEmpty())
    {
        auto inner = r.withSizeKeepingCentre (54.0f, r.getHeight());
        auto sq = inner.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.3f);
        g.setColour (ink);
        if (on) g.fillRect (sq); else g.drawRect (sq, 1.5f);
        g.setFont (settings.display (15.0f));
        g.drawText (on ? "ON" : "OFF", inner, juce::Justification::centredLeft);
        if (b.hasKeyboardFocus (true)) focusRing (g, b.getLocalBounds().toFloat(), t);
        return;
    }
    auto box = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.3f);
    g.setColour (ink);
    if (on) g.fillRect (box); else g.drawRect (box, 1.5f);
    g.setFont (settings.display (14.0f));
    auto stateR = r.removeFromRight (34.0f);
    g.drawText (on ? "ON" : "OFF", stateR, juce::Justification::centred);
    g.setFont (settings.body (13.0f, true));
    g.drawFittedText (b.getButtonText(), r.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);

    if (b.hasKeyboardFocus (true)) focusRing (g, b.getLocalBounds().toFloat(), t);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const auto path = chamfer (r, 8.0f);
    const bool on = b.getToggleState() || down;
    g.setColour (on ? t.value : (over ? t.line : t.panel2));
    g.fillPath (path);
    g.setColour (on ? t.value : t.line);
    g.strokePath (path, juce::PathStrokeType (1.2f));
    if (b.hasKeyboardFocus (false)) focusRing (g, b.getLocalBounds().toFloat(), t);
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    g.setColour ((b.getToggleState() || down) ? (t.light ? t.panel2 : t.bg) : t.text);
    g.setFont (settings.display (15.0f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 2), juce::Justification::centred, 1, 0.8f);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
    const auto path = chamfer (r, 6.0f);
    g.setColour (t.panel2);
    g.fillPath (path);
    const bool f = box.hasKeyboardFocus (false);
    g.setColour (f ? t.focus : t.line);
    g.strokePath (path, juce::PathStrokeType (f ? 2.0f : 1.0f));

    juce::Path arrow;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.startNewSubPath (ax - 5.0f, ay - 2.5f);
    arrow.lineTo (ax, ay + 2.5f);
    arrow.lineTo (ax + 5.0f, ay - 2.5f);
    g.setColour (box.isEnabled() ? t.value : t.dim);
    g.strokePath (arrow, juce::PathStrokeType (2.0f));
}

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& l)
{
    l.setBounds (6, 1, box.getWidth() - 28, box.getHeight() - 2);
    l.setFont (settings.body (14.0f));
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (t.panel2);
    g.setColour (t.line);
    g.drawRect (0, 0, w, h, 1);
}

//==============================================================================
// Controls
//==============================================================================
Control::Control (const juce::String& paramID, const juce::String& label, const juce::String& a11y)
    : id (paramID), labelText (label), hint (describe (paramID)), a11yTitle (a11y.isEmpty() ? label : a11y)
{
    addMouseListener (this, true);
}

void Control::mouseEnter (const juce::MouseEvent&) { showHint(); }

void Control::focusChanged (bool f)
{
    focused = f;
    repaint();
    if (f) showHint();
}

void Control::showHint()
{
    if (auto* c = findParentComponentOfClass<Content>()) c->showHint (labelText, hint);
}

void Control::paintOverChildren (juce::Graphics& g)
{
    if (focused) focusRing (g, getLocalBounds().toFloat(), themeOf (*this));
}

//------------------------------------------------------------------------------
bool Knob::S::keyPressed (const juce::KeyPress& k)
{
    const int kc = k.getKeyCode();
    const bool fine = k.getModifiers().isShiftDown();
    double d = 0.0;
    if (kc == juce::KeyPress::upKey || kc == juce::KeyPress::rightKey)        d = fine ? 0.002 : 0.01;
    else if (kc == juce::KeyPress::downKey || kc == juce::KeyPress::leftKey)  d = fine ? -0.002 : -0.01;
    else if (kc == juce::KeyPress::pageUpKey)   d = 0.1;
    else if (kc == juce::KeyPress::pageDownKey) d = -0.1;
    else if (kc == juce::KeyPress::homeKey) { setValue (getMinimum(), juce::sendNotificationSync); return true; }
    else if (kc == juce::KeyPress::endKey)  { setValue (getMaximum(), juce::sendNotificationSync); return true; }
    else if (kc == juce::KeyPress::deleteKey || kc == juce::KeyPress::backspaceKey)
    {
        setValue (getDoubleClickReturnValue(), juce::sendNotificationSync);
        return true;
    }
    else if (k.getTextCharacter() == 'm' || k.getTextCharacter() == 'M')
    {
        if (onLearn) onLearn();
        return true;
    }
    else return juce::Slider::keyPressed (k);

    const double cur = getValue();
    double nv = proportionOfLengthToValue (juce::jlimit (0.0, 1.0, valueToProportionOfLength (cur) + d));
    const double interval = getInterval();
    if (interval > 0.0)
    {
        nv = getMinimum() + interval * std::round ((nv - getMinimum()) / interval);
        if (std::abs (nv - cur) < interval * 0.5) nv = cur + (d > 0.0 ? interval : -interval);
    }
    setValue (juce::jlimit (getMinimum(), getMaximum(), nv), juce::sendNotificationSync);
    return true;
}

Knob::Knob (FacetProcessor& p, const juce::String& paramID, const juce::String& label, const juce::String& a11y, bool isCompact)
    : Control (paramID, label, a11y), proc (p), compact (isCompact)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    if (compact) slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 16);
    else         slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 17);
    slider.setWantsKeyboardFocus (true);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setTitle (a11yTitle);
    slider.setDescription (hint);
    slider.setHelpText ("Arrows adjust, Shift for fine, Page Up and Down for big steps, Delete resets, M for MIDI learn.");
    slider.onFocusChange = [this] (bool f) { focusChanged (f); };
    slider.onMenu  = [this] { showMenu(); };
    slider.onLearn = [this]
    {
        proc.startMidiLearn (id);
        if (auto* c = findParentComponentOfClass<Content>()) c->announce ("MIDI learn for " + labelText + ": move a controller.");
    };
    addAndMakeVisible (slider);

    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, paramID, slider);
    if (auto* prm = p.apvts.getParameter (paramID))
        slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    lastMapping = proc.midiLearnInfo (id);
    startTimerHz (6);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (compact ? 16 : 19);
    slider.setBounds (r.withTrimmedBottom (2));
}

void Knob::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    auto top = getLocalBounds().removeFromTop (19).toFloat();
    if (learning)
    {
        g.setColour (t.warn);
        g.drawRect (getLocalBounds().toFloat().reduced (1.0f), 1.5f);
        g.setFont (displayFont (*this, 13.0f));
        g.drawText ("MOVE A CONTROL", top, juce::Justification::centred);
        return;
    }
    g.setColour (t.text);
    g.setFont (bodyFont (*this, 13.0f, true));
    g.drawFittedText (labelText, top.toNearestInt(), juce::Justification::centred, 1, 0.8f);
    if (lastMapping.isNotEmpty())
    {
        g.setColour (t.mod);
        g.setFont (bodyFont (*this, 10.0f, true));
        g.drawText (lastMapping.replace ("CC ", "CC"), getLocalBounds().toFloat().removeFromRight (30.0f).removeFromTop (36.0f).withTrimmedTop (18.0f),
                    juce::Justification::centredRight);
    }
}

void Knob::timerCallback()
{
    const bool l = proc.isLearning (id);
    const auto mapping = proc.midiLearnInfo (id);
    if (l != learning || mapping != lastMapping)
    {
        if (learning && ! l && mapping.isNotEmpty() && mapping != lastMapping)
            if (auto* c = findParentComponentOfClass<Content>()) c->announce (labelText + " mapped to " + mapping);
        learning = l;
        lastMapping = mapping;
        repaint();
    }
}

void Knob::showMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader (lastMapping.isEmpty() ? labelText + ": no MIDI mapping" : labelText + ": " + lastMapping);
    m.addItem (1, "MIDI learn (then move a controller)");
    m.addItem (2, "Clear MIDI mapping", lastMapping.isNotEmpty());
    m.addSeparator();
    m.addItem (3, "Reset to default");
    juce::Component::SafePointer<Knob> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slider), [safe] (int r)
    {
        if (safe == nullptr) return;
        if (r == 1) safe->slider.onLearn();
        if (r == 2) safe->proc.clearMidiLearn (safe->id);
        if (r == 3) safe->slider.setValue (safe->slider.getDoubleClickReturnValue(), juce::sendNotificationSync);
    });
}

//------------------------------------------------------------------------------
Action::Action (const juce::String& paramID, const juce::String& label, const juce::String& buttonText,
                const juce::String& a11y, const juce::String& description)
    : Control (paramID, label, a11y)
{
    hint = description;
    button.setButtonText (buttonText);
    button.setTitle (a11yTitle);
    button.setDescription (description);
    button.setWantsKeyboardFocus (true);
    button.onFocusChange = [this] (bool f) { focusChanged (f); };
    addAndMakeVisible (button);
}

void Action::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (16);
    button.setBounds (r.withSizeKeepingCentre (r.getWidth() - 6, 30).withY (r.getY() + juce::jmax (1, (r.getHeight() - 30) / 2 - 4)));
}

void Action::paint (juce::Graphics& g)
{
    g.setColour (themeOf (*this).text);
    g.setFont (bodyFont (*this, 12.5f, true));
    g.drawFittedText (labelText, getLocalBounds().removeFromTop (16), juce::Justification::centred, 1, 0.8f);
}

//------------------------------------------------------------------------------
Choice::Choice (FacetProcessor& p, const juce::String& paramID, const juce::String& label, const juce::String& a11y)
    : Control (paramID, label, a11y)
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (paramID)))
        box.addItemList (c->choices, 1);
    box.setWantsKeyboardFocus (true);
    box.setTitle (a11yTitle);
    box.setDescription (hint);
    box.onFocusChange = [this] (bool f) { focusChanged (f); };
    addAndMakeVisible (box);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, paramID, box);
}

void Choice::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (19);
    box.setBounds (r.withSizeKeepingCentre (r.getWidth() - 6, 30).withY (r.getY() + juce::jmax (2, (r.getHeight() - 30) / 2 - 8)));
}

void Choice::paint (juce::Graphics& g)
{
    g.setColour (themeOf (*this).text);
    g.setFont (bodyFont (*this, 13.0f, true));
    g.drawFittedText (labelText, getLocalBounds().removeFromTop (19), juce::Justification::centred, 1, 0.8f);
}

Toggle::Toggle (FacetProcessor& p, const juce::String& paramID, const juce::String& label, const juce::String& a11y)
    : Control (paramID, label, a11y)
{
    btn.setClickingTogglesState (true);
    btn.setWantsKeyboardFocus (true);
    btn.setTitle (a11yTitle);
    btn.setDescription (hint);
    btn.onFocusChange = [this] (bool f) { focusChanged (f); };
    addAndMakeVisible (btn);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, paramID, btn);
}

void Toggle::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (19);
    btn.setBounds (r.withSizeKeepingCentre (r.getWidth() - 6, 32).withY (r.getY() + juce::jmax (2, (r.getHeight() - 32) / 2 - 8)));
}

void Toggle::paint (juce::Graphics& g)
{
    g.setColour (themeOf (*this).text);
    g.setFont (bodyFont (*this, 13.0f, true));
    g.drawFittedText (labelText, getLocalBounds().removeFromTop (19), juce::Justification::centred, 1, 0.8f);
}

//------------------------------------------------------------------------------
ModMeter::ModMeter (FacetProcessor& p, int index) : proc (p), k (index)
{
    setAccessible (false);
    startTimerHz (30);
}

void ModMeter::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    auto r = getLocalBounds().toFloat().reduced (6.0f, 0.0f);
    g.setColour (t.mod);
    g.setFont (displayFont (*this, 16.0f));
    g.drawText ("MOD " + juce::String (k + 1), r.removeFromTop (19.0f), juce::Justification::centredLeft);

    // segmented bipolar meter, filled from the centre
    const float v = juce::jlimit (-1.0f, 1.0f, proc.engine.modView[(size_t) k].load());
    auto bar = r.withSizeKeepingCentre (r.getWidth(), 12.0f).withY (r.getY() + 14.0f);
    constexpr int segs = 12;
    const float sw = bar.getWidth() / (float) segs;
    for (int i = 0; i < segs; ++i)
    {
        const float mid = ((float) i + 0.5f) / (float) segs * 2.0f - 1.0f;
        const bool lit = (v >= 0.0f) ? (mid >= 0.0f && mid <= v) : (mid <= 0.0f && mid >= v);
        auto s = juce::Rectangle<float> (bar.getX() + (float) i * sw, bar.getY(), sw - 2.0f, bar.getHeight());
        g.setColour (lit ? t.mod : t.line);
        if (lit) g.fillRect (s); else g.drawRect (s, 1.0f);
    }
    g.setColour (t.dim);
    g.drawVerticalLine ((int) bar.getCentreX() - 1, bar.getY() - 3.0f, bar.getBottom() + 3.0f);
}

//------------------------------------------------------------------------------
void Section::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const auto path = chamfer (r, 16.0f);
    g.setColour (t.panel);
    g.fillPath (path);
    g.setColour (t.line);
    g.strokePath (path, juce::PathStrokeType (1.0f));

    g.setColour (t.value);
    g.setFont (displayFont (*this, 22.0f));
    g.drawText (number, 12, 6, 34, 24, juce::Justification::left);
    g.fillRect (12.0f, 30.0f, 22.0f, 3.0f);
    g.setColour (t.text);
    g.setFont (displayFont (*this, 18.0f));
    g.drawText (title, 44, 7, getWidth() - 100, 24, juce::Justification::left);
    if (note.isNotEmpty())
    {
        g.setColour (t.warn);
        g.setFont (displayFont (*this, 14.0f));
        g.drawText (note, juce::Rectangle<float> (r.getRight() - 120.0f, 8.0f, 106.0f, 20.0f), juce::Justification::centredRight);
    }
    else if (t.decor) drawChevrons (g, { r.getRight() - 52.0f, 12.0f, 40.0f, 12.0f }, 3, t.dim);
}

void Section::resized()
{
    for (size_t i = 0; i < cells.size(); ++i)
        if (cells[i] != nullptr)
            cells[i]->setBounds (10 + (int) (i % (size_t) cols) * cw, 36 + (int) (i / (size_t) cols) * ch, cw, ch);
}

//------------------------------------------------------------------------------
BufferView::BufferView (FacetProcessor& p) : proc (p)
{
    setTitle ("Buffer");
    setDescription (describe ("buffer"));
    setAccessible (true);
    startTimerHz (30);
}

void BufferView::mouseEnter (const juce::MouseEvent&)
{
    if (auto* c = findParentComponentOfClass<Content>()) c->showHint ("Buffer", describe ("buffer"));
}

void BufferView::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const auto path = chamfer (r, 18.0f, false, true, false, true);
    g.setColour (t.panel);
    g.fillPath (path);
    g.setColour (t.line);
    g.strokePath (path, juce::PathStrokeType (1.0f));

    auto& e = proc.engine;
    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    const bool fileMode = mode == facet::ModeFile;
    auto& peaks = fileMode ? e.filePeaks : e.capPeaks;

    // header line
    auto head = r.reduced (16.0f, 0.0f).removeFromTop (34.0f);
    g.setFont (displayFont (*this, 18.0f));
    juce::String tag = fileMode ? "FILE" : (mode == facet::ModeFreeze ? "FROZEN" : "CAPTURING");
    g.setColour (mode == facet::ModeFreeze ? t.warn : t.value);
    g.drawText (tag, head, juce::Justification::centredLeft);
    g.setColour (t.dim);
    g.setFont (bodyFont (*this, 13.0f));
    const juce::String detail = fileMode ? proc.loadedFileName
                                         : juce::String (e.viewRegionSeconds.load(), 1) + " s window";
    g.drawText (detail, head.withTrimmedLeft (110.0f), juce::Justification::centredLeft);

    auto area = r.reduced (16.0f, 0.0f).withTrimmedTop (36.0f).withTrimmedBottom (26.0f);

    if (fileMode && ! e.hasFile())
    {
        g.setColour (t.dim);
        g.setFont (bodyFont (*this, 16.0f));
        g.drawFittedText ("Drop an audio file anywhere on the window, or press LOAD AUDIO.\nOr switch Buffer to Capture to grab your track.",
                          area.toNearestInt(), juce::Justification::centred, 3);
        return;
    }

    const float rs = e.viewRegionStart.load(), rl = e.viewRegionLen.load();
    const float mid = area.getCentreY(), half = area.getHeight() * 0.48f;

    // segmented columns
    g.setColour (t.text.withAlpha (t.light ? 0.55f : 0.5f));
    const int w = (int) area.getWidth();
    for (int x = 0; x < w; x += 4)
    {
        float pk = 0.0f;
        for (int dx = 0; dx < 4; ++dx)
        {
            const double f = facet::frac ((double) rs + (double) (x + dx) / w * rl);
            pk = juce::jmax (pk, peaks[(size_t) juce::jlimit (0, facet::kNumPeakBins - 1, (int) (f * facet::kNumPeakBins))].load());
        }
        pk = juce::jmin (1.0f, pk);
        const float bh = juce::jmax (1.0f, pk * half);
        g.fillRect (area.getX() + (float) x, mid - bh, 3.0f, bh * 2.0f);
    }

    // time ruler
    auto ruler = r.reduced (16.0f, 0.0f).removeFromBottom (24.0f);
    const float secs = juce::jmax (0.01f, e.viewRegionSeconds.load());
    const float stepS = secs > 6.0f ? 1.0f : secs > 2.0f ? 0.5f : secs > 0.6f ? 0.1f : 0.05f;
    g.setFont (bodyFont (*this, 11.0f));
    for (float s = 0.0f; s <= secs + 1.0e-4f; s += stepS)
    {
        const float x = area.getX() + (fileMode ? s / secs : 1.0f - s / secs) * area.getWidth();
        g.setColour (t.line);
        g.drawVerticalLine ((int) x, ruler.getY(), ruler.getY() + 6.0f);
        const int every = stepS < 0.1f ? 4 : 2;
        if (((int) std::round (s / stepS)) % every == 0)
        {
            g.setColour (t.dim);
            g.drawText ((fileMode ? "" : "-") + juce::String (s, stepS < 1.0f ? 1 : 0) + "s",
                        juce::Rectangle<float> (x - 24.0f, ruler.getY() + 6.0f, 48.0f, 16.0f), juce::Justification::centred);
        }
    }

    // read head (scan): chevron + dashed line, in modulation colour
    const float sx = area.getX() + e.viewScanPos.load() * area.getWidth();
    g.setColour (t.mod);
    juce::Path chev;
    chev.addTriangle (sx - 7.0f, area.getY() - 2.0f, sx + 7.0f, area.getY() - 2.0f, sx, area.getY() + 8.0f);
    g.fillPath (chev);
    const float dashes[] = { 4.0f, 4.0f };
    g.drawDashedLine ({ sx, area.getY() + 8.0f, sx, area.getBottom() }, dashes, 2, 1.5f);

    // read heads: numbered diamonds at the bottom of the wave area
    int headsOn = 0;
    for (auto& hv : e.viewHeadPos) headsOn += hv.load() >= 0.0f ? 1 : 0;
    g.setFont (displayFont (*this, 13.0f));
    for (int h = 0; h < facet::kNumHeads; ++h)
    {
        const float v = e.viewHeadPos[(size_t) h].load();
        if (v < 0.0f || headsOn < 2) continue;
        const float x = area.getX() + v * area.getWidth(), y = area.getBottom() - 9.0f;
        juce::Path d;
        d.addQuadrilateral (x, y - 9.0f, x + 9.0f, y, x, y + 9.0f, x - 9.0f, y);
        g.setColour (t.panel2);
        g.fillPath (d);
        g.setColour (t.value);
        g.strokePath (d, juce::PathStrokeType (1.5f));
        g.setColour (t.text);
        g.drawText (juce::String (h + 1), juce::Rectangle<float> (x - 8.0f, y - 8.0f, 16.0f, 16.0f), juce::Justification::centred);
    }

    // grains playing now: triangles, one lane per head when several heads are on
    g.setColour (t.value);
    const float laneH = headsOn > 1 ? half * 1.4f / (float) facet::kNumHeads : 0.0f;
    for (size_t gi = 0; gi < e.grainView.size(); ++gi)
    {
        const float v = e.grainView[gi].load();
        if (v < 0.0f) continue;
        const float x = area.getX() + v * area.getWidth();
        const float y = mid + ((float) e.grainHead[gi].load() - 1.5f) * laneH;
        juce::Path tri;
        tri.addTriangle (x - 5.0f, y + 5.0f, x + 5.0f, y + 5.0f, x, y - 5.0f);
        g.fillPath (tri);
    }
}

//------------------------------------------------------------------------------
GrainPad::GrainPad (FacetProcessor& p)
    : proc (p), px (*p.apvts.getParameter ("position")), py (*p.apvts.getParameter ("size"))
{
    setWantsKeyboardFocus (true);
    setTitle ("Grain pad");
    setDescription (describe ("pad"));
    setHelpText ("Left and right arrows: position. Up and down arrows: size. Hold Shift for fine steps.");
    startTimerHz (30);
}

juce::String GrainPad::valueText() const
{
    return "Position " + px.getCurrentValueAsText() + ", size " + py.getCurrentValueAsText() + " milliseconds";
}

namespace
{
    class PadValue : public juce::AccessibilityTextValueInterface
    {
    public:
        explicit PadValue (GrainPad& p) : pad (p) {}
        bool isReadOnly() const override { return true; }
        juce::String getCurrentValueAsString() const override { return pad.valueText(); }
        void setValueAsString (const juce::String&) override {}
    private:
        GrainPad& pad;
    };
}

std::unique_ptr<juce::AccessibilityHandler> GrainPad::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::slider, juce::AccessibilityActions(),
                                                         juce::AccessibilityHandler::Interfaces { std::make_unique<PadValue> (*this) });
}

void GrainPad::set (const juce::MouseEvent& e)
{
    px.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, e.position.x / (float) getWidth()));
    py.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, 1.0f - e.position.y / (float) getHeight()));
}

void GrainPad::mouseDown (const juce::MouseEvent& e) { px.beginChangeGesture(); py.beginChangeGesture(); set (e); }
void GrainPad::mouseDrag (const juce::MouseEvent& e) { set (e); }
void GrainPad::mouseUp   (const juce::MouseEvent&)   { px.endChangeGesture(); py.endChangeGesture(); }
void GrainPad::mouseEnter (const juce::MouseEvent&)
{
    if (auto* c = findParentComponentOfClass<Content>()) c->showHint ("Grain pad", describe ("pad"));
}
void GrainPad::focusGained (FocusChangeType)
{
    repaint();
    if (auto* c = findParentComponentOfClass<Content>()) c->showHint ("Grain pad", describe ("pad"));
}

void GrainPad::nudge (float dx, float dy)
{
    if (dx != 0.0f) { px.beginChangeGesture(); px.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, px.getValue() + dx)); px.endChangeGesture(); }
    if (dy != 0.0f) { py.beginChangeGesture(); py.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, py.getValue() + dy)); py.endChangeGesture(); }
    if (auto* h = getAccessibilityHandler()) h->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);
}

bool GrainPad::keyPressed (const juce::KeyPress& k)
{
    const float s = k.getModifiers().isShiftDown() ? 0.002f : 0.01f;
    const int kc = k.getKeyCode();
    if (kc == juce::KeyPress::leftKey)  { nudge (-s, 0.0f); return true; }
    if (kc == juce::KeyPress::rightKey) { nudge (s, 0.0f);  return true; }
    if (kc == juce::KeyPress::upKey)    { nudge (0.0f, s);  return true; }
    if (kc == juce::KeyPress::downKey)  { nudge (0.0f, -s); return true; }
    return false;
}

void GrainPad::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    auto* st = settingsOf (*this);
    const bool reduced = st != nullptr && st->reducedMotion;
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const auto path = chamfer (r, 18.0f, true, false, true, false);
    g.setColour (t.panel);
    g.fillPath (path);

    g.saveState();
    g.reduceClipRegion (path);
    // grid + converging track lines
    g.setColour (t.line.withMultipliedAlpha (t.decor ? 1.0f : 0.45f));
    for (int i = 1; i < 8; ++i)
    {
        g.drawHorizontalLine ((int) (r.getHeight() * (float) i / 8.0f), r.getX(), r.getRight());
        g.drawVerticalLine ((int) (r.getWidth() * (float) i / 8.0f), r.getY(), r.getBottom());
    }
    if (t.decor)
    {
        g.setColour (t.line.withAlpha (0.6f));
        const juce::Point<float> vp (r.getCentreX(), r.getY() - r.getHeight() * 0.6f);
        for (int i = -4; i <= 4; ++i)
            g.drawLine ({ vp, { r.getCentreX() + (float) i * r.getWidth() * 0.22f, r.getBottom() } }, 0.7f);
    }

    // where the engine actually is (after modulation)
    const float mx = juce::jlimit (0.0f, 1.0f, proc.engine.viewPosition.load());
    const float my = py.convertTo0to1 (proc.engine.viewSize.load() * 1000.0f);
    const juce::Point<float> mp (mx * r.getWidth(), (1.0f - my) * r.getHeight());
    if (! reduced)
    {
        trail[(size_t) trailHead] = mp;
        trailHead = (trailHead + 1) % (int) trail.size();
        for (size_t i = 0; i < trail.size(); ++i)
        {
            const auto& pt = trail[(i + (size_t) trailHead) % trail.size()];
            g.setColour (t.mod.withAlpha ((float) i / (float) trail.size() * 0.6f));
            g.fillRect (juce::Rectangle<float> (5.0f, 5.0f).withCentre (pt));
        }
    }
    g.setColour (t.mod);
    g.drawRect (juce::Rectangle<float> (10.0f, 10.0f).withCentre (mp), 1.5f);

    // other read heads (2-4): small numbered marks on the same row
    g.setFont (bodyFont (*this, 11.0f, true));
    for (int h = 1; h < facet::kNumHeads; ++h)
    {
        const float v = proc.engine.viewHeadPos[(size_t) h].load();
        if (v < 0.0f) continue;
        const juce::Point<float> hp (v * r.getWidth(), (1.0f - py.getValue()) * r.getHeight());
        g.setColour (t.value.withAlpha (0.8f));
        g.drawRect (juce::Rectangle<float> (14.0f, 14.0f).withCentre (hp), 1.5f);
        g.setColour (t.text);
        g.drawText (juce::String (h + 1), juce::Rectangle<float> (14.0f, 14.0f).withCentre (hp), juce::Justification::centred);
    }

    // your setting: crosshair + diamond reticle
    const juce::Point<float> up (px.getValue() * r.getWidth(), (1.0f - py.getValue()) * r.getHeight());
    g.setColour (t.value.withAlpha (0.45f));
    g.drawHorizontalLine ((int) up.y, r.getX(), r.getRight());
    g.drawVerticalLine ((int) up.x, r.getY(), r.getBottom());
    juce::Path dia;
    dia.addQuadrilateral (up.x, up.y - 12.0f, up.x + 12.0f, up.y, up.x, up.y + 12.0f, up.x - 12.0f, up.y);
    g.setColour (t.value);
    g.strokePath (dia, juce::PathStrokeType (2.5f));
    g.fillRect (juce::Rectangle<float> (4.0f, 4.0f).withCentre (up));
    g.restoreState();

    g.setColour (hasKeyboardFocus (false) ? t.focus : t.line);
    g.strokePath (path, juce::PathStrokeType (hasKeyboardFocus (false) ? 2.0f : 1.0f));

    // labels
    g.setColour (t.text);
    g.setFont (displayFont (*this, 16.0f));
    g.drawText ("GRAIN PAD", r.reduced (14.0f, 8.0f).removeFromTop (20.0f), juce::Justification::centredRight);
    g.setColour (t.dim);
    g.setFont (bodyFont (*this, 12.0f, true));
    g.drawText ("SIZE ^", r.reduced (10.0f, 8.0f).removeFromTop (18.0f), juce::Justification::centredLeft);
    g.drawText ("POSITION >", r.reduced (10.0f, 6.0f).removeFromBottom (18.0f), juce::Justification::centredRight);
    g.setColour (t.text);
    g.setFont (bodyFont (*this, 13.0f));
    g.drawText (px.getCurrentValueAsText() + "  /  " + py.getCurrentValueAsText() + " ms",
                r.reduced (10.0f, 6.0f).removeFromBottom (18.0f), juce::Justification::centredLeft);
}

//==============================================================================
// RoomView
//==============================================================================
RoomView::RoomView (FacetProcessor& p) : proc (p)
{
    setTitle ("Room");
    setDescription ("Seen from above, you in the middle, front at the top. Grains appear where they are heard: "
                    "filled above ear level, hollow below. With Place by Each head, drag the numbered handles.");
    setAccessible (true);
    startTimerHz (30);
}

juce::Rectangle<float> RoomView::circleArea() const
{
    auto r = getLocalBounds().toFloat().reduced (14.0f).withTrimmedTop (26.0f).withTrimmedBottom (18.0f);
    const float d = juce::jmin (r.getWidth() - 150.0f, r.getHeight());
    return juce::Rectangle<float> (d, d).withCentre ({ r.getX() + 150.0f + (r.getWidth() - 150.0f) * 0.5f, r.getCentreY() });
}

juce::Point<float> RoomView::pointFor (float az, float el) const
{
    const auto c = circleArea();
    const float rad = c.getWidth() * 0.5f * std::cos (juce::degreesToRadians (el));
    const float a = juce::degreesToRadians (az);
    return { c.getCentreX() + std::sin (a) * rad, c.getCentreY() - std::cos (a) * rad };
}

int RoomView::headAt (juce::Point<float> pt) const
{
    if ((int) proc.apvts.getRawParameterValue ("placeBy")->load() != facet::PlaceEachHead) return -1;
    for (int h = 0; h < facet::kNumHeads; ++h)
    {
        const juce::String id = "h" + juce::String (h + 1) + "_";
        if (proc.apvts.getRawParameterValue (id + "on")->load() < 0.5f) continue;
        const auto hp = pointFor (proc.apvts.getRawParameterValue (id + "az")->load(), 0.0f);
        if (hp.getDistanceFrom (pt) < 14.0f) return h;
    }
    return -1;
}

void RoomView::setHeadAz (int h, juce::Point<float> pt)
{
    const auto c = circleArea().getCentre();
    const float az = juce::radiansToDegrees (std::atan2 (pt.x - c.x, c.y - pt.y));
    if (auto* prm = proc.apvts.getParameter ("h" + juce::String (h + 1) + "_az"))
        prm->setValueNotifyingHost (prm->convertTo0to1 (az));
}

void RoomView::mouseDown (const juce::MouseEvent& e)
{
    dragHead = headAt (e.position);
    if (dragHead >= 0)
        if (auto* prm = proc.apvts.getParameter ("h" + juce::String (dragHead + 1) + "_az")) prm->beginChangeGesture();
}
void RoomView::mouseDrag (const juce::MouseEvent& e) { if (dragHead >= 0) setHeadAz (dragHead, e.position); }
void RoomView::mouseUp (const juce::MouseEvent&)
{
    if (dragHead >= 0)
        if (auto* prm = proc.apvts.getParameter ("h" + juce::String (dragHead + 1) + "_az")) prm->endChangeGesture();
    dragHead = -1;
}
void RoomView::mouseEnter (const juce::MouseEvent&)
{
    if (auto* c = findParentComponentOfClass<Content>()) c->showHint ("Room", getDescription());
}

void RoomView::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const auto path = chamfer (r, 18.0f, true, false, true, false);
    g.setColour (t.panel);
    g.fillPath (path);
    g.setColour (t.line);
    g.strokePath (path, juce::PathStrokeType (1.0f));

    const int choice = (int) proc.apvts.getRawParameterValue ("output")->load();
    const bool stems = choice >= facet::kStemsChoice;
    const int channels = facet::outputChannels (choice);

    // header + plain-language setup note
    g.setColour (t.text);
    g.setFont (displayFont (*this, 16.0f));
    g.drawText ("ROOM", r.reduced (14.0f, 6.0f).removeFromTop (20.0f), juce::Justification::centredLeft);
    auto info = r.reduced (14.0f, 0.0f).withTrimmedTop (34.0f).removeFromLeft (140.0f);
    g.setColour (t.value);
    g.setFont (displayFont (*this, 15.0f));
    g.drawFittedText (facet::outputNames()[choice].toUpperCase(), info.removeFromTop (36.0f).toNearestInt(), juce::Justification::topLeft, 2);
    g.setColour (t.text);
    g.setFont (bodyFont (*this, 13.0f));
    juce::String howTo = channels <= 2 ? juce::String ("Stereo track: nothing to set up.")
                                       : "Set the track to " + juce::String (channels) + " channels (Reaper: Route > Track channels).";
    if (choice >= 9 && choice < Spatial::firstSurroundChoice) howTo << " AmbiX: ACN / SN3D. Decode with your usual decoder.";
    if (stems) howTo << " Each head on its own pair: 1-2, 3-4, 5-6, 7-8.";
    g.drawFittedText (howTo, info.removeFromTop (110.0f).toNearestInt(), juce::Justification::topLeft, 7, 1.0f);

    const auto c = circleArea();
    const auto ctr = c.getCentre();

    if (stems)
    {
        g.setColour (t.dim);
        g.setFont (bodyFont (*this, 14.0f));
        g.drawFittedText ("Head stems: no placement in space.\nEach head plays on its own stereo pair (Stem in the HEADS tab).",
                          c.toNearestInt(), juce::Justification::centred, 4);
        return;
    }

    // rings: ear level (outer), 30 and 60 degrees up or down (inner)
    for (int k = 0; k < 3; ++k)
    {
        const float rr = c.getWidth() * 0.5f * std::cos (juce::degreesToRadians (30.0f * (float) k));
        g.setColour (k == 0 ? t.line.brighter (0.2f) : t.line);
        g.drawEllipse (juce::Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (ctr), k == 0 ? 1.5f : 0.8f);
    }
    g.setColour (t.dim);
    g.setFont (bodyFont (*this, 11.0f, true));
    g.drawText ("FRONT", juce::Rectangle<float> (60.0f, 14.0f).withCentre ({ ctr.x, c.getY() - 24.0f }), juce::Justification::centred);

    // listener
    juce::Path you;
    you.addTriangle (ctr.x, ctr.y - 7.0f, ctr.x + 6.0f, ctr.y + 5.0f, ctr.x - 6.0f, ctr.y + 5.0f);
    g.setColour (t.text);
    g.fillPath (you);

    // speakers
    const bool front = proc.apvts.getRawParameterValue ("frontCentre")->load() > 0.5f;
    const bool centre = proc.apvts.getRawParameterValue ("useCentre")->load() > 0.5f;
    const auto fmt = Spatial::formatFor (choice, front, centre, false);
    auto speaker = [&] (float az, float el, const juce::String& name)
    {
        const auto pt = pointFor (az, el);
        g.setColour (t.panel2);
        g.fillRect (juce::Rectangle<float> (16.0f, 12.0f).withCentre (pt));
        g.setColour (t.dim);
        g.drawRect (juce::Rectangle<float> (16.0f, 12.0f).withCentre (pt), 1.0f);
        g.setFont (bodyFont (*this, 9.5f, true));
        g.drawText (name, juce::Rectangle<float> (40.0f, 12.0f).withCentre (pt.translated (0.0f, el > 0.0f ? 12.0f : -13.0f)),
                    juce::Justification::centred);
    };
    if (fmt.kind == Spatial::Kind::stereo) { speaker (-30.0f, 0.0f, "L"); speaker (30.0f, 0.0f, "R"); }
    else if (fmt.kind == Spatial::Kind::ring)
        for (int k = 0; k < fmt.channels; ++k) speaker (fmt.speakerAngle (k), 0.0f, juce::String (k + 1));
    else if (fmt.kind == Spatial::Kind::surround && fmt.layout != nullptr)
    {
        for (auto& sp : fmt.layout->speakers)
            if (! sp.lfe) speaker (sp.az, sp.el, sp.name);
    }
    else
    {
        g.setColour (t.dim);
        g.setFont (bodyFont (*this, 11.0f));
        g.drawText ("whole sphere", juce::Rectangle<float> (100.0f, 14.0f).withCentre ({ ctr.x, c.getBottom() + 9.0f }), juce::Justification::centred);
    }

    // grains: filled above ear level, hollow below, size grows with height
    for (size_t gi = 0; gi < proc.engine.grainView.size(); ++gi)
    {
        if (proc.engine.grainView[gi].load() < 0.0f) continue;
        const float az = proc.engine.grainAz[gi].load(), el = proc.engine.grainEl[gi].load();
        const auto pt = pointFor (az, el);
        const float sz = 5.0f + 3.0f * std::abs (el) / 90.0f;
        g.setColour (t.value);
        if (el >= -1.0f) g.fillEllipse (juce::Rectangle<float> (sz, sz).withCentre (pt));
        else             g.drawEllipse (juce::Rectangle<float> (sz, sz).withCentre (pt), 1.5f);
    }

    // head handles (Each head)
    if ((int) proc.apvts.getRawParameterValue ("placeBy")->load() == facet::PlaceEachHead)
        for (int h = 0; h < facet::kNumHeads; ++h)
        {
            const juce::String id = "h" + juce::String (h + 1) + "_";
            if (proc.apvts.getRawParameterValue (id + "on")->load() < 0.5f) continue;
            const auto pt = pointFor (proc.apvts.getRawParameterValue (id + "az")->load(), 0.0f);
            juce::Path d;
            d.addQuadrilateral (pt.x, pt.y - 11.0f, pt.x + 11.0f, pt.y, pt.x, pt.y + 11.0f, pt.x - 11.0f, pt.y);
            g.setColour (t.panel2);
            g.fillPath (d);
            g.setColour (t.mod);
            g.strokePath (d, juce::PathStrokeType (2.0f));
            g.setColour (t.text);
            g.setFont (displayFont (*this, 13.0f));
            g.drawText (juce::String (h + 1), juce::Rectangle<float> (16.0f, 16.0f).withCentre (pt), juce::Justification::centred);
        }
}

//==============================================================================
// Panels
//==============================================================================
void Panel::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    g.fillAll (t.bg.withAlpha (0.86f));
    const auto c = card().toFloat();
    const auto path = chamfer (c, 22.0f);
    g.setColour (t.panel2);
    g.fillPath (path);
    g.setColour (t.value);
    g.strokePath (path, juce::PathStrokeType (1.5f));
    g.setColour (t.text);
    g.setFont (displayFont (*this, 28.0f));
    g.drawText (title, c.reduced (26.0f, 16.0f).removeFromTop (34.0f), juce::Justification::centredLeft);
    g.setColour (t.value);
    g.fillRect (c.getX() + 26.0f, c.getY() + 52.0f, 40.0f, 3.0f);
}

DisplayPanel::DisplayPanel (Settings& s) : Panel ("DISPLAY & ACCESSIBILITY"), settings (s)
{
    cardW = 600; cardH = 470;
    auto setup = [this] (juce::ComboBox& b, const juce::StringArray& items, const juce::String& name, std::function<void (int)> apply)
    {
        b.addItemList (items, 1);
        b.setTitle (name);
        b.setWantsKeyboardFocus (true);
        b.onChange = [this, &b, apply]
        {
            apply (b.getSelectedItemIndex());
            settings.changed();
        };
        addAndMakeVisible (b);
    };
    setup (themeBox,   themeNames(),   "Theme",        [this] (int i) { settings.theme = i; });
    setup (paletteBox, paletteNames(), "Colours",      [this] (int i) { settings.palette = i; });
    setup (fontBox,    fontNames(),    "Font",         [this] (int i) { settings.font = i; });
    setup (scaleBox,   scaleNames(),   "Size",         [this] (int i) { settings.scaleIndex = i; });

    auto setupToggle = [this] (juce::ToggleButton& b, const juce::String& desc, std::function<void (bool)> apply)
    {
        b.setClickingTogglesState (true);
        b.setWantsKeyboardFocus (true);
        b.setTitle (b.getButtonText());
        b.setDescription (desc);
        b.onClick = [this, &b, apply] { apply (b.getToggleState()); settings.changed(); };
        addAndMakeVisible (b);
    };
    setupToggle (styledBtn, "Use the condensed display font for headings. Turn off to use your reading font everywhere.", [this] (bool v) { settings.styled = v; });
    setupToggle (motionBtn, "Stops the moving trail on the grain pad.", [this] (bool v) { settings.reducedMotion = v; });
    setupToggle (hintsBtn,  "Shows what the control under the mouse or keyboard does, at the bottom of the window.", [this] (bool v) { settings.hints = v; });

    closeBtn.setTitle ("Done");
    closeBtn.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeBtn);

    setDescription ("Display settings apply to every Facet window and are remembered.");
    settings.addListener (this);
    sync();
}

void DisplayPanel::sync()
{
    themeBox.setSelectedItemIndex (settings.theme, juce::dontSendNotification);
    paletteBox.setSelectedItemIndex (settings.palette, juce::dontSendNotification);
    fontBox.setSelectedItemIndex (settings.font, juce::dontSendNotification);
    scaleBox.setSelectedItemIndex (settings.scaleIndex, juce::dontSendNotification);
    styledBtn.setToggleState (settings.styled, juce::dontSendNotification);
    motionBtn.setToggleState (settings.reducedMotion, juce::dontSendNotification);
    hintsBtn.setToggleState (settings.hints, juce::dontSendNotification);
}

void DisplayPanel::resized()
{
    auto c = card().reduced (26, 0);
    c.removeFromTop (72);
    for (auto* b : { &themeBox, &paletteBox, &fontBox, &scaleBox })
        b->setBounds (c.removeFromTop (46).withTrimmedLeft (170).withSizeKeepingCentre (c.getWidth() - 170, 34).withX (c.getX() + 170));
    c.removeFromTop (8);
    for (auto* b : { &styledBtn, &motionBtn, &hintsBtn })
        b->setBounds (c.removeFromTop (44).withTrimmedLeft (170).withSizeKeepingCentre (c.getWidth() - 170, 34).withX (c.getX() + 170));
    closeBtn.setBounds (card().getRight() - 26 - 130, card().getBottom() - 22 - 40, 130, 40);
}

void DisplayPanel::paint (juce::Graphics& g)
{
    Panel::paint (g);
    const auto& t = themeOf (*this);
    auto c = card().reduced (26, 0);
    c.removeFromTop (72);
    g.setFont (settings.body (15.0f, true));
    g.setColour (t.text);
    for (auto* label : { "Theme", "Colours", "Font", "Size" })
        g.drawText (label, c.removeFromTop (46).removeFromLeft (160), juce::Justification::centredLeft);
    c.removeFromTop (8);
    g.setColour (t.dim);
    for (auto* label : { "Headings", "Motion", "Hints" })
        g.drawText (label, c.removeFromTop (44).removeFromLeft (160), juce::Justification::centredLeft);
    g.setFont (settings.body (13.0f));
    g.drawFittedText ("Applies to every Facet window. Remembered between sessions.",
                      card().reduced (26, 22).removeFromBottom (40).withTrimmedRight (150), juce::Justification::centredLeft, 2);
}

//------------------------------------------------------------------------------
namespace
{
    const char* helpQuick[] =
    {
        "1   Feed it sound. Put Facet on a track and press play, or drop an audio file anywhere on the window.",
        "2   Play the grain pad. Left and right choose where grains read, up and down how long they last.",
        "3   Lock it to the song. Turn on Sync: every playback from the same spot sounds the same. Change Seed for a new variation that is just as repeatable."
    };
    const char* helpKeys[][2] =
    {
        { "Tab / Shift+Tab", "move between controls" },
        { "Arrows", "adjust (hold Shift for fine steps)" },
        { "Page Up / Down", "big steps" },
        { "Delete", "back to default" },
        { "M  or right-click", "MIDI learn" },
        { "F", "freeze / unfreeze the buffer" },
        { "H  /  D", "this help  /  display settings" },
        { "Esc", "close a panel" }
    };
}

HelpPanel::HelpPanel (Settings& s) : Panel ("QUICK START"), settings (s)
{
    cardW = 700; cardH = 560;
    juce::String all;
    for (auto* l : helpQuick) all << l << "\n";
    for (auto& k : helpKeys) all << k[0] << ": " << k[1] << "\n";
    setDescription (all);

    onStartBtn.setClickingTogglesState (true);
    onStartBtn.setToggleState (false, juce::dontSendNotification);
    onStartBtn.setTitle ("Show this on start");
    onStartBtn.onClick = [this] { settings.showHelpOnStart = onStartBtn.getToggleState(); settings.changed(); };
    addAndMakeVisible (onStartBtn);

    startBtn.setTitle ("Start");
    startBtn.onClick = [this]
    {
        settings.showHelpOnStart = onStartBtn.getToggleState();
        settings.changed();
        if (onClose) onClose();
    };
    addAndMakeVisible (startBtn);
}

void HelpPanel::resized()
{
    const auto c = card();
    startBtn.setBounds (c.getRight() - 26 - 140, c.getBottom() - 22 - 42, 140, 42);
    onStartBtn.setBounds (c.getX() + 26, c.getBottom() - 22 - 40, 260, 36);
}

void HelpPanel::paint (juce::Graphics& g)
{
    Panel::paint (g);
    const auto& t = themeOf (*this);
    auto c = card().reduced (26, 0);
    c.removeFromTop (70);
    g.setFont (settings.body (16.0f));
    for (auto* l : helpQuick)
    {
        g.setColour (t.text);
        g.drawFittedText (l, c.removeFromTop (54), juce::Justification::centredLeft, 3, 1.0f);
        c.removeFromTop (4);
    }
    c.removeFromTop (8);
    g.setColour (t.value);
    g.setFont (settings.display (18.0f));
    g.drawText ("KEYBOARD", c.removeFromTop (26), juce::Justification::centredLeft);
    for (auto& k : helpKeys)
    {
        auto row = c.removeFromTop (22);
        g.setColour (t.text);
        g.setFont (settings.body (14.0f, true));
        g.drawText (k[0], row.removeFromLeft (190), juce::Justification::centredLeft);
        g.setColour (t.dim);
        g.setFont (settings.body (14.0f));
        g.drawText (k[1], row, juce::Justification::centredLeft);
    }
}

//==============================================================================
// Content
//==============================================================================
Content::Content (FacetProcessor& p, Settings& s)
    : proc (p), settings (s), bufferView (p), pad (p), modeChoice (p, "mode", "Buffer"),
      playChoice (p, "playMode", "Mode", "Play mode"), room (p),
      displayPanel (s), helpPanel (s)
{
    setWantsKeyboardFocus (true);
    setFocusContainerType (FocusContainerType::keyboardFocusContainer);
    setTitle ("Facet granular buffer");

    addAndMakeVisible (main);
    main.setFocusContainerType (FocusContainerType::focusContainer);
    for (auto* c : std::initializer_list<juce::Component*> { &playChoice, &modeChoice, &freezeBtn, &loadBtn, &helpBtn, &displayBtn, &bufferView, &pad })
        main.addAndMakeVisible (c);

    loadBtn.setTitle ("Load audio");
    loadBtn.setDescription ("Choose an audio file to play from. You can also drop a file anywhere on the window.");
    freezeBtn.setTitle ("Freeze");
    freezeBtn.setDescription ("Stops recording so the grains keep playing what is in the buffer. Shortcut: F.");
    helpBtn.setTitle ("Help");
    helpBtn.setDescription ("Quick start and keyboard shortcuts. Shortcut: H.");
    displayBtn.setTitle ("Display settings");
    displayBtn.setDescription ("Theme, colours, font, size, motion. Shortcut: D.");

    loadBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load audio", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult().existsAsFile() && proc.loadAudioFile (fc.getResult()))
                                      announce ("Loaded " + fc.getResult().getFileName());
                              });
    };
    freezeBtn.onClick = [this]
    {
        auto* m = proc.apvts.getParameter ("mode");
        const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
        const int next = mode == facet::ModeFreeze ? facet::ModeCapture : facet::ModeFreeze;
        m->beginChangeGesture();
        m->setValueNotifyingHost (m->convertTo0to1 ((float) next));
        m->endChangeGesture();
    };
    helpBtn.onClick    = [this] { openHelp(); };
    displayBtn.onClick = [this] { openDisplay(); };
    for (auto* b : { &loadBtn, &freezeBtn, &helpBtn, &displayBtn })
        b->onStateChange = [this, b] { if (b->isMouseOver()) showHint (b->getTitle(), b->getDescription()); };

    // 01 GRAIN
    grain.add<Knob>   (p, "size", "Size");
    grain.add<Knob>   (p, "density", "Density");
    grain.add<Toggle> (p, "sync", "Sync");
    grain.add<Choice> (p, "division", "Division");
    grain.add<Knob>   (p, "spray", "Spray");
    grain.add<Knob>   (p, "pattern", "Pattern");
    grain.add<Knob>   (p, "seed", "Seed");
    grain.add<Knob>   (p, "pitch", "Pitch");
    grain.add<Knob>   (p, "fine", "Fine");
    grain.add<Choice> (p, "direction", "Direction");
    grain.add<Knob>   (p, "shape", "Shape");
    grain.add<Knob>   (p, "pan", "Pan spread");
    // 02 BUFFER / SCAN
    buffer.add<Knob>   (p, "position", "Position");
    buffer.add<Knob>   (p, "window", "Window");
    buffer.add<Knob>   (p, "feedback", "Feedback");
    buffer.add<Choice> (p, "scanMode", "Scan");
    buffer.add<Knob>   (p, "scanSpeed", "Scan speed");
    // 03 SHIFT / SLICE
    shiftS.add<Knob>   (p, "shiftStep", "Shift step");
    shiftS.add<Knob>   (p, "shiftSteps", "Shift steps");
    shiftS.add<Knob>   (p, "freqShift", "Freq shift");
    shiftS.add<Toggle> (p, "sliceOn", "Slice");
    shiftS.add<Choice> (p, "sliceDiv", "Slice rate");
    shiftS.add<Choice> (p, "slicePattern", "Pattern");
    shiftS.add<Knob>   (p, "sliceDuty", "Duty");
    shiftS.add<Toggle> (p, "sliceReset", "Reset");
    // 04 MODULATION
    for (int k = 1; k <= facet::kNumMods; ++k)
    {
        const juce::String m = "m" + juce::String (k) + "_";
        const juce::String n = "Mod " + juce::String (k) + " ";
        mods.add<ModMeter> (p, k - 1);
        mods.add<Choice>   (p, m + "shape", "Shape", n + "shape");
        mods.add<Choice>   (p, m + "rate", "Rate mode", n + "rate mode");
        mods.add<Knob>     (p, m + "hz", "Rate", n + "rate");
        mods.add<Choice>   (p, m + "div", "Division", n + "division");
        mods.add<Knob>     (p, m + "depth", "Depth", n + "depth");
        mods.add<Choice>   (p, m + "dest", "Target", n + "target");
    }
    // 05 DYNAMICS / OUT
    dyn.add<Knob> (p, "compThresh", "Threshold");
    dyn.add<Knob> (p, "compRatio", "Ratio");
    dyn.add<Knob> (p, "compAttack", "Attack");
    dyn.add<Knob> (p, "compRelease", "Release");
    dyn.add<Knob> (p, "compMakeup", "Makeup");
    dyn.add<Knob> (p, "mix", "Mix");
    dyn.add<Knob> (p, "out", "Output");
    // 06 INSTRUMENT
    inst.add<Knob> (p, "root", "Root", "Root note");
    inst.add<Knob> (p, "attack", "Attack", "Note attack");
    inst.add<Knob> (p, "release", "Release", "Note release");

    // 07 HEADS: one row per head
    for (int h = 0; h < facet::kNumHeads; ++h)
    {
        const juce::String id = "h" + juce::String (h + 1) + "_";
        const juce::String n  = "Head " + juce::String (h + 1) + " ";
        heads.add<Toggle> (p, id + "on", "HEAD " + juce::String (h + 1), n + "on");
        heads.add<Toggle> (p, id + "link", "Link", n + "link");
        heads.add<Knob>   (p, id + "offset", "Offset", n + "offset", true);
        heads.add<Knob>   (p, id + "pitch", "Pitch", n + "pitch", true);
        heads.add<Knob>   (p, id + "level", "Level", n + "level", true);
        heads.add<Knob>   (p, id + "pan", "Pan", n + "pan", true);
        heads.add<Choice> (p, id + "out", "Output", n + "output");
        auto& edit = heads.add<Action> (id + "edit", "Settings", "EDIT", "Edit head " + juce::String (h + 1) + " settings",
                                        describe (id + "edit"));
        edit.button.setClickingTogglesState (false);
        edit.button.onClick = [this, h] { selectHead (h); };
        editButtons[(size_t) h] = &edit;

        // 08 detail page for this head (own settings, used when Link is off)
        auto d = std::make_unique<Section> ("08", "HEAD " + juce::String (h + 1) + " SETTINGS", 4, 80, 70);
        d->add<Knob>   (p, id + "size", "Size", n + "size");
        d->add<Knob>   (p, id + "density", "Density", n + "density");
        d->add<Choice> (p, id + "division", "Division", n + "division");
        d->add<Knob>   (p, id + "spray", "Spray", n + "spray");
        d->add<Choice> (p, id + "dir", "Direction", n + "direction");
        d->add<Knob>   (p, id + "shape", "Shape", n + "shape");
        d->add<Choice> (p, id + "scan", "Scan", n + "scan");
        d->add<Knob>   (p, id + "scanSpeed", "Scan speed", n + "scan speed");
        d->add<Knob>   (p, id + "pattern", "Pattern", n + "pattern");
        d->add<Knob>   (p, id + "seed", "Seed", n + "seed");
        auto& copy = d->add<Action> (id + "copy", "Start from", "COPY MAIN", "Copy main settings to head " + juce::String (h + 1),
                                     describe (id + "copy"));
        copy.button.onClick = [this, h]
        {
            proc.copyMainToHead (h);
            announce ("Main settings copied to head " + juce::String (h + 1));
        };
        main.addChildComponent (*d);
        headDetail[(size_t) h] = std::move (d);
    }

    // 09 SPACE: output format and where grains are heard (same formats as Sediments)
    space.add<Choice> (p, "output", "Output");
    space.add<Choice> (p, "placeBy", "Place by");
    space.add<Choice> (p, "height", "Height");
    space.add<Knob>   (p, "width", "Width");
    space.add<Knob>   (p, "scatter", "Scatter");
    space.add<Knob>   (p, "rotate", "Rotation");
    space.add<Toggle> (p, "frontCentre", "Speaker 1 front");
    space.add<Toggle> (p, "useCentre", "Centre spk");
    for (int h = 1; h <= facet::kNumHeads; ++h)
        space.add<Knob> (p, "h" + juce::String (h) + "_az", "H" + juce::String (h) + " dir", "Head " + juce::String (h) + " direction");
    for (int h = 1; h <= facet::kNumHeads; ++h)
        space.add<Knob> (p, "h" + juce::String (h) + "_el", "H" + juce::String (h) + " height", "Head " + juce::String (h) + " height");
    space.add<Toggle> (p, "lfe", "Feed LFE");
    main.addChildComponent (space);
    main.addChildComponent (room);

    tabMods.setTitle ("Show modulation and dynamics");
    tabHeads.setTitle ("Show read heads");
    tabHeads.setDescription ("Up to 4 read heads, each reading its own spot, with its own settings and output.");
    tabSpace.setTitle ("Show space");
    tabSpace.setDescription ("Output format (stereo to 16 channels, surround, Ambisonics, head stems) and where the grains are heard.");
    for (auto* b : { &tabMods, &tabHeads, &tabSpace })
    {
        b->setRadioGroupId (4242);
        b->setClickingTogglesState (true);
        main.addAndMakeVisible (b);
    }
    tabMods.setToggleState (true, juce::dontSendNotification);
    tabMods.onClick  = [this] { showTab (0); };
    tabHeads.onClick = [this] { showTab (1); };
    tabSpace.onClick = [this] { showTab (2); };

    linkAllBtn.setTitle ("Link all heads");
    linkAllBtn.setDescription ("All heads follow the main Grain and Scan settings.");
    unlinkAllBtn.setTitle ("Unlink all heads");
    unlinkAllBtn.setDescription ("All heads use their own settings.");
    linkAllBtn.onClick   = [this] { proc.setAllLinks (true);  announce ("All heads linked"); };
    unlinkAllBtn.onClick = [this] { proc.setAllLinks (false); announce ("All heads unlinked"); };
    main.addChildComponent (linkAllBtn);
    main.addChildComponent (unlinkAllBtn);
    for (auto* b : { &tabMods, &tabHeads, &tabSpace, &linkAllBtn, &unlinkAllBtn })
        b->onStateChange = [this, b] { if (b->isMouseOver()) showHint (b->getTitle(), b->getDescription()); };

    for (auto* s2 : { &grain, &buffer, &shiftS, &mods, &dyn }) main.addAndMakeVisible (s2);
    main.addChildComponent (heads);
    main.addChildComponent (inst);
    selectHead (0);

    addChildComponent (displayPanel);
    addChildComponent (helpPanel);
    displayPanel.onClose = [this] { closePanels(); };
    helpPanel.onClose    = [this] { closePanels(); };

    settings.addListener (this);
    startTimerHz (10);

    if (settings.showHelpOnStart)
    {
        juce::Component::SafePointer<Content> safe (this);
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->openHelp(); });
    }
}

Content::~Content() { settings.removeListener (this); }

void Content::displaySettingsChanged() { repaint(); }

void Content::showHint (const juce::String& title, const juce::String& text)
{
    if (title == hintTitle && text == hintText) return;
    hintTitle = title;
    hintText = text;
    repaint (0, getHeight() - 52, getWidth(), 52);
}

void Content::announce (const juce::String& text)
{
    juce::AccessibilityHandler::postAnnouncement (text, juce::AccessibilityHandler::AnnouncementPriority::medium);
    showHint ("", text);
}

void Content::openDisplay()
{
    helpPanel.setVisible (false);
    main.setEnabled (false);
    displayPanel.setVisible (true);
    displayPanel.toFront (false);
    displayPanel.focusFirst();
}

void Content::openHelp()
{
    displayPanel.setVisible (false);
    main.setEnabled (false);
    helpPanel.setVisible (true);
    helpPanel.toFront (false);
    helpPanel.focusFirst();
}

void Content::closePanels()
{
    const bool wasOpen = displayPanel.isVisible() || helpPanel.isVisible();
    displayPanel.setVisible (false);
    helpPanel.setVisible (false);
    main.setEnabled (true);
    if (wasOpen) pad.grabKeyboardFocus();
}

void Content::showTab (int t)
{
    tab = t;
    headsView = t == 1;
    const bool notes = (int) proc.apvts.getRawParameterValue ("playMode")->load() != facet::PlayAudio;
    tabMods.setToggleState (t == 0, juce::dontSendNotification);
    tabHeads.setToggleState (t == 1, juce::dontSendNotification);
    tabSpace.setToggleState (t == 2, juce::dontSendNotification);
    mods.setVisible (t == 0);
    dyn.setVisible (t == 0);
    inst.setVisible (t == 0 && notes);
    heads.setVisible (t == 1);
    linkAllBtn.setVisible (t == 1);
    unlinkAllBtn.setVisible (t == 1);
    for (int h = 0; h < facet::kNumHeads; ++h)
        headDetail[(size_t) h]->setVisible (t == 1 && h == selectedHead);
    space.setVisible (t == 2);
    room.setVisible (t == 2);
    announce (t == 0 ? "Modulation and dynamics" : t == 1 ? "Heads" : "Space");
}

void Content::selectHead (int h)
{
    selectedHead = juce::jlimit (0, facet::kNumHeads - 1, h);
    for (int i = 0; i < facet::kNumHeads; ++i)
    {
        editButtons[(size_t) i]->button.setToggleState (i == selectedHead, juce::dontSendNotification);
        headDetail[(size_t) i]->setVisible (headsView && i == selectedHead);
    }
}

bool Content::keyPressed (const juce::KeyPress& k)
{
    const auto ch = juce::CharacterFunctions::toLowerCase (k.getTextCharacter());
    if (k == juce::KeyPress::escapeKey) { closePanels(); return true; }
    if (displayPanel.isVisible() || helpPanel.isVisible()) return false;
    if (ch == 'f') { freezeBtn.triggerClick(); return true; }
    if (ch == 'h' || k.getKeyCode() == juce::KeyPress::F1Key) { openHelp(); return true; }
    if (ch == 'd') { openDisplay(); return true; }
    return false;
}

juce::String Content::statusText() const
{
    auto& e = proc.engine;
    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    const bool sync = proc.apvts.getRawParameterValue ("sync")->load() > 0.5f;
    juce::String s;
    if (mode == facet::ModeFile) s << (e.hasFile() ? "PLAYING FILE" : "NO FILE LOADED");
    else if (mode == facet::ModeFreeze) s << "FROZEN";
    else s << "CAPTURING";
    const int pm = (int) proc.apvts.getRawParameterValue ("playMode")->load();
    s << (pm == facet::PlayMidi ? "  //  MIDI NOTES" : pm == facet::PlayBoth ? "  //  AUDIO + NOTES" : "");
    const int oc = (int) proc.apvts.getRawParameterValue ("output")->load();
    s << "\n" << facet::outputNames()[oc].toUpperCase() << "  //  " << (sync ? "SYNCED" : "FREE RATE");
    s << "\n" << (e.viewHostPlaying.load() ? "LOCKED TO SONG POSITION" : "TRANSPORT STOPPED");
    return s;
}

void Content::timerCallback()
{
    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    freezeBtn.setToggleState (mode == facet::ModeFreeze, juce::dontSendNotification);
    freezeBtn.setButtonText (mode == facet::ModeFreeze ? "UNFREEZE" : "FREEZE");
    freezeBtn.setEnabled (mode != facet::ModeFile);
    if (lastMode >= 0 && mode != lastMode)
        announce (mode == facet::ModeFreeze ? "Buffer frozen" : mode == facet::ModeCapture ? "Capturing input" : "Playing file");
    lastMode = mode;

    int onCount = 0;
    for (int h = 0; h < facet::kNumHeads; ++h)
    {
        const juce::String id = "h" + juce::String (h + 1) + "_";
        const bool on     = proc.apvts.getRawParameterValue (id + "on")->load() > 0.5f;
        const bool linked = proc.apvts.getRawParameterValue (id + "link")->load() > 0.5f;
        onCount += on ? 1 : 0;
        auto& d = *headDetail[(size_t) h];
        d.setNote (linked ? "LINKED" : (on ? "OWN SETTINGS" : "HEAD OFF"));
        for (auto* c : d.getChildren()) c->setEnabled (! linked || dynamic_cast<Action*> (c) != nullptr);
    }
    tabHeads.setButtonText ("HEADS  " + juce::String (onCount) + "/4 ON");
    const int playMode = (int) proc.apvts.getRawParameterValue ("playMode")->load();
    inst.setVisible (tab == 0 && playMode != facet::PlayAudio);
    if (lastPlayMode >= 0 && playMode != lastPlayMode)
        announce (playMode == facet::PlayAudio ? "Audio mode: grains play continuously"
                : playMode == facet::PlayMidi ? "MIDI mode: notes play the grains"
                                              : "Both: continuous grains plus notes");
    lastPlayMode = playMode;
    const int outCh = facet::outputChannels ((int) proc.apvts.getRawParameterValue ("output")->load());
    tabSpace.setButtonText ("SPACE  " + juce::String (outCh) + " CH");

    const auto st = statusText();
    if (st != lastStatus) { lastStatus = st; repaint (0, 0, getWidth(), 90); }
}

void Content::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    g.fillAll (t.bg);

    if (t.decor)   // faint diagonal hatching
    {
        g.setColour (t.line.withAlpha (0.25f));
        for (int x = -getHeight(); x < getWidth(); x += 22)
            g.drawLine ((float) x, (float) getHeight(), (float) (x + getHeight()), 0.0f, 0.5f);
    }

    // header band
    auto header = getLocalBounds().reduced (12).removeFromTop (66).toFloat();
    const auto band = chamfer (header, 20.0f, false, true, false, true);
    g.setColour (t.panel2);
    g.fillPath (band);
    g.setColour (t.line);
    g.strokePath (band, juce::PathStrokeType (1.0f));

    // angled signal stripes
    for (int i = 0; i < 3; ++i)
    {
        juce::Path s;
        const float x = header.getX() + 18.0f + (float) i * 9.0f;
        s.addQuadrilateral (x + 8.0f, header.getY() + 14.0f, x + 13.0f, header.getY() + 14.0f,
                            x + 5.0f, header.getBottom() - 14.0f, x, header.getBottom() - 14.0f);
        g.setColour (i == 2 ? t.mod : t.value);
        g.fillPath (s);
    }

    g.setColour (t.text);
    g.setFont (settings.styled ? settings.display (44.0f).withExtraKerningFactor (0.18f) : settings.body (34.0f, true));
    g.drawText ("FACET", juce::Rectangle<float> (header.getX() + 58.0f, header.getY() + 2.0f, 190.0f, 44.0f), juce::Justification::centredLeft);
    g.setColour (t.value);
    g.setFont (settings.display (13.0f));
    g.drawText ("LPF // GRANULAR BUFFER",
                juce::Rectangle<float> (header.getX() + 60.0f, header.getY() + 42.0f, 220.0f, 16.0f), juce::Justification::centredLeft);

    // plain-language status
    g.setColour (t.dim);
    g.setFont (settings.display (14.0f));
    g.setFont (settings.display (13.0f));
    g.drawFittedText (lastStatus, juce::Rectangle<int> ((int) header.getX() + 262, (int) header.getY() + 6, 205, 54),
                      juce::Justification::centredLeft, 3, 0.85f);

    // hint bar
    if (settings.hints)
    {
        auto hb = getLocalBounds().reduced (12).removeFromBottom (38).toFloat();
        const auto hp = chamfer (hb, 12.0f, true, false, true, false);
        g.setColour (t.panel2);
        g.fillPath (hp);
        g.setColour (t.line);
        g.strokePath (hp, juce::PathStrokeType (1.0f));
        auto inner = hb.reduced (16.0f, 0.0f);
        if (hintTitle.isEmpty() && hintText.isEmpty())
        {
            g.setColour (t.dim);
            g.setFont (settings.body (14.0f));
            g.drawText ("Point at or Tab to any control to read what it does.   H: help   D: display settings",
                        inner, juce::Justification::centredLeft);
        }
        else
        {
            if (hintTitle.isNotEmpty())
            {
                g.setColour (t.value);
                g.setFont (settings.display (16.0f));
                const auto title = hintTitle.toUpperCase();
                const float tw = juce::GlyphArrangement::getStringWidth (settings.display (16.0f), title) + 18.0f;
                g.drawText (title, inner.removeFromLeft (tw), juce::Justification::centredLeft);
            }
            g.setColour (t.text);
            g.setFont (settings.body (14.0f));
            g.drawFittedText (hintText, inner.toNearestInt(), juce::Justification::centredLeft, 2, 0.85f);
        }
    }
}

void Content::resized()
{
    main.setBounds (getLocalBounds());
    displayPanel.setBounds (getLocalBounds());
    helpPanel.setBounds (getLocalBounds());

    auto r = getLocalBounds().reduced (12);
    auto header = r.removeFromTop (66).reduced (14, 6);
    displayBtn.setBounds (header.removeFromRight (100).withSizeKeepingCentre (100, 38));
    header.removeFromRight (8);
    helpBtn.setBounds (header.removeFromRight (70).withSizeKeepingCentre (70, 38));
    header.removeFromRight (8);
    loadBtn.setBounds (header.removeFromRight (120).withSizeKeepingCentre (120, 38));
    header.removeFromRight (8);
    freezeBtn.setBounds (header.removeFromRight (104).withSizeKeepingCentre (104, 38));
    header.removeFromRight (8);
    modeChoice.setBounds (header.removeFromRight (140).withTrimmedTop (-6).withTrimmedBottom (-6));
    header.removeFromRight (8);
    playChoice.setBounds (header.removeFromRight (120).withTrimmedTop (-6).withTrimmedBottom (-6));

    r.removeFromTop (10);
    r.removeFromBottom (38 + 8);   // hint bar
    auto top = r.removeFromTop (186);
    pad.setBounds (top.removeFromRight (250));
    top.removeFromRight (10);
    bufferView.setBounds (top);

    r.removeFromTop (10);
    auto rowA = r.removeFromTop (grain.preferredHeight());
    grain.setBounds (rowA.removeFromLeft (grain.preferredWidth()));
    rowA.removeFromLeft (10);
    buffer.setBounds (rowA.removeFromLeft (buffer.preferredWidth()));
    rowA.removeFromLeft (10);
    shiftS.setBounds (rowA.removeFromLeft (shiftS.preferredWidth()));

    r.removeFromTop (8);
    auto tabs = r.removeFromTop (30);
    tabMods.setBounds (tabs.removeFromLeft (230));
    tabs.removeFromLeft (6);
    tabHeads.setBounds (tabs.removeFromLeft (170));
    tabs.removeFromLeft (6);
    tabSpace.setBounds (tabs.removeFromLeft (150));
    unlinkAllBtn.setBounds (tabs.removeFromRight (130));
    tabs.removeFromRight (6);
    linkAllBtn.setBounds (tabs.removeFromRight (110));
    r.removeFromTop (6);
    {
        auto rowH = r;
        heads.setBounds (rowH.removeFromLeft (heads.preferredWidth()).withHeight (heads.preferredHeight()));
        rowH.removeFromLeft (10);
        for (auto& d : headDetail)
            d->setBounds (rowH.withWidth (d->preferredWidth()).withHeight (d->preferredHeight()));
    }
    {
        auto rowS = r;
        space.setBounds (rowS.removeFromRight (space.preferredWidth()).withHeight (space.preferredHeight()));
        rowS.removeFromRight (10);
        room.setBounds (rowS.withHeight (space.preferredHeight()));
    }
    auto rowB = r;
    mods.setBounds (rowB.removeFromLeft (mods.preferredWidth()).withHeight (mods.preferredHeight()));
    rowB.removeFromLeft (10);
    dyn.setBounds (rowB.removeFromLeft (dyn.preferredWidth()).withHeight (dyn.preferredHeight()));
    rowB.removeFromLeft (10);
    inst.setBounds (rowB.removeFromLeft (inst.preferredWidth()).withHeight (inst.preferredHeight()));
}

//==============================================================================
// Editor: scales the whole UI with the Size setting
//==============================================================================
FacetEditor::FacetEditor (FacetProcessor& p)
    : AudioProcessorEditor (p), proc (p), lnf (*settings), content (p, *settings)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    sendLookAndFeelChange();   // children were built before the look-and-feel was set
    settings->addListener (this);
    applyScale();
}

FacetEditor::~FacetEditor()
{
    settings->removeListener (this);
    setLookAndFeel (nullptr);
}

void FacetEditor::applyScale()
{
    const float s = settings->scale();
    content.setBounds (0, 0, facetui::Content::baseW, facetui::Content::baseH);
    content.setTransform (juce::AffineTransform::scale (s));
    setSize ((int) std::ceil (facetui::Content::baseW * s), (int) std::ceil (facetui::Content::baseH * s));
}

void FacetEditor::resized() {}

void FacetEditor::displaySettingsChanged()
{
    lnf.refresh();
    sendLookAndFeelChange();
    applyScale();
    repaint();
}

void FacetEditor::paintOverChildren (juce::Graphics& g)
{
    if (! dragging) return;
    const auto& t = lnf.theme();
    g.setColour (t.bg.withAlpha (0.7f));
    g.fillAll();
    g.setColour (t.value);
    g.drawRect (getLocalBounds().reduced (6), 3);
    g.setFont (settings->display (40.0f * settings->scale()));
    g.drawText ("DROP TO LOAD", getLocalBounds(), juce::Justification::centred);
}

bool FacetEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        for (auto* ext : { ".wav", ".aif", ".aiff", ".flac", ".mp3", ".ogg" })
            if (f.endsWithIgnoreCase (ext)) return true;
    return false;
}

void FacetEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dragging = false;
    for (auto& f : files)
        if (proc.loadAudioFile (juce::File (f)))
        {
            content.announce ("Loaded " + juce::File (f).getFileName());
            break;
        }
    repaint();
}
