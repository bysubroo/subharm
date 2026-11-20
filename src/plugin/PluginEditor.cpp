#include "PluginEditor.h"

namespace subharm
{
using namespace theme;

static juce::String dot() { return juce::String::fromUTF8 ("  \xc2\xb7  "); }

// =====================================================================================
Segment::Segment (juce::RangedAudioParameter& p, juce::StringArray labels) : param (p)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        b->getProperties().set ("kind", "seg");
        b->onClick = [this, i]
        {
            param.beginChangeGesture();
            param.setValueNotifyingHost (param.convertTo0to1 ((float) i));
            param.endChangeGesture();
            update();
        };
        addAndMakeVisible (b);
    }
    update();
}

void Segment::paint (juce::Graphics& g)
{
    g.setColour (bgDeep);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
    g.setColour (border);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.0f);
}

void Segment::resized()
{
    auto r = getLocalBounds().reduced (3);
    const int w = r.getWidth() / juce::jmax (1, buttons.size());
    for (auto* b : buttons) b->setBounds (r.removeFromLeft (w));
}

void Segment::update()
{
    const int idx = (int) std::lround (param.convertFrom0to1 (param.getValue()));
    for (int i = 0; i < buttons.size(); ++i)
        if (buttons[i]->getToggleState() != (i == idx))
            buttons[i]->setToggleState (i == idx, juce::dontSendNotification);
}

// =====================================================================================
void DragTile::paint (juce::Graphics& g)
{
    bool any = false;
    for (const auto& s : proc.getStacks()) any |= s->enabled && s->state.load() == StackEntry::Done;
    const bool over = isMouseOver();
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const auto c = isStems ? secondary : primary;

    if (any && over)
    {
        juce::Path p; p.addRoundedRectangle (r.reduced (2.0f), 8.0f);
        glowPath (g, p, c, 12, 0.45f);
    }
    g.setColour (any ? (over ? surfaceHi.brighter (0.05f) : surfaceHi) : surface);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (any ? c.withAlpha (over ? 1.0f : 0.6f) : border);
    juce::Path dash; dash.addRoundedRectangle (r, 8.0f);
    juce::Path dashed;
    const float pat[] = { 5.0f, 4.0f };
    juce::PathStrokeType (1.2f).createDashedStroke (dashed, dash, pat, 2);
    g.fillPath (dashed);

    // drag glyph: three stacked bars
    auto icon = juce::Rectangle<float> (r.getX() + 14.0f, r.getCentreY() - 9.0f, 18.0f, 18.0f);
    g.setColour (any ? c : faint);
    for (int i = 0; i < 3; ++i)
        g.fillRoundedRectangle (icon.getX() + (isStems ? 0.0f : i * 2.0f), icon.getY() + i * 6.5f + 1.0f,
                                isStems ? 18.0f - i * 4.0f : 14.0f, 3.5f, 1.5f);

    auto t = r.withTrimmedLeft (42.0f);
    g.setColour (any ? text : faint);
    g.setFont (ui (13.0f, Weight::ExtraBold));
    g.drawText (isStems ? "STEMS" : "MIX", t.withTrimmedBottom (t.getHeight() * 0.45f), juce::Justification::bottomLeft);
    g.setColour (any ? muted : faint);
    g.setFont (ui (10.5f, Weight::Medium));
    g.drawText (isStems ? "track per line, dry" : "ticked stacks, stereo", t.withTrimmedTop (t.getHeight() * 0.55f), juce::Justification::topLeft);
}

void DragTile::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6) return;
    dragging = true;
    auto files = proc.exportForDrag (isStems);
    if (! files.isEmpty())
        juce::DragAndDropContainer::performExternalDragDropOfFiles (files, false, this);
}

