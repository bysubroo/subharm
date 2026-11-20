#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"
#include "Types.h"

// SubHarm design system ("neon studio"): deep ink background, violet/teal glow on
// whatever is active, Manrope for the interface, DSEG7 LCD digits for values.
// References: Auto-Tune (neon ring knobs), Waves Atlas (LED rings, LCD readouts),
// Waves Harmony (line tokens, key display), VocAlign (timeline), FabFilter (top bar, colour as language).
namespace subharm::theme
{
    // ---------------- tokens ----------------
    inline const juce::Colour bg        { 0xff0e0d16 };   // window
    inline const juce::Colour bgDeep    { 0xff0a0910 };   // top bar, status bar, lanes
    inline const juce::Colour surface   { 0xff17151f };   // panels
    inline const juce::Colour surfaceHi { 0xff1f1c2a };   // fields, chips, hover
    inline const juce::Colour border    { 0xff2c2838 };
    inline const juce::Colour text      { 0xffece9f5 };
    inline const juce::Colour muted     { 0xff8a86a0 };
    inline const juce::Colour faint     { 0xff55516a };
    inline const juce::Colour primary   { 0xff8b5cff };   // violet
    inline const juce::Colour primaryHi { 0xffb18cff };
    inline const juce::Colour secondary { 0xff2ee6c8 };   // teal
    inline const juce::Colour warm      { 0xffff9f5a };   // coral
    inline const juce::Colour success   { 0xff4be38f };
    inline const juce::Colour warning   { 0xffffc94d };
    inline const juce::Colour error     { 0xffff5a6e };
    inline const juce::Colour info      { 0xff5fd3ff };
    inline const juce::Colour wave      { 0xff6e6496 };   // vocal outside parts

    // legacy names used by the timeline / lists
    inline const juce::Colour panel = surface;
    inline const juce::Colour panelHi = surfaceHi;
    inline const juce::Colour line = border;
    inline const juce::Colour dim = muted;
    inline const juce::Colour accent = primary;

    // Every harmony line keeps one colour everywhere (tokens, stacks list, stems).
    inline juce::Colour lineColour (LineType t)
    {
        switch (t)
        {
            case LineType::PedalRoot:  return juce::Colour (0xff2ee6c8);
            case LineType::PedalFifth: return juce::Colour (0xff5fd3ff);
            case LineType::Double:     return juce::Colour (0xffc9b8ff);
            case LineType::ThirdUp:    return juce::Colour (0xff8b5cff);
            case LineType::ThirdDown:  return juce::Colour (0xffb26bff);
            case LineType::FifthUp:    return juce::Colour (0xffff6ec7);
            case LineType::FifthDown:  return juce::Colour (0xffff5a8a);
            case LineType::OctaveUp:   return juce::Colour (0xffff9f5a);
            case LineType::OctaveDown: return juce::Colour (0xffffc94d);
            case LineType::ChordHigh:  return juce::Colour (0xff4be38f);
            case LineType::ChordLow:   return juce::Colour (0xffa6f05b);
            default:                   return primary;
        }
    }

    inline juce::Colour partColour (int i)
    {
        static const juce::uint32 c[8] = { 0xffff9f5a, 0xff2ee6c8, 0xff8b5cff, 0xffff6ec7,
                                            0xffffc94d, 0xff5fd3ff, 0xff4be38f, 0xffb26bff };
        return juce::Colour (c[((i % 8) + 8) % 8]);
    }

    // ---------------- type ----------------
    enum class Weight { Medium, SemiBold, Bold, ExtraBold };

    inline juce::Typeface::Ptr typeface (Weight w)
    {
        static juce::Typeface::Ptr tf[4] = {
            juce::Typeface::createSystemTypefaceFor (BinaryData::ManropeMedium_ttf, BinaryData::ManropeMedium_ttfSize),
            juce::Typeface::createSystemTypefaceFor (BinaryData::ManropeSemiBold_ttf, BinaryData::ManropeSemiBold_ttfSize),
            juce::Typeface::createSystemTypefaceFor (BinaryData::ManropeBold_ttf, BinaryData::ManropeBold_ttfSize),
            juce::Typeface::createSystemTypefaceFor (BinaryData::ManropeExtraBold_ttf, BinaryData::ManropeExtraBold_ttfSize) };
        return tf[(int) w];
    }

