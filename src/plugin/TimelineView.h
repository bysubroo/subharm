#pragma once
#include "PluginProcessor.h"

namespace subharm
{

// Song view: bar ruler, your vocal (draw parts here), beat, chords (click to edit).
class TimelineView : public juce::Component
{
public:
    explicit TimelineView (SubHarmProcessor& p) : proc (p) {}

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void zoom (double factor, double anchorSeconds = -1.0);
    void fit();
    void followPlayhead();

private:
    double xToTime (float x) const { return viewStart + (x - laneX()) * secPerPx; }
    float timeToX (double t) const { return laneX() + (float) ((t - viewStart) / secPerPx); }
    float laneX() const { return 70.0f; }
    juce::Rectangle<int> rulerArea() const  { return { (int) laneX(), 0, getWidth() - (int) laneX(), 18 }; }
    juce::Rectangle<int> vocalArea() const  { return { (int) laneX(), 20, getWidth() - (int) laneX(), getHeight() - 20 - 46 - 30 - 6 }; }
    juce::Rectangle<int> beatArea() const   { auto v = vocalArea(); return { v.getX(), v.getBottom() + 3, v.getWidth(), 46 }; }
    juce::Rectangle<int> chordArea() const  { auto b = beatArea(); return { b.getX(), b.getBottom() + 3, b.getWidth(), 30 }; }
    void clampView();
    double snap (double t) const;
    void drawWave (juce::Graphics&, juce::Rectangle<int>, const std::vector<float>& peaks, juce::Colour);
    void chordMenu (int segIndex, double atTime);

    enum class Drag { None, Create, ResizeStart, ResizeEnd, Pan };
    SubHarmProcessor& proc;
    double viewStart = 0.0, secPerPx = 0.02;
    bool fitted = false;
    Drag drag = Drag::None;
    int dragPart = -1;
    double dragAnchor = 0.0, dragNow = 0.0, panStartView = 0.0;
};

} // namespace subharm
