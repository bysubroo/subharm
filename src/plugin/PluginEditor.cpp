#include "PluginEditor.h"

namespace subharm
{
using namespace theme;

// =====================================================================================
Segment::Segment (juce::RangedAudioParameter& p, juce::StringArray labels) : param (p)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        b->setClickingTogglesState (false);
        b->getProperties().set ("lamp", true);
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

void Segment::resized()
{
    auto r = getLocalBounds();
    const int w = r.getWidth() / juce::jmax (1, buttons.size());
    for (auto* b : buttons) b->setBounds (r.removeFromLeft (w).reduced (2, 0));
}

void Segment::update()
{
    const int idx = (int) std::lround (param.convertFrom0to1 (param.getValue()));
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (i == idx, juce::dontSendNotification);
}

// =====================================================================================
void DragTile::paint (juce::Graphics& g)
{
    bool any = false;
    for (const auto& s : proc.getStacks()) any |= s->enabled && s->state.load() == StackEntry::Done;
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0, 2.0f), 6.0f);
    auto face = r.withTrimmedBottom (2.0f);
    const auto base = any ? panelHi : panelHi.darker (0.4f);
    g.setGradientFill (juce::ColourGradient (base.brighter (0.15f), 0, face.getY(), base.darker (0.3f), 0, face.getBottom(), false));
    g.fillRoundedRectangle (face, 6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (face, 6.0f, 1.0f);
    // grip lines
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    for (int i = 0; i < 3; ++i) g.drawHorizontalLine ((int) face.getBottom() - 10 - i * 4, face.getCentreX() - 14, face.getCentreX() + 14);
    lamp (g, { face.getX() + 12.0f, face.getY() + 12.0f }, 3.5f, accent, any);
    engrave (g, isStems ? "STEMS" : "MIX", face.withTrimmedBottom (face.getHeight() * 0.42f).toNearestInt(),
             juce::Justification::centredBottom, any ? text : faint, 16.0f);
    engrave (g, isStems ? "line by line" : "one track", face.withTrimmedTop (face.getHeight() * 0.58f).withTrimmedBottom (16).toNearestInt(),
             juce::Justification::centredTop, dim, 9.0f, false);
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
        g.setFont (font (12.0f));
        g.drawFittedText ("Your stacks show up here. Tick the ones you want to hear and drag them out.",
                          getLocalBounds().reduced (10), juce::Justification::centred, 3);
        return;
    }
    int y = 0;
    for (const auto& s : stacks)
    {
        auto row = juce::Rectangle<int> (0, y, getWidth(), kRow - 4);
        recess (g, row.toFloat().reduced (0.5f), 4.0f, bg);

        int colour = 0;
        for (const auto& p : proc.getParts()) if (p.id == s->partId) colour = p.colourIndex;
        const bool done = s->state.load() == StackEntry::Done;

        // tick box
        lamp (g, { 18.0f, (float) y + (kRow - 4) / 2.0f }, 5.0f, accent, s->enabled && done);

        // part badge
        auto badge = juce::Rectangle<float> (34.0f, (float) y + 8.0f, 24.0f, (float) kRow - 20.0f);
        g.setColour (partColour (colour));
        g.fillRoundedRectangle (badge, 4.0f);
        g.setColour (bg);
        g.setFont (font (12.0f, true));
        g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + (s->partId - 1) % 26)), badge, juce::Justification::centred);

        g.setColour (accent.brighter (0.4f));
        g.setFont (font (12.0f, true));
        g.drawText (juce::String (numLines (s->settings.lines) * 2) + " voices" + juce::String::fromUTF8 (" · ") + toString (s->settings.flavour)
                        + juce::String::fromUTF8 (" · ") + toString (s->settings.crew) + juce::String::fromUTF8 (" · ") + toString (s->settings.delivery)
                        + juce::String::fromUTF8 (" · ") + toString (s->settings.humanize),
                    66, y + 4, getWidth() - 190, 18, juce::Justification::centredLeft);
        g.setColour (dim);
        g.setFont (font (10.0f));
        g.drawText (s->label(), 66, y + 20, getWidth() - 190, 16, juce::Justification::centredLeft, true);

        const int st = s->state.load();
        if (st == StackEntry::Queued || st == StackEntry::Rendering)
        {
            auto bar = juce::Rectangle<float> ((float) getWidth() - 120.0f, (float) y + kRow / 2.0f - 4.0f, 80.0f, 4.0f);
            g.setColour (line); g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (accent); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * s->progress.load()), 2.0f);
            g.setColour (dim); g.setFont (font (10.0f));
            g.drawText (st == StackEntry::Queued ? "queued" : "stacking", (int) bar.getX(), y + 4, 80, 12, juce::Justification::centred);
        }
        else if (st == StackEntry::Failed)
        {
            g.setColour (accent); g.setFont (font (11.0f));
            g.drawText ("failed", getWidth() - 120, y, 80, kRow - 4, juce::Justification::centred);
        }

        g.setColour (dim);
        g.setFont (font (15.0f));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xc3\x97")), getWidth() - 34, y, 28, kRow - 4, juce::Justification::centred);
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

    recordButton.setComponentID ("rec");
    recordButton.onClick = [this] { proc.toggleRecord(); refresh(); };
    addAndMakeVisible (recordButton);
    zoomOut.onClick = [this] { timeline.zoom (0.7); };
    zoomIn.onClick = [this] { timeline.zoom (1.4); };
    fitButton.onClick = [this] { timeline.fit(); };
    for (auto* b : { &zoomOut, &zoomIn, &fitButton }) addAndMakeVisible (b);
    addAndMakeVisible (leadThru);
    leadAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "leadThru", leadThru);
    addAndMakeVisible (timeline);

    keyBox.addItemList (SubHarmProcessor::keyChoices(), 1);
    addAndMakeVisible (keyBox);
    keyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "key", keyBox);

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
        const int i = (int) t;
        auto* b = lineButtons.add (new juce::TextButton (shortName (t)));
        b->setClickingTogglesState (true);
        juce::String tip = lineName (t);
        if (t == LineType::PedalRoot) tip << ": one constant note on the key's tonic";
        else if (t == LineType::PedalFifth) tip << ": one constant note on the key's 5th";
        else if (t == LineType::Double) tip << ": your melody doubled, Revoice-style";
        b->setTooltip (tip + " (L + R take)");
        addAndMakeVisible (b);
        lineAtts.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (proc.apvts, "line" + juce::String (i), *b));
    }
    const char* presetNames[3] = { "Short", "SubRoo", "Full" };
    for (int i = 0; i < 3; ++i)
    {
        auto* b = presetButtons.add (new juce::TextButton (presetNames[i]));
        b->onClick = [this, i] { proc.applyLinePreset ((StackSize) i); };
        b->setColour (juce::TextButton::buttonColourId, panelHi.darker (0.15f));
        addAndMakeVisible (b);
    }

    crewBox.addItemList ({ "Your voice", "Wide crew", "Deeper crew", "Lighter crew" }, 1);
    deliveryBox.addItemList ({ "Natural", "Airy", "Gentle", "Full voice", "Energetic" }, 1);
    addAndMakeVisible (crewBox);
    addAndMakeVisible (deliveryBox);
    crewAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "crew", crewBox);
    deliveryAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "delivery", deliveryBox);

    for (auto* s : { &flavourSeg, &humanizeSeg }) addAndMakeVisible (s);

    stackButton.setButtonText ("STACK HARMONIES");
    stackButton.setColour (juce::TextButton::buttonColourId, accent);
    stackButton.onClick = [this] { proc.stackHarmonies(); };
    addAndMakeVisible (stackButton);
    stackInfo.setJustificationType (juce::Justification::centred);
    stackInfo.setFont (font (11.0f));
    stackInfo.setColour (juce::Label::textColourId, dim);
    addAndMakeVisible (stackInfo);

    stacksViewport.setViewedComponent (&stacksList, false);
    stacksViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (stacksViewport);
    addAndMakeVisible (mixTile);
    addAndMakeVisible (stemsTile);

    for (auto* k : { &sauceKnob, &levelKnob })
    {
        k->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        k->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (k);
    }
    sauceAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "sauce", sauceKnob);
    levelAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "stackLevel", levelKnob);
    sauceKnob.setTextValueSuffix (" %");
    levelKnob.setTextValueSuffix (" dB");

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
    addAndMakeVisible (exportButton);

    folderButton.setButtonText ("SESSION FOLDER");
    folderButton.onClick = [this]
    {
        auto f = proc.getSessionFolder();
        if (f.isDirectory()) f.revealToUser();
        else f.getParentDirectory().revealToUser();
    };
    addAndMakeVisible (folderButton);

    proc.addChangeListener (this);
    setResizable (true, true);
    setResizeLimits (1060, 840, 2200, 1500);
    setSize (1180, 880);
    refresh();
    startTimerHz (30);
}