// =====================================================================================
void StacksList::paint (juce::Graphics& g)
{
    const auto& stacks = proc.getStacks();
    if (stacks.empty())
    {
        g.setColour (faint);
        g.setFont (ui (12.0f, Weight::Medium));
        g.drawFittedText ("Draw a part on your vocal, pick the lines\nand hit STACK HARMONIES.",
                          getLocalBounds().reduced (10), juce::Justification::centred, 3);
        return;
    }
    int y = 0;
    for (const auto& s : stacks)
    {
        auto row = juce::Rectangle<float> (0.0f, (float) y, (float) getWidth(), (float) kRow - 6.0f);
        const bool done = s->state.load() == StackEntry::Done;
        const bool on = s->enabled && done;
        const bool selected = s->partId == proc.selectedPart;

        g.setColour (selected ? surfaceHi.brighter (0.03f) : surfaceHi);
        g.fillRoundedRectangle (row, 8.0f);
        g.setColour (selected ? primary.withAlpha (0.45f) : border);
        g.drawRoundedRectangle (row.reduced (0.5f), 8.0f, 1.0f);

        // enable dot
        juce::Path d; d.addEllipse (12.0f, row.getCentreY() - 6.0f, 12.0f, 12.0f);
        if (on) glowPath (g, d, secondary, 8, 0.8f);
        g.setColour (on ? secondary : faint.withAlpha (0.6f));
        if (on) g.fillPath (d); else g.strokePath (d, juce::PathStrokeType (1.4f));

        int colour = 0;
        for (const auto& p : proc.getParts()) if (p.id == s->partId) colour = p.colourIndex;
        auto badge = juce::Rectangle<float> (34.0f, row.getCentreY() - 11.0f, 22.0f, 22.0f);
        g.setColour (partColour (colour).withAlpha (0.2f));
        g.fillRoundedRectangle (badge, 6.0f);
        g.setColour (partColour (colour));
        g.drawRoundedRectangle (badge, 6.0f, 1.2f);
        g.setFont (ui (12.0f, Weight::ExtraBold));
        g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + (s->partId - 1) % 26)), badge, juce::Justification::centred);

        g.setColour (text);
        g.setFont (ui (12.5f, Weight::Bold));
        g.drawText (juce::String (numLines (s->settings.lines) * 2) + " voices" + dot() + toString (s->settings.flavour)
                        + dot() + toString (s->settings.humanize),
                    66, (int) row.getY() + 6, getWidth() - 170, 16, juce::Justification::centredLeft);
        g.setColour (muted);
        g.setFont (ui (10.5f, Weight::Medium));
        g.drawText (juce::String (toString (s->settings.crew)) + dot() + toString (s->settings.delivery),
                    66, (int) row.getY() + 23, getWidth() - 170, 14, juce::Justification::centredLeft);

        // line colour dots (which lines are in this stack)
        float lx = (float) getWidth() - 104.0f;
        for (auto t : kLineDisplayOrder)
            if (hasLine (s->settings.lines, t))
            {
                g.setColour (lineColour (t));
                g.fillEllipse (lx, row.getCentreY() - 3.0f, 6.0f, 6.0f);
                lx += 8.0f;
            }

        const int st = s->state.load();
        if (st == StackEntry::Queued || st == StackEntry::Rendering)
        {
            auto bar = juce::Rectangle<float> (66.0f, row.getBottom() - 6.0f, (float) getWidth() - 110.0f, 2.5f);
            g.setColour (border);
            g.fillRoundedRectangle (bar, 1.2f);
            auto fill = bar.withWidth (bar.getWidth() * juce::jmax (0.02f, s->progress.load()));
            juce::Path pf; pf.addRoundedRectangle (fill, 1.2f);
            glowPath (g, pf, primary, 6, 0.9f);
            g.setColour (primaryHi);
            g.fillPath (pf);
        }
        else if (st == StackEntry::Failed)
        {
            g.setColour (error);
            g.setFont (ui (11.0f, Weight::Bold));
            g.drawText ("failed", getWidth() - 120, (int) row.getY(), 60, (int) row.getHeight(), juce::Justification::centredRight);
        }

        // remove
        const float cx = (float) getWidth() - 20.0f, cy = row.getCentreY();
        g.setColour (muted);
        g.drawLine (cx - 4, cy - 4, cx + 4, cy + 4, 1.5f);
        g.drawLine (cx - 4, cy + 4, cx + 4, cy - 4, 1.5f);
        y += kRow;
    }
}

void StacksList::mouseUp (const juce::MouseEvent& e)
{
    const int i = e.y / kRow;
    const auto& stacks = proc.getStacks();
    if (i < 0 || i >= (int) stacks.size()) return;
    auto s = stacks[(size_t) i];
    if (e.x > getWidth() - 36) proc.removeStack (s->id);
    else if (e.x < 30) proc.setStackEnabled (s->id, ! s->enabled);
    else { proc.selectedPart = s->partId; proc.sendChangeMessage(); }
}

// =====================================================================================
static juce::RangedAudioParameter& param (SubHarmProcessor& p, const char* id) { return *p.apvts.getParameter (id); }

