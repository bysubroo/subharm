#pragma once
#include "PluginProcessor.h"
#include "Theme.h"
#include "TimelineView.h"

namespace subharm
{

// Row of radio buttons bound to a choice parameter.
class Segment : public juce::Component
{
public:
    Segment (juce::RangedAudioParameter& p, juce::StringArray labels);
    void resized() override;
    void update();
private:
    juce::RangedAudioParameter& param;
    juce::OwnedArray<juce::TextButton> buttons;
};

// MIX / STEMS tile you drag straight onto the DAW timeline.
class DragTile : public juce::Component
{
public:
    DragTile (SubHarmProcessor& p, bool stems) : proc (p), isStems (stems) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    void paint (juce::Graphics&) override;
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
    static constexpr int kRow = 44;
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
    void drawCard (juce::Graphics&, juce::Rectangle<int>, const juce::String& num, const juce::String& title);

    SubHarmProcessor& proc;
    theme::LookAndFeel lnf;

    juce::TextButton recordButton, zoomOut { "-" }, zoomIn { "+" }, fitButton { "FIT" };
    juce::ToggleButton leadThru { "Hear lead" };
    TimelineView timeline;

    juce::ComboBox keyBox, crewBox, deliveryBox;
    juce::OwnedArray<juce::TextButton> lineButtons, presetButtons;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> lineAtts;
    Segment flavourSeg, humanizeSeg;
    juce::TextButton stackButton;
    juce::Label stackInfo;

    StacksList stacksList;
    juce::Viewport stacksViewport;
    DragTile mixTile, stemsTile;
    juce::Slider sauceKnob, levelKnob;
    juce::TextButton exportButton { "Export..." };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, crewAtt, deliveryAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sauceAtt, levelAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> leadAtt;
    std::unique_ptr<juce::FileChooser> chooser;

    int slowTick = 0;
    juce::Rectangle<int> headerArea, partsCard, voicesCard, singCard, stackCard, stacksCard, ledArea, linesArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubHarmEditor)
};

} // namespace subharm