SubHarmEditor::~SubHarmEditor()
{
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void SubHarmEditor::drawCard (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& num, const juce::String& title)
{
    // a recessed module on the faceplate, with its number stamped in a ring
    recess (g, r.toFloat(), 6.0f, juce::Colours::black.withAlpha (0.18f));
    auto ring = juce::Rectangle<float> ((float) r.getX() + 12, (float) r.getY() + 11, 20, 20);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (ring.translated (0, 1), 1.4f);
    g.setColour (dim);
    g.drawEllipse (ring, 1.4f);
    engrave (g, num, ring.toNearestInt(), juce::Justification::centred, text, 10.5f);
    engrave (g, title, { r.getX() + 40, r.getY() + 11, r.getWidth() - 60, 20 }, juce::Justification::centredLeft, text, 11.5f);
}

void SubHarmEditor::paint (juce::Graphics& g)
{
    if (faceplate.getWidth() != getWidth() || faceplate.getHeight() != getHeight() - 30)
        faceplate = makeFaceplate (getWidth(), getHeight() - 30, kEar);
    g.drawImageAt (faceplate, 0, 0);

    // ---- header: nameplate ----
    drawNameplate (g, nameplate.toFloat(), "SubHarm", "VOCAL HARMONY STACKER  SH-2");

    // ---- header: song display (key / tempo / length) ----
    {
        recess (g, songDisplay.toFloat(), 4.0f, bg);
        const auto& take = proc.getTake();
        auto d = songDisplay.reduced (10, 4);
        auto cell = [&] (const char* label, const juce::String& value, int w)
        {
            auto c = d.removeFromLeft (w);
            g.setColour (faint);
            g.setFont (font (8.5f, true).withExtraKerningFactor (0.1f));
            g.drawText (label, c.removeFromTop (12), juce::Justification::centredLeft);
            g.setColour (accent.brighter (0.4f));
            g.setFont (font (15.0f, true));
            g.drawText (value, c, juce::Justification::centredLeft);
        };
        juce::String key = "--", bpm = "--", len = "--:--";
        if (! take.isEmpty())
        {
            if (take.analysed) key = juce::String (take.key.name()) + (take.keyForced ? "" : " A");
            else key = "...";
            if (take.bpm > 0) bpm = juce::String (take.bpm, 1);
            const int secs = (int) take.lengthSeconds();
            len = juce::String (secs / 60) + ":" + juce::String (secs % 60).paddedLeft ('0', 2);
        }
        cell (take.keyForced ? "KEY (SET)" : "KEY (AUTO)", key, juce::jmax (90, d.getWidth() / 2));
        cell ("BPM", bpm, d.getWidth() / 2);
        cell ("TAKE", len, d.getWidth());
    }

    // ---- header: side chain VU ----
    drawVu (g, vuArea.toFloat(), vuNeedle, proc.isSidechainConnected());

    drawCard (g, partsCard, "1", "PICK YOUR PARTS");
    engrave (g, juce::String::fromUTF8 ("drag on the vocal = new part  ·  drag edges = resize (Alt: free)  ·  click a chord to fix it  ·  Ctrl+wheel = zoom"),
             { partsCard.getX() + 200, partsCard.getY() + 12, partsCard.getWidth() - 360, 20 }, juce::Justification::centredLeft, faint, 9.5f, false);
    drawCard (g, voicesCard, "2", "CHOOSE YOUR VOICES");
    drawCard (g, singCard, "3", "HOW THEY SING");
    auto lbl = [&] (juce::Rectangle<int> r, const char* t) { engrave (g, t, r, juce::Justification::centredLeft, dim, 9.5f); };
    lbl ({ singCard.getX() + 16, flavourSeg.getY(), 80, flavourSeg.getHeight() }, "FLAVOUR");
    lbl ({ singCard.getX() + 16, humanizeSeg.getY(), 80, humanizeSeg.getHeight() }, "HUMANIZE");
    if (! lineButtons.isEmpty())
    {
        lbl ({ singCard.getX() + 16, lineButtons[0]->getY(), 80, lineButtons[0]->getHeight() }, "LINES");
        lbl ({ singCard.getX() + 16, presetButtons[0]->getY(), 80, presetButtons[0]->getHeight() }, "PRESET");
        engrave (g, "each line = L + R take", { presetButtons[2]->getRight() + 10, presetButtons[0]->getY(),
                 singCard.getRight() - presetButtons[2]->getRight() - 24, presetButtons[0]->getHeight() },
                 juce::Justification::centredLeft, faint, 9.0f, false);
    }
    lbl ({ keyBox.getX(), keyBox.getY() - 16, 120, 14 }, "KEY");
    lbl ({ crewBox.getX(), crewBox.getY() - 16, 120, 14 }, "VOICES");
    lbl ({ deliveryBox.getX(), deliveryBox.getY() - 16, 120, 14 }, "DELIVERY");
    drawCard (g, stackCard, "4", "STACK HARMONIES");
    drawCard (g, stacksCard, "5", "YOUR STACKS");
    engrave (g, "SAUCE", { sauceKnob.getX(), sauceKnob.getY() - 14, sauceKnob.getWidth(), 14 }, juce::Justification::centred, dim, 9.5f);
    engrave (g, "LEVEL", { levelKnob.getX(), levelKnob.getY() - 14, levelKnob.getWidth(), 14 }, juce::Justification::centred, dim, 9.5f);
    engrave (g, "DRAG TO YOUR DAW", { mixTile.getX(), mixTile.getY() - 16, 200, 14 }, juce::Justification::centredLeft, faint, 9.0f);
    drawLedLadder (g, outMeter.toFloat(), outLevel, "OUT");

    // ---- bottom toolbar ----
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c1b19), 0, (float) toolbar.getY(), juce::Colour (0xff121110), 0, (float) toolbar.getBottom(), false));
    g.fillRect (toolbar);
    g.setColour (juce::Colours::black);
    g.drawHorizontalLine (toolbar.getY(), (float) toolbar.getX(), (float) toolbar.getRight());
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine (toolbar.getY() + 1, (float) toolbar.getX(), (float) toolbar.getRight());
    auto tb = toolbar.reduced (kEar + 12, 0);
    g.setFont (font (11.0f, true).withExtraKerningFactor (0.06f));
    g.setColour (text);
    g.drawText ("SubHarm", tb.removeFromLeft (80), juce::Justification::centredLeft);
    g.setColour (faint);
    g.setFont (font (10.5f));
    g.drawText (juce::String ("v") + SUBHARM_VERSION + juce::String::fromUTF8 ("  ·  SubRoo  ·  ") + status,
                tb.withTrimmedRight (170), juce::Justification::centredLeft);
}