SubHarmEditor::SubHarmEditor (SubHarmProcessor& p)
    : AudioProcessorEditor (p), proc (p), timeline (p),
      flavourSeg (param (p, "flavour"), { "R&B", "Pop", "Clean", "Gospel" }),
      humanizeSeg (param (p, "humanize"), { "Tight", "Natural", "Loose" }),
      stacksList (p), mixTile (p, false), stemsTile (p, true)
{
    setLookAndFeel (&lnf);

    // ---- top bar ----
    recordButton.getProperties().set ("kind", "rec");
    recordButton.onClick = [this] { proc.toggleRecord(); refresh(); };
    addAndMakeVisible (recordButton);

    keyBox.getProperties().set ("big", true);
    keyBox.addItemList (SubHarmProcessor::keyChoices(), 1);
    keyBox.setTooltip ("Key of the song. Auto detects it; a key you choose also locks the chord detection to it.");
    addAndMakeVisible (keyBox);
    keyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "key", keyBox);

    addAndMakeVisible (leadThru);
    leadAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "leadThru", leadThru);

    // ---- song ----
    addAndMakeVisible (timeline);
    zoomOut.onClick = [this] { timeline.zoom (0.7); };
    zoomIn.onClick = [this] { timeline.zoom (1.4); };
    fitButton.onClick = [this] { timeline.fit(); };
    for (auto* b : { &zoomOut, &zoomIn, &fitButton }) addAndMakeVisible (b);

    // ---- lines ----
    auto shortName = [] (LineType t) -> juce::String
    {
        switch (t)
        {
            case LineType::OctaveUp:   return "OCT UP";
            case LineType::OctaveDown: return "OCT DOWN";
            case LineType::ChordHigh:  return "CHORD HI";
            case LineType::ChordLow:   return "CHORD LO";
            default:                   return lineName (t);
        }
    };
    for (auto t : kLineDisplayOrder)
    {
        auto* b = lineButtons.add (new juce::TextButton (shortName (t)));
        b->setClickingTogglesState (true);
        b->getProperties().set ("kind", "token");
        b->getProperties().set ("tint", (juce::int64) lineColour (t).getARGB());
        juce::String tip = lineName (t);
        if (t == LineType::PedalRoot) tip << ": one constant note on the key's tonic";
        else if (t == LineType::PedalFifth) tip << ": one constant note on the key's 5th";
        else if (t == LineType::Double) tip << ": your melody doubled, Revoice-style";
        else if (t == LineType::ChordHigh || t == LineType::ChordLow) tip << ": follows the chords (flavour colours it)";
        b->setTooltip (tip + "  |  L + R take");
        addAndMakeVisible (b);
        lineAtts.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (proc.apvts, "line" + juce::String ((int) t), *b));
    }
    const char* presetNames[3] = { "Short", "SubRoo", "Full" };
    for (int i = 0; i < 3; ++i)
    {
        auto* b = presetButtons.add (new juce::TextButton (presetNames[i]));
        b->onClick = [this, i] { proc.applyLinePreset ((StackSize) i); };
        addAndMakeVisible (b);
    }

    // ---- performance ----
    for (auto* s : { &flavourSeg, &humanizeSeg }) addAndMakeVisible (s);
    crewBox.addItemList ({ "Your voice", "Wide crew", "Deeper crew", "Lighter crew" }, 1);
    deliveryBox.addItemList ({ "Natural", "Airy", "Gentle", "Full voice", "Energetic" }, 1);
    addAndMakeVisible (crewBox);
    addAndMakeVisible (deliveryBox);
    crewAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "crew", crewBox);
    deliveryAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "delivery", deliveryBox);

    // ---- output ----
    for (auto* k : { &sauceKnob, &levelKnob })
    {
        k->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k->setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        addAndMakeVisible (k);
    }
    sauceAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "sauce", sauceKnob);
    levelAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "stackLevel", levelKnob);
    sauceKnob.textFromValueFunction = [] (double v) { return juce::String ((int) std::lround (v)); };
    levelKnob.textFromValueFunction = [] (double v) { return juce::String (v, 1); };
    sauceKnob.setTooltip ("Sauce: stereo width, air and a touch of saturation on the stacks (STEMS stay dry)");
    levelKnob.setTooltip ("Level of the stacks in playback (dB)");

    // ---- stacks ----
    stackButton.setButtonText ("STACK HARMONIES");
    stackButton.getProperties().set ("kind", "primary");
    stackButton.onClick = [this] { proc.stackHarmonies(); };
    addAndMakeVisible (stackButton);
    stackInfo.setJustificationType (juce::Justification::centred);
    stackInfo.setFont (ui (11.0f, Weight::Medium));
    stackInfo.setColour (juce::Label::textColourId, muted);
    addAndMakeVisible (stackInfo);
    stacksViewport.setViewedComponent (&stacksList, false);
    stacksViewport.setScrollBarsShown (true, false);
    stacksViewport.setScrollBarThickness (8);
    addAndMakeVisible (stacksViewport);
    addAndMakeVisible (mixTile);
    addAndMakeVisible (stemsTile);

    // ---- status bar ----
    exportButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Export SubHarm stacks to folder",
                                                       juce::File::getSpecialLocation (juce::File::userDesktopDirectory));
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto dir = fc.getResult();
                                  if (dir == juce::File()) return;
                                  proc.exportToFolder (dir, false);
                                  proc.exportToFolder (dir, true);
                                  dir.revealToUser();
                              });
    };
    folderButton.onClick = [this]
    {
        auto f = proc.getSessionFolder();
        if (f.isDirectory()) f.revealToUser();
        else f.getParentDirectory().revealToUser();
    };
    addAndMakeVisible (exportButton);
    addAndMakeVisible (folderButton);

    proc.addChangeListener (this);
    setResizable (true, true);
    setResizeLimits (1100, 760, 2200, 1500);
    setSize (1240, 820);
    refresh();
    startTimerHz (30);
}

