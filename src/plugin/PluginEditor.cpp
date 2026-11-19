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
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    bool any = false;
    for (const auto& s : proc.getStacks()) any |= s->enabled && s->state.load() == StackEntry::Done;
    g.setColour (any ? panelHi : panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (any ? accent : line);
    g.drawRoundedRectangle (r, 8.0f, 1.5f);
    g.setColour (any ? text : faint);
    g.setFont (font (16.0f, true));
    g.drawText (isStems ? "STEMS" : "MIX", r.withTrimmedBottom (r.getHeight() * 0.45f), juce::Justification::centredBottom);
    g.setColour (dim);
    g.setFont (font (10.0f));
    g.drawText (isStems ? "a track per line" : "one track", r.withTrimmedTop (r.getHeight() * 0.6f).reduced (4, 0), juce::Justification::centredTop);
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
        g.setColour (panelHi);
        g.fillRoundedRectangle (row.toFloat(), 6.0f);

        int colour = 0;
        for (const auto& p : proc.getParts()) if (p.id == s->partId) colour = p.colourIndex;
        const bool done = s->state.load() == StackEntry::Done;

        // tick box
        auto box = juce::Rectangle<float> (10.0f, (float) y + (kRow - 4) / 2.0f - 8.0f, 16.0f, 16.0f);
        g.setColour (s->enabled && done ? accent : line);
        if (s->enabled && done) g.fillRoundedRectangle (box, 3.0f);
        else g.drawRoundedRectangle (box, 3.0f, 1.5f);
        if (s->enabled && done)
        {
            g.setColour (juce::Colours::white);
            juce::Path tick; tick.startNewSubPath (box.getX() + 3.5f, box.getCentreY()); tick.lineTo (box.getX() + 7.0f, box.getBottom() - 4.0f); tick.lineTo (box.getRight() - 3.0f, box.getY() + 4.0f);
            g.strokePath (tick, juce::PathStrokeType (2.0f));
        }

        // part badge
        auto badge = juce::Rectangle<float> (34.0f, (float) y + 8.0f, 24.0f, (float) kRow - 20.0f);
        g.setColour (partColour (colour));
        g.fillRoundedRectangle (badge, 4.0f);
        g.setColour (bg);
        g.setFont (font (12.0f, true));
        g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + (s->partId - 1) % 26)), badge, juce::Justification::centred);

        g.setColour (text);
        g.setFont (font (12.0f, true));
        g.drawText (juce::String (toString (s->settings.size)) + juce::String::fromUTF8 (" · ") + toString (s->settings.flavour),
                    66, y + 4, getWidth() - 110, 18, juce::Justification::centredLeft);
        g.setColour (dim);
        g.setFont (font (10.5f));
        g.drawText (juce::String (toString (s->settings.crew)) + juce::String::fromUTF8 (" · ") + toString (s->settings.delivery) + juce::String::fromUTF8 (" · ") + toString (s->settings.humanize),
                    66, y + 20, getWidth() - 110, 16, juce::Justification::centredLeft);

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
      sizeSeg (param (p, "size"), { "Short 3", "Tall 4", "Full 8" }),
      flavourSeg (param (p, "flavour"), { "R&B", "Pop", "Clean", "Gospel" }),
      humanizeSeg (param (p, "humanize"), { "Tight", "Natural", "Loose" }),
      stacksList (p), mixTile (p, false), stemsTile (p, true)
{
    setLookAndFeel (&lnf);

    recordButton.onClick = [this] { proc.toggleRecord(); refresh(); };
    addAndMakeVisible (recordButton);
    zoomOut.onClick = [this] { timeline.zoom (0.7); };
    zoomIn.onClick = [this] { timeline.zoom (1.4); };
    fitButton.onClick = [this] { timeline.fit(); };
    for (auto* b : { &zoomOut, &zoomIn, &fitButton }) addAndMakeVisible (b);
    addAndMakeVisible (leadThru);
    leadAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "leadThru", leadThru);
    addAndMakeVisible (timeline);

    crewBox.addItemList ({ "Your voice", "Wide crew", "Deeper crew", "Lighter crew" }, 1);
    deliveryBox.addItemList ({ "Natural", "Airy", "Gentle", "Full voice", "Energetic" }, 1);
    addAndMakeVisible (crewBox);
    addAndMakeVisible (deliveryBox);
    crewAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "crew", crewBox);
    deliveryAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "delivery", deliveryBox);

    for (auto* s : { &sizeSeg, &flavourSeg, &humanizeSeg }) addAndMakeVisible (s);

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

    proc.addChangeListener (this);
    setResizable (true, true);
    setResizeLimits (1000, 760, 2200, 1500);
    setSize (1120, 800);
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
    g.setColour (panel);
    g.fillRoundedRectangle (r.toFloat(), 10.0f);
    g.setColour (line);
    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 10.0f, 1.0f);
    auto badge = juce::Rectangle<float> ((float) r.getX() + 14, (float) r.getY() + 12, 20, 20);
    g.setColour (accent);
    g.fillEllipse (badge);
    g.setColour (juce::Colours::white);
    g.setFont (font (11.0f, true));
    g.drawText (num, badge, juce::Justification::centred);
    g.setColour (text);
    g.setFont (font (12.0f, true));
    g.drawText (title, r.getX() + 42, r.getY() + 12, r.getWidth() - 60, 20, juce::Justification::centredLeft);
}

void SubHarmEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    // header
    g.setColour (text);
    g.setFont (font (24.0f, true));
    g.drawText ("SUBHARM", headerArea.getX(), headerArea.getY(), 150, headerArea.getHeight(), juce::Justification::centredLeft);
    g.setColour (accent);
    g.fillRect (headerArea.getX() + 2, headerArea.getBottom() - 10, 26, 3);
    g.setColour (dim);
    g.setFont (font (11.0f));
    g.drawText (juce::String::fromUTF8 ("stacks that know the song  ·  by SubRoo"), headerArea.getX() + 140, headerArea.getY(), 260, headerArea.getHeight(), juce::Justification::centredLeft);

    const auto& take = proc.getTake();
    juce::String info;
    if (! take.isEmpty())
    {
        info = take.analysed ? juce::String (take.key.name()) : juce::String ("...");
        if (take.bpm > 0) info << juce::String::fromUTF8 ("  ·  ") << juce::String (take.bpm, 1) << " BPM";
        const int secs = (int) take.lengthSeconds();
        info << juce::String::fromUTF8 ("  ·  ") << secs / 60 << ":" << juce::String (secs % 60).paddedLeft ('0', 2);
    }
    g.setColour (text);
    g.setFont (font (13.0f, true));
    g.drawText (info, ledArea.getRight() + 22, headerArea.getY(), 260, headerArea.getHeight(), juce::Justification::centredLeft);

    // side chain LED
    const bool sig = proc.getSidechainLevel() > 0.003f;
    g.setColour (sig ? green : (proc.isSidechainConnected() ? faint : accentDim));
    g.fillEllipse (ledArea.toFloat());
    g.setColour (dim);
    g.setFont (font (10.0f));
    g.drawText ("SIDE CHAIN", ledArea.getX() - 68, ledArea.getY() - 3, 64, ledArea.getHeight() + 6, juce::Justification::centredRight);

    drawCard (g, partsCard, "1", "PICK YOUR PARTS");
    g.setColour (faint);
    g.setFont (font (10.5f));
    g.drawText (juce::String::fromUTF8 ("Drag on the vocal to add a part  ·  drag edges to resize (Alt = no snap)  ·  click a chord to fix it  ·  Ctrl+wheel zoom"),
                partsCard.getX() + 200, partsCard.getY() + 12, partsCard.getWidth() - 420, 20, juce::Justification::centredLeft);
    drawCard (g, voicesCard, "2", "CHOOSE YOUR VOICES");
    drawCard (g, singCard, "3", "HOW THEY SING");
    g.setColour (dim);
    g.setFont (font (10.0f, true));
    auto lbl = [&] (juce::Component& c, const char* t) { g.drawText (t, singCard.getX() + 16, c.getY(), 80, c.getHeight(), juce::Justification::centredLeft); };
    lbl (sizeSeg, "SIZE"); lbl (flavourSeg, "FLAVOUR"); lbl (humanizeSeg, "HUMANIZE");
    g.drawText ("VOICES", voicesCard.getX() + 16, crewBox.getY() - 16, 120, 14, juce::Justification::centredLeft);
    g.drawText ("DELIVERY", deliveryBox.getX(), deliveryBox.getY() - 16, 120, 14, juce::Justification::centredLeft);
    drawCard (g, stackCard, "4", "STACK HARMONIES");
    drawCard (g, stacksCard, "5", "YOUR STACKS");
    g.setColour (dim);
    g.setFont (font (10.0f, true));
    g.drawText ("SAUCE", sauceKnob.getX(), sauceKnob.getY() - 14, sauceKnob.getWidth(), 14, juce::Justification::centred);
    g.drawText ("LEVEL", levelKnob.getX(), levelKnob.getY() - 14, levelKnob.getWidth(), 14, juce::Justification::centred);
    g.setColour (faint);
    g.drawText ("DRAG TO YOUR DAW", mixTile.getX(), mixTile.getY() - 16, 160, 14, juce::Justification::centredLeft);
}

