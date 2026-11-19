#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace subharm::theme
{
    inline const juce::Colour bg        { 0xff0c0c0e };
    inline const juce::Colour panel     { 0xff151519 };
    inline const juce::Colour panelHi   { 0xff1d1d23 };
    inline const juce::Colour line      { 0xff2a2a32 };
    inline const juce::Colour text      { 0xffecebef };
    inline const juce::Colour dim       { 0xff8b8a95 };
    inline const juce::Colour faint     { 0xff55545e };
    inline const juce::Colour accent    { 0xffff3d6e };
    inline const juce::Colour accentDim { 0xff7a1f38 };
    inline const juce::Colour green     { 0xff3ddc84 };
    inline const juce::Colour wave      { 0xff6d6c78 };

    inline juce::Colour partColour (int i)
    {
        static const juce::uint32 c[8] = { 0xffff3d6e, 0xff4cc9f0, 0xffffb703, 0xff9b5de5,
                                            0xff3ddc84, 0xfff15bb5, 0xff00bbf9, 0xfffee440 };
        return juce::Colour (c[((i % 8) + 8) % 8]);
    }

    inline juce::Font font (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel()
        {
            setColour (juce::ResizableWindow::backgroundColourId, bg);
            setColour (juce::TextButton::buttonColourId, panelHi);
            setColour (juce::TextButton::buttonOnColourId, accent);
            setColour (juce::TextButton::textColourOffId, text);
            setColour (juce::TextButton::textColourOnId, juce::Colours::white);
            setColour (juce::ComboBox::backgroundColourId, panelHi);
            setColour (juce::ComboBox::outlineColourId, line);
            setColour (juce::ComboBox::textColourId, text);
            setColour (juce::ComboBox::arrowColourId, dim);
            setColour (juce::PopupMenu::backgroundColourId, panel);
            setColour (juce::PopupMenu::textColourId, text);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, accentDim);
            setColour (juce::Label::textColourId, text);
            setColour (juce::Slider::rotarySliderFillColourId, accent);
            setColour (juce::Slider::rotarySliderOutlineColourId, line);
            setColour (juce::Slider::thumbColourId, text);
            setColour (juce::Slider::textBoxTextColourId, dim);
            setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
            setColour (juce::ToggleButton::textColourId, text);
            setColour (juce::ToggleButton::tickColourId, accent);
            setColour (juce::ToggleButton::tickDisabledColourId, faint);
            setColour (juce::ScrollBar::thumbColourId, line);
        }

        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (0.5f);
            auto on = b.getToggleState();
            auto c = on ? accent : b.findColour (juce::TextButton::buttonColourId);
            if (over && ! on) c = c.brighter (0.08f);
            if (down) c = c.darker (0.15f);
            if (! b.isEnabled()) c = c.withAlpha (0.4f);
            g.setColour (c);
            g.fillRoundedRectangle (r, 6.0f);
            if (! on && b.findColour (juce::TextButton::buttonColourId) != accent)
            {
                g.setColour (line);
                g.drawRoundedRectangle (r, 6.0f, 1.0f);
            }
        }

        juce::Font getTextButtonFont (juce::TextButton&, int h) override { return font (juce::jmin (14.0f, h * 0.45f), true); }
        juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.0f); }
        juce::Font getPopupMenuFont() override { return font (13.0f); }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override
        {
            auto r = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
            auto radius = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
            auto c = r.getCentre();
            juce::Path track, val;
            track.addCentredArc (c.x, c.y, radius, radius, 0, a0, a1, true);
            val.addCentredArc (c.x, c.y, radius, radius, 0, a0, a0 + pos * (a1 - a0), true);
            g.setColour (line);
            g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (accent);
            g.strokePath (val, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            auto ang = a0 + pos * (a1 - a0);
            g.setColour (text);
            g.drawLine (c.x + std::sin (ang) * radius * 0.35f, c.y - std::cos (ang) * radius * 0.35f,
                        c.x + std::sin (ang) * radius * 0.8f, c.y - std::cos (ang) * radius * 0.8f, 2.0f);
        }
    };
}
