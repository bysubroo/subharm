#pragma once
#include "PluginProcessor.h"
#include "Theme.h"
#include "TimelineView.h"

#ifndef SUBHARM_VERSION
 #define SUBHARM_VERSION "dev"
#endif

namespace subharm
{

// Segmented control bound to a choice parameter.
class Segment : public juce::Component
{
public:
    Segment (juce::RangedAudioParameter& p, juce::StringArray labels);
    void paint (juce::Graphics&) override;
    void resized() override;
    void update();
private:
    juce::RangedAudioParameter& param;
    juce::OwnedArray<juce::TextButton> buttons;
};

// MIX / STEMS pad you drag straight onto the DAW timeline.
class DragTile : public juce::Component
{
public:
    DragTile (SubHarmProcessor& p, bool stems) : proc (p), isStems (stems) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseDown (const juce::MouseEvent&) override { dragging = false; }
    void mouseDrag (const juce::MouseEvent&) override;
private:
    SubHarmProcessor& proc;
    bool isStems, dragging = false;
};

class StacksList : public juce::Component
{
public:
    explicit StacksList (SubHarmProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    int idealHeight() const { return juce::jmax (60, (int) proc.getStacks().size() * kRow); }
    static constexpr int kRow = 48;
private:
    SubHarmProcessor& proc;
};

class SubHarmEditor : public juce::AudioProcessorEditor,
                      private juce::ChangeListener,
                      private juce::Timer
{
public:
    explicit SubHarmEditor (SubHarmProcessor&);
    ~SubHarmEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refresh();

    SubHarmProcessor& proc;
    theme::LookAndFeel lnf;

    // top bar
    juce::TextButton recordButton;
    juce::ComboBox keyBox;
    juce::ToggleButton leadThru { "Hear lead" };
    // song
    TimelineView timeline;
    juce::TextButton zoomOut { "-" }, zoomIn { "+" }, fitButton { "Fit" };
    // lines
    juce::OwnedArray<juce::TextButton> lineButtons, presetButtons;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> lineAtts;
    // performance
    Segment flavourSeg, humanizeSeg;
    juce::ComboBox crewBox, deliveryBox;
    // output
    juce::Slider sauceKnob, levelKnob;
    // stacks
    juce::TextButton stackButton;
    juce::Label stackInfo;
    StacksList stacksList;
    juce::Viewport stacksViewport;
    DragTile mixTile, stemsTile;
    // status bar
    juce::TextButton exportButton { "Export to folder" }, folderButton { "Session folder" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, crewAtt, deliveryAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sauceAtt, levelAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> leadAtt;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<int> topBar, statusBar, songPanel, linesPanel, perfPanel, outPanel, stacksPanel;
    juce::Rectangle<int> logoArea, bpmArea, takeArea, scArea, outMeterArea;
    float scLevel = 0.0f, outLevel = 0.0f;
    juce::String status;
    int slowTick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubHarmEditor)
};

} // namespace subharm