    inline juce::Font ui (float h, Weight w = Weight::SemiBold)
    {
        return juce::Font (juce::FontOptions (typeface (w)).withHeight (h));
    }

    // small caps labels: bold, tracked out
    inline juce::Font label (float h = 10.0f) { return ui (h, Weight::Bold).withExtraKerningFactor (0.12f); }

    inline juce::Font lcd (float h)
    {
        static juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor (BinaryData::DSEG7ClassicBold_ttf,
                                                                                BinaryData::DSEG7ClassicBold_ttfSize);
        return juce::Font (juce::FontOptions (tf).withHeight (h));
    }

    // compatibility helper for older call sites
    inline juce::Font font (float h, bool bold = false) { return ui (h, bold ? Weight::Bold : Weight::SemiBold); }

    // ---------------- glow helpers ----------------
    inline void glowPath (juce::Graphics& g, const juce::Path& p, juce::Colour c, int radius, float alpha = 0.7f)
    {
        juce::DropShadow (c.withAlpha (alpha), radius, {}).drawForPath (g, p);
    }

    inline void glowStroke (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width, int radius)
    {
        juce::Path s;
        juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (s, p);
        glowPath (g, s, c, radius);
        g.setColour (c);
        g.fillPath (s);
    }

    inline void panelBox (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, const juce::String& hint = {})
    {
        g.setColour (surface);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (border);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
        g.setColour (muted);
        g.setFont (label (10.0f));
        g.drawText (title, r.reduced (16.0f, 0).withHeight (34.0f), juce::Justification::centredLeft);
        if (hint.isNotEmpty())
        {
            g.setColour (faint);
            g.setFont (ui (10.5f, Weight::Medium));
            g.drawText (hint, r.reduced (16.0f, 0).withHeight (34.0f), juce::Justification::centredRight);
        }
    }

    // LCD readout: label above, DSEG7 digits with the unlit "8" segments behind them.
    inline void lcdReadout (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title,
                            const juce::String& value, const juce::String& ghost, juce::Colour c)
    {
        g.setColour (bgDeep);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (border);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (faint);
        g.setFont (label (8.5f));
        g.drawText (title, r.reduced (10.0f, 5.0f).withHeight (12.0f), juce::Justification::topLeft);
        auto v = r.reduced (10.0f, 4.0f).withTrimmedTop (13.0f);
        g.setFont (lcd (juce::jmin (20.0f, v.getHeight() * 0.85f)));
        g.setColour (c.withAlpha (0.08f));
        g.drawText (ghost, v, juce::Justification::centredRight);
        juce::GlyphArrangement ga;
        ga.addFittedText (g.getCurrentFont(), value, v.getX(), v.getY(), v.getWidth(), v.getHeight(), juce::Justification::centredRight, 1);
        juce::Path p; ga.createPath (p);
        glowPath (g, p, c, 6, 0.8f);
        g.setColour (c.brighter (0.25f));
        g.fillPath (p);
    }

    // Horizontal LED meter (side chain / output)
    inline void ledMeter (juce::Graphics& g, juce::Rectangle<float> r, float level01, juce::Colour c)
    {
        const int n = 20;
        const float w = r.getWidth() / n;
        for (int i = 0; i < n; ++i)
        {
            const float frac = (i + 1) / (float) n;
            auto seg = juce::Rectangle<float> (r.getX() + i * w + 1.0f, r.getY(), w - 2.0f, r.getHeight());
            const auto col = frac > 0.9f ? error : frac > 0.72f ? warning : c;
            const bool on = level01 >= frac - 0.5f / n;
            g.setColour (on ? col : col.withAlpha (0.12f));
            g.fillRoundedRectangle (seg, 1.5f);
        }
    }