void SubHarmEditor::resized()
{
    auto full = getLocalBounds();
    toolbar = full.removeFromBottom (30);
    {
        auto tb = toolbar.reduced (kEar + 12, 4);
        folderButton.setBounds (tb.removeFromRight (160));
    }
    auto r = full.reduced (kEar + 12, 12);
    headerArea = r.removeFromTop (54);
    {
        auto h = headerArea;
        nameplate = h.removeFromLeft (270).withSizeKeepingCentre (262, 50);
        h.removeFromLeft (20);
        recordButton.setBounds (h.removeFromLeft (200).withSizeKeepingCentre (200, 40));
        h.removeFromLeft (14);
        leadThru.setBounds (h.removeFromRight (110).withSizeKeepingCentre (110, 30));
        h.removeFromRight (12);
        vuArea = h.removeFromRight (120);
        h.removeFromRight (14);
        songDisplay = h.withSizeKeepingCentre (h.getWidth(), 44);
    }
    r.removeFromTop (10);

    partsCard = r.removeFromTop ((int) (r.getHeight() * 0.40f));
    timeline.setBounds (partsCard.reduced (12).withTrimmedTop (30));
    {
        auto zr = juce::Rectangle<int> (partsCard.getRight() - 140, partsCard.getY() + 9, 126, 26);
        fitButton.setBounds (zr.removeFromRight (50));
        zr.removeFromRight (4);
        zoomIn.setBounds (zr.removeFromRight (34));
        zr.removeFromRight (4);
        zoomOut.setBounds (zr.removeFromRight (34));
    }
    r.removeFromTop (10);

    auto left = r.removeFromLeft ((int) (r.getWidth() * 0.5f));
    r.removeFromLeft (10);
    stacksCard = r;

    voicesCard = left.removeFromTop (92);
    {
        auto c = voicesCard.reduced (16).withTrimmedTop (40);
        const int w = (c.getWidth() - 24) / 3;
        keyBox.setBounds (c.removeFromLeft (w));
        c.removeFromLeft (12);
        crewBox.setBounds (c.removeFromLeft (w));
        c.removeFromLeft (12);
        deliveryBox.setBounds (c);
    }
    left.removeFromTop (10);
    stackCard = left.removeFromBottom (124);
    {
        auto c = stackCard.reduced (16).withTrimmedTop (28);
        stackInfo.setBounds (c.removeFromBottom (18));
        c.removeFromBottom (4);
        stackButton.setBounds (c);
    }
    left.removeFromBottom (10);
    singCard = left;
    {
        auto c = singCard.reduced (16).withTrimmedTop (32);
        const int rowH = juce::jlimit (22, 34, c.getHeight() / 5 - 5);
        // lines: two rows (6 + 5)
        for (int rowIdx = 0; rowIdx < 2; ++rowIdx)
        {
            auto row = c.removeFromTop (rowH);
            row.removeFromLeft (74);
            const int first = rowIdx == 0 ? 0 : 6, count = rowIdx == 0 ? 6 : 5;
            const int bw = row.getWidth() / 6;
            for (int i = 0; i < count; ++i) lineButtons[first + i]->setBounds (row.removeFromLeft (bw).reduced (2, 0));
            c.removeFromTop (4);
        }
        {
            auto row = c.removeFromTop (rowH);
            row.removeFromLeft (84);
            const int bw = row.getWidth() / 5;
            for (auto* b : presetButtons) b->setBounds (row.removeFromLeft (bw).reduced (2, 2));
            c.removeFromTop (8);
        }
        for (auto* s : { &flavourSeg, &humanizeSeg })
        {
            auto row = c.removeFromTop (rowH);
            row.removeFromLeft (84);
            s->setBounds (row);
            c.removeFromTop (4);
        }
    }

    {
        auto c = stacksCard.reduced (14).withTrimmedTop (32);
        auto bottom = c.removeFromBottom (112);
        c.removeFromBottom (8);
        stacksViewport.setBounds (c);
        stacksList.setBounds (0, 0, c.getWidth() - 10, juce::jmax (c.getHeight(), stacksList.idealHeight()));

        bottom.removeFromTop (16);
        bottom = bottom.withTrimmedBottom (4);
        mixTile.setBounds (bottom.removeFromLeft (100));
        bottom.removeFromLeft (8);
        stemsTile.setBounds (bottom.removeFromLeft (100));
        bottom.removeFromLeft (8);
        exportButton.setBounds (bottom.removeFromRight (80).withSizeKeepingCentre (80, 28));
        bottom.removeFromRight (8);
        outMeter = bottom.removeFromRight (26);
        sauceKnob.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2));
        levelKnob.setBounds (bottom);
    }
}

