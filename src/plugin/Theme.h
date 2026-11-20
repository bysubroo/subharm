#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// SubHarm "hardware" look: a rack unit with a brushed faceplate, recessed sections,
// bakelite knobs, lamp buttons and an analog VU meter. Original design.
namespace subharm::theme
{
    // faceplate
    inline const juce::Colour plateTop  { 0xff3d3b37 };
    inline const juce::Colour plateBot  { 0xff2b2a27 };
    inline const juce::Colour ear       { 0xff23221f };
    // displays / insets
    inline const juce::Colour bg        { 0xff11100f };
    inline const juce::Colour panel     { 0xff191816 };
    // controls
    inline const juce::Colour panelHi   { 0xff45423c };
    inline const juce::Colour line      { 0xff4d4941 };
    // print on the faceplate
    inline const juce::Colour text      { 0xffefe7d3 };
    inline const juce::Colour dim       { 0xffc2b79c };
    inline const juce::Colour faint     { 0xff857d69 };
    // lamps
    inline const juce::Colour accent    { 0xffffa21f };   // amber
    inline const juce::Colour accentDim { 0xff5e3f12 };
    inline const juce::Colour red       { 0xffe8432d };
    inline const juce::Colour green     { 0xff8fd16a };
    inline const juce::Colour wave      { 0xffd9cfb6 };

    inline juce::Colour partColour (int i)
    {
        static const juce::uint32 c[8] = { 0xffffa21f, 0xff6fb7c9, 0xffe8432d, 0xffb59ad6,
                                            0xff8fd16a, 0xffe07fa7, 0xff5fa3e0, 0xffe9d36b };
        return juce::Colour (c[((i % 8) + 8) % 8]);
    }

    inline juce::Font font (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }

    // Text "engraved" into the plate: dark shadow below, light print on top.
    inline void engrave (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, juce::Justification j,
                         juce::Colour c = dim, float size = 10.5f, bool bold = true)
    {
        g.setFont (font (size, bold).withExtraKerningFactor (0.08f));
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawText (t, r.translated (0, 1), j, true);
        g.setColour (c);
        g.drawText (t, r, j, true);
    }

    // Recessed area (sections, displays): dark edge top-left, highlight bottom-right.
    inline void recess (juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour fill)
    {
        g.setColour (fill);
        g.fillRoundedRectangle (r, radius);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawRoundedRectangle (r.translated (0.0f, -0.5f), radius, 1.2f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (r.translated (0.0f, 1.0f), radius, 1.0f);
    }

    inline void screw (juce::Graphics& g, juce::Point<float> c, float rad, float angle)
    {
        juce::ColourGradient grad (juce::Colour (0xffd8d4cc), c.x - rad, c.y - rad, juce::Colour (0xff6a665e), c.x + rad, c.y + rad, false);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2, 1.0f);
        const float dx = std::cos (angle) * rad * 0.75f, dy = std::sin (angle) * rad * 0.75f;
        g.setColour (juce::Colour (0xff3a3833));
        g.drawLine (c.x - dx, c.y - dy, c.x + dx, c.y + dy, 1.8f);
        g.drawLine (c.x + dy, c.y - dx, c.x - dy, c.y + dx, 1.8f);
    }