SubHarmEditor::~SubHarmEditor()
{
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

// =====================================================================================
void SubHarmEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    // ---------------- top bar ----------------
    g.setColour (bgDeep);
    g.fillRect (topBar);
    g.setColour (border);
    g.drawHorizontalLine (topBar.getBottom() - 1, 0.0f, (float) getWidth());

    {   // logo: stacked bars mark + wordmark
        auto l = logoArea.toFloat();
        auto mark = l.removeFromLeft (30.0f).withSizeKeepingCentre (26.0f, 24.0f);
        const juce::Colour mc[3] = { secondary, primary, warm };
        for (int i = 0; i < 3; ++i)
        {
            juce::Path bar;
            bar.addRoundedRectangle (mark.getX() + i * 3.0f, mark.getY() + i * 8.5f, mark.getWidth() - i * 6.0f, 5.5f, 2.5f);
            glowPath (g, bar, mc[i], 6, 0.6f);
            g.setColour (mc[i]);
            g.fillPath (bar);
        }
        l.removeFromLeft (10.0f);
        g.setColour (text);
        g.setFont (ui (21.0f, Weight::ExtraBold));
        g.drawText ("SubHarm", l.withTrimmedBottom (14.0f), juce::Justification::bottomLeft);
        g.setColour (muted);
        g.setFont (label (8.5f));
        g.drawText ("BY SUBROO", l.withTrimmedTop (l.getHeight() - 16.0f), juce::Justification::topLeft);
    }

    const auto& take = proc.getTake();
    {
        juce::String bpm = "--.-", len = "-:--";
        if (! take.isEmpty())
        {
            if (take.bpm > 0) bpm = juce::String (take.bpm, 1);
            const int secs = (int) take.lengthSeconds();
            len = juce::String (secs / 60) + ":" + juce::String (secs % 60).paddedLeft ('0', 2);
        }
        lcdReadout (g, bpmArea.toFloat(), "BPM", bpm, "888.8", primaryHi);
        lcdReadout (g, takeArea.toFloat(), "TAKE", len, "8:88", primaryHi);
        // key status above the key box
        g.setFont (label (8.5f));
        g.setColour (take.keyForced ? warm : secondary);
        juce::String ks = take.isEmpty() ? "KEY"
                        : ! take.analysed ? "KEY  |  READING THE SONG..."
                        : take.keyForced ? "KEY  |  SET BY YOU"
                        : "KEY  |  DETECTED " + juce::String (take.key.name()).toUpperCase();
        g.drawText (ks, keyBox.getBounds().translated (0, -16).withHeight (13), juce::Justification::centredLeft);
    }

    {   // side chain meter
        auto r = scArea.toFloat();
        g.setColour (faint);
        g.setFont (label (8.5f));
        const bool connected = proc.isSidechainConnected();
        g.drawText (connected ? "SIDE CHAIN" : "SIDE CHAIN  |  NOT CONNECTED", r.removeFromTop (14.0f), juce::Justification::centredLeft);
        ledMeter (g, r.withSizeKeepingCentre (r.getWidth(), 10.0f), scLevel, secondary);
    }

    // ---------------- panels ----------------
    panelBox (g, songPanel.toFloat(), "SONG");
    g.setColour (faint);
    g.setFont (ui (10.5f, Weight::Medium));
    g.drawText (juce::String::fromUTF8 ("drag on the vocal = new part  \xc2\xb7  drag edges = resize (Alt: free)  \xc2\xb7  click a chord to fix it"),
                juce::Rectangle<int> (songPanel.getX() + 80, songPanel.getY(), zoomOut.getX() - songPanel.getX() - 96, 34),
                juce::Justification::centredRight);
    panelBox (g, linesPanel.toFloat(), "LINES");
    panelBox (g, perfPanel.toFloat(), "PERFORMANCE");
    panelBox (g, outPanel.toFloat(), "OUTPUT");
    panelBox (g, stacksPanel.toFloat(), "STACKS");

    auto small = [&] (juce::Rectangle<int> r, const juce::String& t)
    {
        g.setColour (faint);
        g.setFont (label (8.5f));
        g.drawText (t, r, juce::Justification::centredLeft);
    };
    small (flavourSeg.getBounds().translated (0, -16).withHeight (14), "FLAVOUR  |  CHORD LINES");
    small (humanizeSeg.getBounds().translated (0, -16).withHeight (14), "HUMANIZE  |  TIMING + PITCH");
    small (crewBox.getBounds().translated (0, -16).withHeight (14), "VOICES");
    small (deliveryBox.getBounds().translated (0, -16).withHeight (14), "DELIVERY");
    if (! presetButtons.isEmpty())
        small (presetButtons[0]->getBounds().translated (0, -16).withHeight (14).withWidth (200), "PRESETS");

    auto knobLabel = [&] (juce::Slider& k, const char* t)
    {
        g.setColour (muted);
        g.setFont (label (9.5f));
        g.drawText (t, k.getBounds().withY (k.getBottom() - 6).withHeight (14), juce::Justification::centred);
    };
    knobLabel (sauceKnob, "SAUCE");
    knobLabel (levelKnob, "LEVEL dB");

    {   // stacks output meter
        auto r = outMeterArea.toFloat();
        g.setColour (faint);
        g.setFont (label (8.5f));
        g.drawText ("STACKS OUT", r.removeFromTop (14.0f), juce::Justification::centredLeft);
        ledMeter (g, r.withSizeKeepingCentre (r.getWidth(), 8.0f), outLevel, primary);
    }

    // ---------------- status bar ----------------
    g.setColour (bgDeep);
    g.fillRect (statusBar);
    g.setColour (border);
    g.drawHorizontalLine (statusBar.getY(), 0.0f, (float) getWidth());
    auto sb = statusBar.reduced (18, 0);
    const bool busy = status.startsWith ("stacking") || status.startsWith ("reading");
    juce::Path sd; sd.addEllipse ((float) sb.getX(), (float) sb.getCentreY() - 3.5f, 7.0f, 7.0f);
    const auto sc = busy ? warning : take.isEmpty() ? muted : success;
    if (! take.isEmpty()) glowPath (g, sd, sc, 6, 0.8f);
    g.setColour (sc);
    g.fillPath (sd);
    g.setColour (muted);
    g.setFont (ui (11.0f, Weight::Medium));
    g.drawText (status, sb.withTrimmedLeft (16).withTrimmedRight (330), juce::Justification::centredLeft);
    g.setColour (faint);
    g.drawText (juce::String ("v") + SUBHARM_VERSION, sb.withTrimmedRight (300), juce::Justification::centredRight);
}