void SubHarmEditor::refresh()
{
    {
        juce::String st;
        const auto& tk = proc.getTake();
        if (tk.isEmpty()) st = "ready: set the side chain, then record";
        else if (! tk.analysed) st = "reading chords, key and tempo";
        else
        {
            int busy = 0;
            for (const auto& s : proc.getStacks()) busy += s->state.load() < StackEntry::Done ? 1 : 0;
            st = busy > 0 ? juce::String ("stacking ") + juce::String (busy) + (busy == 1 ? " part" : " parts")
                          : juce::String (proc.getStacks().size()) + " stacks on " + juce::String (proc.getParts().size()) + " parts";
        }
        if (st != status) { status = st; repaint (toolbar); }
    }
    using RS = SubHarmProcessor::RecState;
    switch (proc.getRecState())
    {
        case RS::Armed:     recordButton.setButtonText (proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone ? "STARTING..." : "ARMED - PRESS PLAY"); recordButton.setToggleState (true, juce::dontSendNotification); break;
        case RS::Recording: recordButton.setButtonText ("RECORDING " + juce::String (proc.getRecordedSeconds(), 0) + "s - STOP"); recordButton.setToggleState (true, juce::dontSendNotification); break;
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
    if (take.isEmpty()) stackInfo.setText ("record a take first", juce::dontSendNotification);
    else if (! take.analysed) stackInfo.setText ("reading the song...", juce::dontSendNotification);
    else if (! anyLine) stackInfo.setText ("pick at least one line", juce::dontSendNotification);
    else if (proc.getParts().empty()) stackInfo.setText ("draw a part on your vocal", juce::dontSendNotification);
    else if (pending > 0) stackInfo.setText (juce::String (pending) + (pending == 1 ? " new part to stack" : " new parts to stack"), juce::dontSendNotification);
    else stackInfo.setText (juce::String::fromUTF8 ("all parts stacked  ·  click to re-stack the selected part with these settings"), juce::dontSendNotification);

    stacksList.setBounds (0, 0, stacksViewport.getWidth() - 10, juce::jmax (stacksViewport.getHeight(), stacksList.idealHeight()));
    stacksList.repaint();
    mixTile.repaint();
    stemsTile.repaint();
    repaint (songDisplay);
}

void SubHarmEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refresh();
    timeline.repaint();
    repaint();
}

void SubHarmEditor::timerCallback()
{
    flavourSeg.update(); humanizeSeg.update();
    if (proc.getRecState() != SubHarmProcessor::RecState::Idle || ++slowTick % 10 == 0) refresh();
    timeline.followPlayhead();
    // VU ballistics: ~300 ms rise/fall like a real meter; 0 VU ~ -18 dBFS RMS
    const float db = juce::Decibels::gainToDecibels (proc.getSidechainLevel(), -60.0f);
    const float target = juce::jlimit (0.0f, 1.05f, (db + 38.0f) / 23.0f);
    vuNeedle += (target - vuNeedle) * 0.25f;
    repaint (vuArea.expanded (2));
    const float outDb = juce::Decibels::gainToDecibels (proc.getStackOutLevel(), -60.0f);
    const float outTarget = juce::jlimit (0.0f, 1.0f, (outDb + 48.0f) / 48.0f);
    outLevel = outTarget > outLevel ? outTarget : outLevel * 0.9f + outTarget * 0.1f;
    repaint (outMeter.expanded (4));
}

} // namespace subharm