    inline void lamp (juce::Graphics& g, juce::Point<float> c, float rad, juce::Colour col, bool on)
    {
        if (on)
        {
            g.setColour (col.withAlpha (0.25f));
            g.fillEllipse (c.x - rad * 2.2f, c.y - rad * 2.2f, rad * 4.4f, rad * 4.4f);
        }
        juce::ColourGradient grad (on ? col.brighter (0.6f) : col.darker (2.2f), c.x - rad * 0.4f, c.y - rad * 0.5f,
                                   on ? col : col.darker (3.0f), c.x + rad, c.y + rad, true);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2, 1.0f);
    }

    // Brushed-metal plate with rack ears and screws, rendered once per size.
    inline juce::Image makeFaceplate (int w, int h, int earW)
    {
        juce::Image img (juce::Image::RGB, juce::jmax (1, w), juce::jmax (1, h), true);
        juce::Graphics g (img);
        g.setGradientFill (juce::ColourGradient (plateTop, 0, 0, plateBot, 0, (float) h, false));
        g.fillAll();
        juce::Random rng (7);
        for (int y = 0; y < h; ++y)   // brushed grain
        {
            g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (rng.nextFloat() * 0.035f));
            g.drawHorizontalLine (y, 0.0f, (float) w);
        }
        for (int side = 0; side < 2; ++side)
        {
            const float x = side == 0 ? 0.0f : (float) (w - earW);
            g.setGradientFill (juce::ColourGradient (ear.brighter (0.1f), x, 0, ear, x + earW, 0, false));
            g.fillRect (x, 0.0f, (float) earW, (float) h);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawVerticalLine (side == 0 ? earW - 1 : w - earW, 0.0f, (float) h);
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.drawVerticalLine (side == 0 ? earW : w - earW + 1, 0.0f, (float) h);
            const float cx = x + earW * 0.5f;
            for (float cy : { 34.0f, (float) h - 34.0f })
            {
                g.setColour (juce::Colours::black.withAlpha (0.5f));
                g.fillRoundedRectangle (cx - 6.0f, cy - 11.0f, 12.0f, 22.0f, 6.0f);   // rack slot
                screw (g, { cx, cy }, 5.5f, 0.4f + side * 0.9f);
            }
        }
        return img;
    }

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel()
        {
            setColour (juce::ResizableWindow::backgroundColourId, plateBot);
            setColour (juce::TextButton::buttonColourId, panelHi);
            setColour (juce::TextButton::buttonOnColourId, panelHi);
            setColour (juce::TextButton::textColourOffId, text);
            setColour (juce::TextButton::textColourOnId, accent.brighter (0.3f));
            setColour (juce::ComboBox::backgroundColourId, bg);
            setColour (juce::ComboBox::outlineColourId, line);
            setColour (juce::ComboBox::textColourId, text);
            setColour (juce::ComboBox::arrowColourId, dim);
            setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff24221f));
            setColour (juce::PopupMenu::textColourId, text);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, accentDim);
            setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
            setColour (juce::Label::textColourId, text);
            setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
            setColour (juce::Slider::textBoxTextColourId, dim);
            setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            setColour (juce::ToggleButton::textColourId, dim);
            setColour (juce::ToggleButton::tickColourId, accent);
            setColour (juce::ToggleButton::tickDisabledColourId, faint);
            setColour (juce::ScrollBar::thumbColourId, line);
            setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff24221f));
            setColour (juce::TooltipWindow::textColourId, text);
        }

        // Hardware push button. Toggled = pressed in + amber lamp lit.
        // Buttons whose colour is 'accent' are the big primary keys; componentID "rec" gets a red lamp.
        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (1.0f);
            const bool on = b.getToggleState();
            const bool primary = b.findColour (juce::TextButton::buttonColourId) == accent;
            const bool isRec = b.getComponentID() == "rec";
            const float rad = juce::jmin (5.0f, r.getHeight() * 0.25f);

            g.setColour (juce::Colours::black.withAlpha (0.6f));   // shadow / bezel
            g.fillRoundedRectangle (r.translated (0, 1.5f), rad);

            auto face = r.withTrimmedBottom (on || down ? 0.0f : 1.5f);
            if (on || down) face = face.translated (0, 1.0f).withTrimmedBottom (1.0f);
            juce::Colour base = primary ? juce::Colour (0xff5a554c) : panelHi;
            if (over) base = base.brighter (0.07f);
            if (on || down) base = base.darker (0.25f);
            if (! b.isEnabled()) base = base.darker (0.4f);
            g.setGradientFill (juce::ColourGradient (base.brighter (0.18f), 0, face.getY(), base.darker (0.25f), 0, face.getBottom(), false));
            g.fillRoundedRectangle (face, rad);
            g.setColour (juce::Colours::white.withAlpha (on ? 0.04f : 0.12f));
            g.drawHorizontalLine ((int) face.getY() + 1, face.getX() + rad, face.getRight() - rad);
            g.setColour (juce::Colours::black.withAlpha (0.8f));
            g.drawRoundedRectangle (face, rad, 1.0f);

            if (hasLamp (b))
            {
                const float lr = juce::jlimit (2.0f, 3.5f, face.getHeight() * 0.11f);
                lamp (g, { face.getX() + 6.0f + lr, face.getCentreY() }, lr,
                      isRec ? red : accent, on || (primary && b.isEnabled()));
            }
        }

        static bool hasLamp (juce::Button& b)
        {
            return b.findColour (juce::TextButton::buttonColourId) == accent || b.getComponentID() == "rec"
                   || b.getClickingTogglesState() || b.getRadioGroupId() != 0 || b.getProperties()["lamp"];
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down) override
        {
            const bool on = b.getToggleState();
            auto r = b.getLocalBounds().withTrimmedLeft (hasLamp (b) ? 12 : 0).translated (0, (on || down) ? 1 : 0);
            g.setFont (getTextButtonFont (b, b.getHeight()));
            juce::Colour c = on ? accent.brighter (0.35f) : text;
            if (! b.isEnabled()) c = faint;
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawText (b.getButtonText(), r.translated (0, 1), juce::Justification::centred, true);
            g.setColour (c);
            g.drawText (b.getButtonText(), r, juce::Justification::centred, true);
        }

        juce::Font getTextButtonFont (juce::TextButton&, int h) override
        {
            return font (juce::jmin (12.5f, h * 0.40f), true).withExtraKerningFactor (0.02f);
        }
        juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.0f, true); }
        juce::Font getPopupMenuFont() override { return font (13.0f); }

        // Small backlit display window
        void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
        {
            auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
            recess (g, r, 4.0f, bg);
            juce::Path arrow;
            const float ax = w - 16.0f, ay = h * 0.5f;
            arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
            g.setColour (accent.withAlpha (0.8f));
            g.fillPath (arrow);
        }

        void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
        {
            label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
            label.setFont (getComboBoxFont (box));
            label.setColour (juce::Label::textColourId, accent.brighter (0.4f));
        }

        // Machined chrome knob with a knurled edge and a silkscreened 0-10 scale.
        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override
        {
            auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
            const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
            const auto c = bounds.getCentre();
            const float scaleR = size * 0.47f, knobR = size * 0.33f, capR = size * 0.24f;

            g.setFont (font (juce::jmax (7.0f, size * 0.085f), true));
            for (int i = 0; i <= 10; ++i)   // printed scale + numbers
            {
                const float a = a0 + (a1 - a0) * i / 10.0f;
                g.setColour (text.withAlpha (0.9f));
                g.drawLine (c.x + std::sin (a) * knobR * 1.08f, c.y - std::cos (a) * knobR * 1.08f,
                            c.x + std::sin (a) * knobR * 1.2f, c.y - std::cos (a) * knobR * 1.2f, 1.2f);
                if (i % 2 == 0)
                {
                    const float tx = c.x + std::sin (a) * scaleR, ty = c.y - std::cos (a) * scaleR;
                    g.drawText (juce::String (i), juce::Rectangle<float> (tx - 8, ty - 5, 16, 10), juce::Justification::centred);
                }
            }
            g.setColour (juce::Colours::black.withAlpha (0.55f));   // shadow on the plate
            g.fillEllipse (c.x - knobR + 1.5f, c.y - knobR + 3.0f, knobR * 2, knobR * 2);

            // knurled skirt
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff9a968e), c.x, c.y - knobR, juce::Colour (0xff2e2c29), c.x, c.y + knobR, false));
            g.fillEllipse (c.x - knobR, c.y - knobR, knobR * 2, knobR * 2);
            const int teeth = 36;
            for (int i = 0; i < teeth; ++i)
            {
                const float a = juce::MathConstants<float>::twoPi * i / teeth;
                const float lit = 0.5f + 0.5f * std::cos (a + 0.8f);
                g.setColour (juce::Colours::black.withAlpha (0.35f + 0.25f * (1.0f - lit)));
                g.drawLine (c.x + std::sin (a) * capR * 1.05f, c.y - std::cos (a) * capR * 1.05f,
                            c.x + std::sin (a) * knobR * 0.98f, c.y - std::cos (a) * knobR * 0.98f, 1.0f);
            }
            // polished cap (chrome: conical highlight)
            juce::ColourGradient chrome (juce::Colour (0xfff4f2ee), c.x - capR * 0.6f, c.y - capR * 0.8f,
                                         juce::Colour (0xff6d6a64), c.x + capR * 0.7f, c.y + capR * 0.9f, false);
            chrome.addColour (0.45, juce::Colour (0xffc9c5bd));
            chrome.addColour (0.55, juce::Colour (0xff8f8b84));
            g.setGradientFill (chrome);
            g.fillEllipse (c.x - capR, c.y - capR, capR * 2, capR * 2);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawEllipse (c.x - capR, c.y - capR, capR * 2, capR * 2, 1.0f);
            g.drawEllipse (c.x - knobR, c.y - knobR, knobR * 2, knobR * 2, 1.0f);

            const float ang = a0 + pos * (a1 - a0);   // engraved pointer
            g.setColour (juce::Colour (0xff1a1917));
            g.drawLine (c.x + std::sin (ang) * capR * 0.2f, c.y - std::cos (ang) * capR * 0.2f,
                        c.x + std::sin (ang) * knobR * 0.95f, c.y - std::cos (ang) * knobR * 0.95f, 2.6f);
            g.setColour (text);
            g.drawLine (c.x + std::sin (ang) * capR * 1.05f, c.y - std::cos (ang) * capR * 1.05f,
                        c.x + std::sin (ang) * knobR * 0.95f, c.y - std::cos (ang) * knobR * 0.95f, 1.6f);
        }

        void drawTickBox (juce::Graphics& g, juce::Component&, float x, float y, float w, float h, bool ticked, bool, bool, bool) override
        {
            const float s = juce::jmin (w, h);
            lamp (g, { x + s * 0.5f, y + h * 0.5f }, s * 0.32f, accent, ticked);
        }

        void drawLabel (juce::Graphics& g, juce::Label& l) override
        {
            if (l.isBeingEdited()) return;
            g.setFont (getLabelFont (l));
            g.setColour (l.findColour (juce::Label::textColourId));
            g.drawText (l.getText(), l.getLocalBounds(), l.getJustificationType(), true);
        }
    };

    // Analog VU meter (side chain level).
    inline void drawVu (juce::Graphics& g, juce::Rectangle<float> r, float needle01, bool connected)
    {
        recess (g, r, 4.0f, juce::Colours::black);
        auto face = r.reduced (3.0f);
        // warm incandescent backlight: bright in the middle, falling off to the corners
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffe3a3), face.getCentreX(), face.getY() + face.getHeight() * 0.65f,
                                                 juce::Colour (0xffc98d36), face.getX(), face.getY(), true));
        g.fillRoundedRectangle (face, 3.0f);

        const juce::Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.35f);
        const float rad = face.getHeight() * 0.95f;
        const float a0 = -0.85f, a1 = 0.85f;
        juce::Path arc;
        arc.addCentredArc (pivot.x, pivot.y, rad * 0.88f, rad * 0.88f, 0, a0, a1, true);
        g.setColour (juce::Colour (0xff2a2520));
        g.strokePath (arc, juce::PathStrokeType (1.0f));
        juce::Path redArc;
        redArc.addCentredArc (pivot.x, pivot.y, rad * 0.88f, rad * 0.88f, 0, a0 + (a1 - a0) * 0.72f, a1, true);
        g.setColour (red);
        g.strokePath (redArc, juce::PathStrokeType (3.0f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = a0 + (a1 - a0) * i / 10.0f;
            const float r0 = rad * (i % 5 == 0 ? 0.78f : 0.83f), r1 = rad * 0.88f;
            g.setColour (i >= 8 ? red : juce::Colour (0xff2a2520));
            g.drawLine (pivot.x + std::sin (a) * r0, pivot.y - std::cos (a) * r0, pivot.x + std::sin (a) * r1, pivot.y - std::cos (a) * r1, 1.0f);
        }
        g.setColour (juce::Colour (0xff2a2520));
        g.setFont (font (8.5f, true));
        g.drawText ("VU", face.withTrimmedTop (face.getHeight() * 0.52f), juce::Justification::centredTop);
        g.setFont (font (7.0f, true));
        g.drawText (connected ? "SIDE CHAIN" : "NO SIDE CHAIN", face.withTrimmedTop (face.getHeight() * 0.74f), juce::Justification::centredTop);

        const float a = a0 + (a1 - a0) * juce::jlimit (0.0f, 1.05f, needle01);
        g.saveState();
        g.reduceClipRegion (face.toNearestInt());
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.drawLine (pivot.x, pivot.y, pivot.x + std::sin (a) * rad * 0.95f, pivot.y - std::cos (a) * rad * 0.95f, 1.3f);
        g.restoreState();
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.18f), face.getX(), face.getY(),
                                                 juce::Colours::transparentWhite, face.getX(), face.getCentreY(), false));
        g.fillRoundedRectangle (face, 3.0f);   // glass reflection
    }

    // Segmented LED ladder (vertical), green -> amber -> red.
    inline void drawLedLadder (juce::Graphics& g, juce::Rectangle<float> r, float level01, const juce::String& label)
    {
        recess (g, r, 3.0f, juce::Colours::black);
        auto area = r.reduced (4.0f).withTrimmedBottom (12.0f);
        const int n = 14;
        const float segH = area.getHeight() / n;
        for (int i = 0; i < n; ++i)
        {
            const float frac = (i + 1) / (float) n;
            auto seg = juce::Rectangle<float> (area.getX(), area.getBottom() - (i + 1) * segH + 1.0f, area.getWidth(), segH - 2.0f);
            const auto col = frac > 0.86f ? red : frac > 0.65f ? accent : green;
            const bool on = level01 >= frac - 0.5f / n;
            g.setColour (on ? col : col.darker (2.5f).withAlpha (0.6f));
            g.fillRoundedRectangle (seg, 1.0f);
            if (on)
            {
                g.setColour (col.withAlpha (0.25f));
                g.fillRoundedRectangle (seg.expanded (1.5f, 1.0f), 2.0f);
            }
        }
        g.setColour (faint);
        g.setFont (font (7.5f, true));
        g.drawText (label, r.withTrimmedTop (r.getHeight() - 13.0f), juce::Justification::centred);
    }

    // Metal nameplate fixed with four screws, model name engraved.
    inline void drawNameplate (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& name, const juce::String& sub)
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (r.translated (0, 2.0f), 3.0f);
        juce::ColourGradient al (juce::Colour (0xffd9d5cc), r.getX(), r.getY(), juce::Colour (0xff9b978e), r.getX(), r.getBottom(), false);
        al.addColour (0.5, juce::Colour (0xffc4c0b6));
        g.setGradientFill (al);
        g.fillRoundedRectangle (r, 3.0f);
        juce::Random rng (3);
        for (int i = 0; i < (int) r.getHeight(); ++i)   // brushed aluminium
        {
            g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (rng.nextFloat() * 0.06f));
            g.drawHorizontalLine ((int) r.getY() + i, r.getX() + 2, r.getRight() - 2);
        }
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        for (auto p : { r.getTopLeft() + juce::Point<float> (7, 7), r.getTopRight() + juce::Point<float> (-7, 7),
                        r.getBottomLeft() + juce::Point<float> (7, -7), r.getBottomRight() + juce::Point<float> (-7, -7) })
            screw (g, p, 3.2f, 0.3f);
        auto t = r.reduced (18.0f, 4.0f);
        g.setFont (font (r.getHeight() * 0.46f, true).withExtraKerningFactor (0.04f));
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.drawText (name, t.withHeight (t.getHeight() * 0.68f).translated (0, 1), juce::Justification::centred);
        g.setColour (juce::Colour (0xff1e1d1b));
        g.drawText (name, t.withHeight (t.getHeight() * 0.68f), juce::Justification::centred);
        g.setFont (font (r.getHeight() * 0.17f, true).withExtraKerningFactor (0.2f));
        g.setColour (juce::Colour (0xff3a3833));
        g.drawText (sub, t.withTrimmedTop (t.getHeight() * 0.66f), juce::Justification::centred);
    }
}