void SubHarmEditor::resized()
{
    auto r = getLocalBounds().reduced (14);
    headerArea = r.removeFromTop (44);
    {
        auto h = headerArea;
        h.removeFromLeft (420);
        recordButton.setBounds (h.removeFromLeft (190).reduced (0, 6));
        leadThru.setBounds (h.removeFromRight (110).reduced (0, 8));
        ledArea = { recordButton.getRight() + 82, recordButton.getBounds().getCentreY() - 5, 10, 10 };
    }
    r.removeFromTop (8);

    partsCard = r.removeFromTop ((int) (r.getHeight() * 0.42f));
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
        crewBox.setBounds (c.removeFromLeft (c.getWidth() / 2 - 6));
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
        const int rowH = juce::jlimit (24, 38, c.getHeight() / 3 - 6);
        for (auto* s : { &sizeSeg, &flavourSeg, &humanizeSeg })
        {
            auto row = c.removeFromTop (rowH);
            row.removeFromLeft (84);
            s->setBounds (row);
            c.removeFromTop (6);
        }
    }

    {
        auto c = stacksCard.reduced (14).withTrimmedTop (32);
        auto bottom = c.removeFromBottom (92);
        c.removeFromBottom (8);
        stacksViewport.setBounds (c);
        stacksList.setBounds (0, 0, c.getWidth() - 10, juce::jmax (c.getHeight(), stacksList.idealHeight()));

        bottom.removeFromTop (16);
        mixTile.setBounds (bottom.removeFromLeft (100));
        bottom.removeFromLeft (8);
        stemsTile.setBounds (bottom.removeFromLeft (100));
        bottom.removeFromLeft (8);
        exportButton.setBounds (bottom.removeFromRight (80).withSizeKeepingCentre (80, 28));
        sauceKnob.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2));
        levelKnob.setBounds (bottom);
    }
}

void SubHarmEditor::refresh()
{
    using RS = SubHarmProcessor::RecState;
    switch (proc.getRecState())
    {
        case RS::Armed:     recordButton.setButtonText ("ARMED - PRESS PLAY"); recordButton.setToggleState (true, juce::dontSendNotification); break;
        case RS::Recording: recordButton.setButtonText ("RECORDING " + juce::String (proc.getRecordedSeconds(), 0) + "s"); recordButton.setToggleState (true, juce::dontSendNotification); break;
        case RS::Idle:
        case RS::Finished:
        default:
            recordButton.setButtonText (proc.getTake().isEmpty() ? "RECORD THIS TRACK" : "RECORD AGAIN");
            recordButton.setToggleState (false, juce::dontSendNotification);
    }

    const auto& take = proc.getTake();
    const int pending = proc.numPendingParts();
    stackButton.setEnabled (! take.isEmpty() && take.analysed && (pending > 0 || proc.selectedPart > 0));
    if (take.isEmpty()) stackInfo.setText ("record a take first", juce::dontSendNotification);
    else if (! take.analysed) stackInfo.setText ("reading the song...", juce::dontSendNotification);
    else if (proc.getParts().empty()) stackInfo.setText ("draw a part on your vocal", juce::dontSendNotification);
    else if (pending > 0) stackInfo.setText (juce::String (pending) + (pending == 1 ? " new part to stack" : " new parts to stack"), juce::dontSendNotification);
    else stackInfo.setText (juce::String::fromUTF8 ("all parts stacked  ·  click to re-stack the selected part with these settings"), juce::dontSendNotification);

    stacksList.setBounds (0, 0, stacksViewport.getWidth() - 10, juce::jmax (stacksViewport.getHeight(), stacksList.idealHeight()));
    stacksList.repaint();
    mixTile.repaint();
    stemsTile.repaint();
    repaint (headerArea);
}

void SubHarmEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refresh();
    timeline.repaint();
    repaint();
}

void SubHarmEditor::timerCallback()
{
    sizeSeg.update(); flavourSeg.update(); humanizeSeg.update();
    if (proc.getRecState() != SubHarmProcessor::RecState::Idle) refresh();
    timeline.followPlayhead();
    repaint (ledArea.expanded (4));
}

} // namespace subharm