    // ---------------- LookAndFeel ----------------
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel()
        {
            setColour (juce::ResizableWindow::backgroundColourId, bg);
            setColour (juce::TextButton::buttonColourId, surfaceHi);
            setColour (juce::TextButton::textColourOffId, text);
            setColour (juce::TextButton::textColourOnId, text);
            setColour (juce::ComboBox::backgroundColourId, surfaceHi);
            setColour (juce::ComboBox::textColourId, text);
            setColour (juce::ComboBox::outlineColourId, border);
            setColour (juce::ComboBox::arrowColourId, muted);
            setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff15131c));
            setColour (juce::PopupMenu::textColourId, text);
            setColour (juce::PopupMenu::headerTextColourId, muted);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, primary.withAlpha (0.28f));
            setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
            setColour (juce::Label::textColourId, text);
            setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
            setColour (juce::ScrollBar::thumbColourId, border);
            setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff15131c));
            setColour (juce::TooltipWindow::textColourId, text);
            setColour (juce::TooltipWindow::outlineColourId, border);
            setDefaultSansSerifTypeface (typeface (Weight::Medium));
        }

        static juce::String kind (juce::Component& c) { return c.getProperties()["kind"].toString(); }

        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (1.0f);
            const auto k = kind (b);
            const bool on = b.getToggleState();
            const bool en = b.isEnabled();

            if (k == "token")   // harmony line token (Harmony-style), tinted with the line colour
            {
                const auto tint = juce::Colour ((juce::uint32) (juce::int64) b.getProperties()["tint"]);
                const float rad = r.getHeight() * 0.5f;
                if (on)
                {
                    juce::Path p; p.addRoundedRectangle (r.reduced (2.0f), rad);
                    glowPath (g, p, tint, 10, 0.4f);
                    g.setColour (surfaceHi);
                    g.fillRoundedRectangle (r, rad);
                    g.setGradientFill (juce::ColourGradient (tint.withAlpha (over ? 0.30f : 0.22f), r.getX(), r.getY(),
                                                             tint.withAlpha (0.06f), r.getRight(), r.getY(), false));
                    g.fillRoundedRectangle (r, rad);
                    g.setColour (tint);
                    g.drawRoundedRectangle (r, rad, 1.4f);
                }
                else
                {
                    g.setColour (over ? surfaceHi.brighter (0.06f) : surfaceHi);
                    g.fillRoundedRectangle (r, rad);
                    g.setColour (border);
                    g.drawRoundedRectangle (r, rad, 1.0f);
                }
                g.setColour (on ? tint : tint.withAlpha (0.35f));   // colour dot
                g.fillEllipse (r.getX() + 11.0f, r.getCentreY() - 3.5f, 7.0f, 7.0f);
                return;
            }
            if (k == "seg")     // item of a segmented control
            {
                if (on)
                {
                    juce::Path p; p.addRoundedRectangle (r.reduced (1.0f), 6.0f);
                    glowPath (g, p, primary, 8, 0.35f);
                    g.setColour (juce::Colour (0xff2a2342));
                    g.fillRoundedRectangle (r, 6.0f);
                    g.setColour (primary);
                    g.drawRoundedRectangle (r, 6.0f, 1.2f);
                }
                else if (over)
                {
                    g.setColour (surfaceHi.brighter (0.05f));
                    g.fillRoundedRectangle (r, 6.0f);
                }
                return;
            }
            if (k == "primary") // STACK HARMONIES
            {
                juce::Path p; p.addRoundedRectangle (r.reduced (2.0f), 9.0f);
                if (en) glowPath (g, p, primary, over ? 18 : 12, over ? 0.75f : 0.55f);
                juce::ColourGradient grad (en ? juce::Colour (0xff9d6bff) : surfaceHi, r.getX(), r.getY(),
                                           en ? juce::Colour (0xff6a3df0) : surfaceHi, r.getRight(), r.getBottom(), false);
                g.setGradientFill (grad);
                g.fillRoundedRectangle (r, 9.0f);
                if (down && en) { g.setColour (juce::Colours::black.withAlpha (0.15f)); g.fillRoundedRectangle (r, 9.0f); }
                g.setColour (juce::Colours::white.withAlpha (en ? 0.18f : 0.05f));
                g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 1.0f);
                return;
            }
            if (k == "rec")
            {
                const bool armed = on;
                g.setColour (armed ? error.withAlpha (0.16f) : (over ? surfaceHi.brighter (0.05f) : surfaceHi));
                g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
                g.setColour (armed ? error : border);
                g.drawRoundedRectangle (r, r.getHeight() * 0.5f, armed ? 1.4f : 1.0f);
                juce::Path dot; dot.addEllipse (r.getX() + 14.0f, r.getCentreY() - 6.0f, 12.0f, 12.0f);
                if (armed) glowPath (g, dot, error, 10, 0.9f);
                g.setColour (armed ? error : error.withAlpha (0.75f));
                g.fillPath (dot);
                return;
            }
            // ghost (zoom, presets, export, folder)
            g.setColour (on ? surfaceHi.brighter (0.08f) : over ? surfaceHi.brighter (0.04f) : surfaceHi);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (border);
            g.drawRoundedRectangle (r, 6.0f, 1.0f);
            if (down) { g.setColour (juce::Colours::black.withAlpha (0.2f)); g.fillRoundedRectangle (r, 6.0f); }
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool) override
        {
            const auto k = kind (b);
            const bool on = b.getToggleState();
            auto r = b.getLocalBounds();
            juce::Colour c = text;
            juce::Font f = ui (12.0f, Weight::SemiBold);

            if (k == "token") { r.removeFromLeft (24); f = ui (11.5f, Weight::Bold); c = on ? juce::Colours::white : muted; }
            else if (k == "seg") { f = ui (12.0f, on ? Weight::Bold : Weight::SemiBold); c = on ? juce::Colours::white : (over ? text : muted); }
            else if (k == "primary") { f = label (12.5f); c = b.isEnabled() ? juce::Colours::white : faint; }
            else if (k == "rec") { r.removeFromLeft (30); f = label (11.0f); c = on ? juce::Colour (0xffffd5da) : text; }
            else { f = ui (11.5f, Weight::SemiBold); c = b.isEnabled() ? (over ? text : muted.brighter (0.25f)) : faint; }

            if (! b.isEnabled() && k != "primary") c = faint;
            g.setFont (f);
            g.setColour (c);
            g.drawText (b.getButtonText(), r.reduced (6, 0), k == "token" ? juce::Justification::centredLeft : juce::Justification::centred, true);
        }

        juce::Font getTextButtonFont (juce::TextButton&, int) override { return ui (12.0f); }

        // switch (Hear lead)
        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
        {
            auto r = b.getLocalBounds().toFloat();
            auto sw = r.removeFromLeft (38.0f).withSizeKeepingCentre (34.0f, 18.0f);
            const bool on = b.getToggleState();
            g.setColour (on ? secondary.withAlpha (0.25f) : surfaceHi);
            g.fillRoundedRectangle (sw, 9.0f);
            g.setColour (on ? secondary : border);
            g.drawRoundedRectangle (sw, 9.0f, 1.0f);
            juce::Path knob; knob.addEllipse (on ? sw.getRight() - 15.0f : sw.getX() + 3.0f, sw.getY() + 3.0f, 12.0f, 12.0f);
            if (on) glowPath (g, knob, secondary, 8, 0.8f);
            g.setColour (on ? secondary : muted);
            g.fillPath (knob);
            g.setColour (over ? text : muted.brighter (0.2f));
            g.setFont (ui (11.5f, Weight::SemiBold));
            g.drawText (b.getButtonText(), r.withTrimmedLeft (6.0f), juce::Justification::centredLeft);
        }

        juce::Font getComboBoxFont (juce::ComboBox& box) override
        {
            return box.getProperties()["big"] ? ui (17.0f, Weight::ExtraBold) : ui (12.5f, Weight::SemiBold);
        }
        juce::Font getPopupMenuFont() override { return ui (13.0f, Weight::Medium); }

        void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
        {
            auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
            const bool big = box.getProperties()["big"];
            const bool over = box.isMouseOver (true);
            g.setColour (big ? bgDeep : surfaceHi);
            g.fillRoundedRectangle (r, big ? 8.0f : 6.0f);
            g.setColour (big ? primary.withAlpha (over ? 0.9f : 0.55f) : (over ? muted : border));
            g.drawRoundedRectangle (r, big ? 8.0f : 6.0f, big ? 1.3f : 1.0f);
            juce::Path chev;
            const float cx = w - 16.0f, cy = h * 0.5f;
            chev.startNewSubPath (cx - 4.0f, cy - 2.0f); chev.lineTo (cx, cy + 2.0f); chev.lineTo (cx + 4.0f, cy - 2.0f);
            g.setColour (big ? primaryHi : muted);
            g.strokePath (chev, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        void positionComboBoxText (juce::ComboBox& box, juce::Label& lbl) override
        {
            const bool big = box.getProperties()["big"];
            lbl.setBounds (big ? 14 : 10, 1, box.getWidth() - (big ? 40 : 30), box.getHeight() - 2);
            lbl.setFont (getComboBoxFont (box));
            lbl.setColour (juce::Label::textColourId, big ? juce::Colours::white : text);
        }

        // Neon ring knob (Auto-Tune / Atlas): dark puck, violet->teal value arc with glow,
        // LED dots around, value in LCD digits in the centre.
        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
        {
            auto b = juce::Rectangle<int> (x, y, w, h).toFloat();
            const float size = juce::jmin (b.getWidth(), b.getHeight());
            const auto c = b.getCentre();
            const float ringR = size * 0.40f, puckR = size * 0.30f;
            const juce::Colour ca = primary;
            const juce::Colour cb = secondary;

            const int dots = 31;   // LED dots (Atlas)
            for (int i = 0; i < dots; ++i)
            {
                const float t = i / (float) (dots - 1);
                const float a = a0 + (a1 - a0) * t;
                const float dr = size * 0.48f;
                const bool lit = t <= pos + 1e-4f;
                g.setColour (lit ? ca.interpolatedWith (cb, t).withAlpha (0.9f) : faint.withAlpha (0.35f));
                const float d = size * 0.016f + 0.7f;
                g.fillEllipse (c.x + std::sin (a) * dr - d, c.y - std::cos (a) * dr - d, d * 2, d * 2);
            }
            juce::Path track;
            track.addCentredArc (c.x, c.y, ringR, ringR, 0, a0, a1, true);
            g.setColour (border);
            g.strokePath (track, juce::PathStrokeType (size * 0.05f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            if (pos > 0.002f)
            {
                juce::Path val;
                val.addCentredArc (c.x, c.y, ringR, ringR, 0, a0, a0 + pos * (a1 - a0), true);
                juce::Path stroked;
                juce::PathStrokeType (size * 0.05f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (stroked, val);
                glowPath (g, stroked, ca.interpolatedWith (cb, pos * 0.6f), (int) (size * 0.12f), 0.8f);
                g.setGradientFill (juce::ColourGradient (ca, c.x - ringR, c.y + ringR, cb, c.x + ringR, c.y - ringR, false));
                g.fillPath (stroked);
            }
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2737), c.x, c.y - puckR, juce::Colour (0xff121019), c.x, c.y + puckR, false));
            g.fillEllipse (c.x - puckR, c.y - puckR, puckR * 2, puckR * 2);
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.drawEllipse (c.x - puckR + 0.5f, c.y - puckR + 0.5f, puckR * 2 - 1, puckR * 2 - 1, 1.0f);
            const float ang = a0 + pos * (a1 - a0);
            const float pr = puckR * 0.80f;
            juce::Path dot; dot.addEllipse (c.x + std::sin (ang) * pr - 2.5f, c.y - std::cos (ang) * pr - 2.5f, 5.0f, 5.0f);
            glowPath (g, dot, ca, 6, 0.9f);
            g.setColour (juce::Colours::white);
            g.fillPath (dot);
            g.setFont (lcd (juce::jmax (9.0f, size * 0.11f)));
            g.setColour (text);
            g.drawText (s.getTextFromValue (s.getValue()), juce::Rectangle<float> (c.x - puckR, c.y - size * 0.07f, puckR * 2, size * 0.14f),
                        juce::Justification::centred);
        }

        void drawLabel (juce::Graphics& g, juce::Label& l) override
        {
            if (l.isBeingEdited()) return;
            g.setFont (getLabelFont (l));
            g.setColour (l.findColour (juce::Label::textColourId));
            g.drawText (l.getText(), l.getLocalBounds(), l.getJustificationType(), true);
        }

        void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
        {
            g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
            g.setColour (border);
            g.drawRect (0, 0, w, h);
        }

        void drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                            int thumbStart, int thumbSize, bool over, bool) override
        {
            auto t = vertical ? juce::Rectangle<int> (x + w / 2 - 2, thumbStart, 4, thumbSize)
                              : juce::Rectangle<int> (thumbStart, y + h / 2 - 2, thumbSize, 4);
            g.setColour (over ? muted : border.brighter (0.2f));
            g.fillRoundedRectangle (t.toFloat(), 2.0f);
        }
    };
}