void SubHarmEditor::resized()
{
    auto r = getLocalBounds();
    topBar = r.removeFromTop (72);
    statusBar = r.removeFromBottom (34);
    {
        auto sb = statusBar.reduced (18, 5);
        folderButton.setBounds (sb.removeFromRight (130));
        sb.removeFromRight (8);
        exportButton.setBounds (sb.removeFromRight (140));
    }
    {
        auto t = topBar.reduced (20, 0);
        logoArea = t.removeFromLeft (180).withSizeKeepingCentre (180, 40);
        t.removeFromLeft (24);
        keyBox.setBounds (t.removeFromLeft (230).withSizeKeepingCentre (230, 34).translated (0, 7));
        t.removeFromLeft (14);
        bpmArea = t.removeFromLeft (104).withSizeKeepingCentre (104, 46);
        t.removeFromLeft (8);
        takeArea = t.removeFromLeft (96).withSizeKeepingCentre (96, 46);
        leadThru.setBounds (t.removeFromRight (110).withSizeKeepingCentre (110, 30));
        t.removeFromRight (16);
        recordButton.setBounds (t.removeFromRight (190).withSizeKeepingCentre (190, 38));
        t.removeFromRight (20);
        const int scw = juce::jmin (180, juce::jmax (60, t.getWidth()));
        scArea = t.removeFromRight (scw).withSizeKeepingCentre (scw, 30);
    }

    auto body = r.reduced (16, 14);
    songPanel = body.removeFromTop ((int) (body.getHeight() * 0.50f));
    body.removeFromTop (12);
    {
        timeline.setBounds (songPanel.reduced (12).withTrimmedTop (24));
        auto zr = juce::Rectangle<int> (songPanel.getRight() - 12 - 132, songPanel.getY() + 6, 132, 24);
        fitButton.setBounds (zr.removeFromRight (52));
        zr.removeFromRight (6);
        zoomIn.setBounds (zr.removeFromRight (34));
        zr.removeFromRight (6);
        zoomOut.setBounds (zr.removeFromRight (34));
    }

    const int gap = 12;
    const int w = body.getWidth() - 3 * gap;
    linesPanel = body.removeFromLeft ((int) (w * 0.29f));
    body.removeFromLeft (gap);
    perfPanel = body.removeFromLeft ((int) (w * 0.25f));
    body.removeFromLeft (gap);
    outPanel = body.removeFromLeft ((int) (w * 0.17f));
    body.removeFromLeft (gap);
    stacksPanel = body;

    {   // lines: 2 columns of tokens + presets
        auto c = linesPanel.reduced (14).withTrimmedTop (24);
        auto presets = c.removeFromBottom (28);
        c.removeFromBottom (22);
        const int pw = (presets.getWidth() - 12) / 3;
        for (auto* b : presetButtons) { b->setBounds (presets.removeFromLeft (pw)); presets.removeFromLeft (6); }
        const int rows = 6;
        const int rowH = juce::jmin (32, (c.getHeight() - (rows - 1) * 6) / rows);
        const int colW = (c.getWidth() - 8) / 2;
        for (int i = 0; i < lineButtons.size(); ++i)
        {
            const int col = i % 2, row = i / 2;
            lineButtons[i]->setBounds (c.getX() + col * (colW + 8), c.getY() + row * (rowH + 6), colW, rowH);
        }
    }
    {   // performance
        auto c = perfPanel.reduced (14).withTrimmedTop (24);
        const int fieldH = 32;
        c.removeFromTop (16);
        flavourSeg.setBounds (c.removeFromTop (fieldH));
        c.removeFromTop (26);
        humanizeSeg.setBounds (c.removeFromTop (fieldH));
        c.removeFromTop (26);
        auto row = c.removeFromTop (fieldH);
        crewBox.setBounds (row.removeFromLeft (row.getWidth() / 2 - 4));
        row.removeFromLeft (8);
        deliveryBox.setBounds (row);
    }
    {   // output
        auto c = outPanel.reduced (12).withTrimmedTop (24);
        outMeterArea = c.removeFromBottom (30);
        c.removeFromBottom (6);
        const int big = juce::jmin (c.getWidth(), (int) (c.getHeight() * 0.56f));
        sauceKnob.setBounds (c.removeFromTop (big).withSizeKeepingCentre (big, big));
        c.removeFromTop (14);
        const int sm = juce::jmax (40, juce::jmin (c.getWidth(), c.getHeight() - 6));
        levelKnob.setBounds (c.removeFromTop (sm).withSizeKeepingCentre (sm, sm));
    }
    {   // stacks
        auto c = stacksPanel.reduced (14).withTrimmedTop (24);
        stackButton.setBounds (c.removeFromTop (44));
        c.removeFromTop (4);
        stackInfo.setBounds (c.removeFromTop (18));
        c.removeFromTop (6);
        auto tiles = c.removeFromBottom (52);
        mixTile.setBounds (tiles.removeFromLeft (tiles.getWidth() / 2 - 5));
        tiles.removeFromLeft (10);
        stemsTile.setBounds (tiles);
        c.removeFromBottom (10);
        stacksViewport.setBounds (c);
        stacksList.setBounds (0, 0, c.getWidth() - 10, juce::jmax (c.getHeight(), stacksList.idealHeight()));
    }
}

void SubHarmEditor::refresh()
{
    using RS = SubHarmProcessor::RecState;
    const bool standalone = proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
    switch (proc.getRecState())
    {
        case RS::Armed:
            recordButton.setButtonText (standalone ? "STARTING" : "ARMED  |  PRESS PLAY");
            recordButton.setToggleState (true, juce::dontSendNotification);
            break;
        case RS::Recording:
            recordButton.setButtonText ("REC " + juce::String ((int) proc.getRecordedSeconds()) + "s  |  STOP");
            recordButton.setToggleState (true, juce::dontSendNotification);
            break;
        case RS::Idle:
        case RS::Finished:
        default:
            recordButton.setButtonText (proc.getTake().isEmpty() ? "RECORD THIS TRACK" : "RECORD AGAIN");
            recordButton.setToggleState (false, juce::dontSendNotification);
    }

    const auto& take = proc.getTake();
    const int pending = proc.numPendingParts();
    const bool anyLine = proc.currentSettings().lines != 0;
    stackButton.setEnabled (! take.isEmpty() && take.analysed && anyLine && (pending > 0 || proc.selectedPart > 0));
    juce::String info;
    if (take.isEmpty()) info = "record a take first";
    else if (! take.analysed) info = "reading the song...";
    else if (! anyLine) info = "pick at least one line";
    else if (proc.getParts().empty()) info = "draw a part on your vocal";
    else if (pending > 0) info = juce::String (pending) + (pending == 1 ? " new part to stack" : " new parts to stack");
    else info = "re-stacks the selected part with these settings";
    stackInfo.setText (info, juce::dontSendNotification);

    {
        juce::String st;
        if (take.isEmpty()) st = standalone ? "Ready. Inputs 1/2 = vocal, 3/4 = side chain (beat). Hit RECORD."
                                            : "Ready. Set SubHarm's side chain to your beat, hit RECORD THIS TRACK and press play.";
        else if (! take.analysed) st = "reading chords, key and tempo...";
        else
        {
            int busy = 0;
            for (const auto& s : proc.getStacks()) busy += s->state.load() < StackEntry::Done ? 1 : 0;
            st = busy > 0 ? juce::String ("stacking ") + juce::String (busy) + (busy == 1 ? " part..." : " parts...")
                          : juce::String ((int) proc.getStacks().size()) + " stacks on " + juce::String ((int) proc.getParts().size()) + " parts"
                                + dot() + juce::String (take.key.name()) + (take.bpm > 0 ? dot() + juce::String (take.bpm, 1) + " BPM" : juce::String());
        }
        if (st != status) { status = st; repaint (statusBar); }
    }

    stacksList.setBounds (0, 0, stacksViewport.getWidth() - 10, juce::jmax (stacksViewport.getHeight(), stacksList.idealHeight()));
    stacksList.repaint();
    mixTile.repaint();
    stemsTile.repaint();
    repaint (topBar);
}

void SubHarmEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refresh();
    timeline.invalidate();
}

void SubHarmEditor::timerCallback()
{
    flavourSeg.update();
    humanizeSeg.update();
    if (proc.getRecState() != SubHarmProcessor::RecState::Idle || ++slowTick % 10 == 0) refresh();
    timeline.followPlayhead();

    const float scDb = juce::Decibels::gainToDecibels (proc.getSidechainLevel(), -60.0f);
    const float scT = juce::jlimit (0.0f, 1.0f, (scDb + 54.0f) / 54.0f);
    scLevel = scT > scLevel ? scT : scLevel * 0.88f + scT * 0.12f;
    repaint (scArea.expanded (2));

    const float outDb = juce::Decibels::gainToDecibels (proc.getStackOutLevel(), -60.0f);
    const float outT = juce::jlimit (0.0f, 1.0f, (outDb + 54.0f) / 54.0f);
    outLevel = outT > outLevel ? outT : outLevel * 0.88f + outT * 0.12f;
    repaint (outMeterArea.expanded (2));
}

} // namespace subharm
