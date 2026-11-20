#include "TimelineView.h"
#include "Theme.h"

namespace subharm
{

void TimelineView::clampView()
{
    const double len = juce::jmax (1.0, proc.getTake().lengthSeconds());
    const double visible = (getWidth() - laneX()) * secPerPx;
    viewStart = juce::jlimit (0.0, juce::jmax (0.0, len - visible * 0.5), viewStart);
}

void TimelineView::fit()
{
    dirty = true;
    const double len = juce::jmax (1.0, proc.getTake().lengthSeconds());
    secPerPx = len / juce::jmax (100.0f, getWidth() - laneX() - 4.0f);
    viewStart = 0.0;
    fitted = true;
    repaint();
}

void TimelineView::zoom (double factor, double anchor)
{
    dirty = true;
    if (anchor < 0) anchor = viewStart + (getWidth() - laneX()) * 0.5 * secPerPx;
    const double ax = (anchor - viewStart) / secPerPx;
    secPerPx = juce::jlimit (0.0005, 1.0, secPerPx / factor);
    viewStart = anchor - ax * secPerPx;
    clampView();
    repaint();
}

void TimelineView::followPlayhead()
{
    if (! fitted && ! proc.getTake().isEmpty() && getWidth() > 0) fit();
    repaint();
}

double TimelineView::snap (double t) const
{
    // snap part edges to 1/16 notes when the tempo is known (hold Alt to drag freely)
    const auto& take = proc.getTake();
    if (take.bpm <= 0 || juce::ModifierKeys::currentModifiers.isAltDown()) return t;
    const double sixteenth = 15.0 / take.bpm;
    const double frac = take.hostStartPpq - std::floor (take.hostStartPpq);
    const double firstGrid = std::fmod ((1.0 - frac) * 60.0 / take.bpm, sixteenth);
    return firstGrid + std::round ((t - firstGrid) / sixteenth) * sixteenth;
}

juce::Path TimelineView::wavePath (juce::Rectangle<int> r, const std::vector<float>& peaks) const
{
    juce::Path p;
    const auto& take = proc.getTake();
    if (peaks.empty()) return p;
    const double peakSec = Take::kPeakBlock / take.sampleRate;
    const float mid = (float) r.getCentreY(), half = r.getHeight() * 0.48f;
    float maxPeak = 0.02f;
    for (float v : peaks) maxPeak = std::max (maxPeak, v);

    std::vector<float> h ((size_t) r.getWidth(), 0.0f);
    for (int x = r.getX(); x < r.getRight(); ++x)
    {
        const double t0 = xToTime ((float) x), t1 = xToTime ((float) x + 1.0f);
        if (t1 < 0 || t0 > take.lengthSeconds()) continue;
        const int i0 = juce::jmax (0, (int) (t0 / peakSec)), i1 = juce::jmin ((int) peaks.size() - 1, (int) (t1 / peakSec));
        float m = 0;
        for (int i = i0; i <= i1; ++i) m = std::max (m, peaks[(size_t) i]);
        h[(size_t) (x - r.getX())] = std::sqrt (m / maxPeak) * half;   // sqrt: quiet bits stay visible
    }
    p.startNewSubPath ((float) r.getX(), mid);
    for (int x = 0; x < r.getWidth(); ++x) p.lineTo ((float) (r.getX() + x), mid - h[(size_t) x]);
    for (int x = r.getWidth() - 1; x >= 0; --x) p.lineTo ((float) (r.getX() + x), mid + h[(size_t) x]);
    p.closeSubPath();
    return p;
}

void TimelineView::paint (juce::Graphics& g)
{
    using namespace theme;
    const auto& take = proc.getTake();

    if (take.isEmpty())   // empty state is live (record status changes)
    {
        g.setColour (bgDeep);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
        auto r = getLocalBounds().reduced (40);
        juce::String head, sub;
        switch (proc.getRecState())
        {
            case SubHarmProcessor::RecState::Armed:
                head = "Armed"; sub = "Press play in your DAW. Stop the DAW when the part is done."; break;
            case SubHarmProcessor::RecState::Recording:
                head = "Recording  " + juce::String (proc.getRecordedSeconds(), 1) + " s"; sub = "Vocal + side chain are coming in as one take."; break;
            case SubHarmProcessor::RecState::Idle:
            case SubHarmProcessor::RecState::Finished:
            default:
                head = "Record your lead with the beat on the side chain";
                sub = "SubHarm reads the chords, key and tempo, then stacks harmonies over them.";
        }
        g.setColour (text);
        g.setFont (ui (17.0f, Weight::Bold));
        g.drawText (head, r.withTrimmedBottom (r.getHeight() / 2), juce::Justification::centredBottom);
        g.setColour (muted);
        g.setFont (ui (12.5f, Weight::Medium));
        g.drawText (sub, r.withTrimmedTop (r.getHeight() / 2 + 6), juce::Justification::centredTop);
        return;
    }

    if (dirty || cache.getWidth() != getWidth() || cache.getHeight() != getHeight())
    {
        cache = juce::Image (juce::Image::ARGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()), true);
        juce::Graphics cg (cache);
        renderStatic (cg);
        dirty = false;
    }
    g.drawImageAt (cache, 0, 0);

    const auto va = vocalArea();
    if (drag == Drag::Create)
    {
        const float x0 = timeToX (std::min (dragAnchor, dragNow)), x1 = timeToX (std::max (dragAnchor, dragNow));
        auto r = juce::Rectangle<float> (x0, (float) va.getY(), x1 - x0, (float) va.getHeight());
        g.setColour (primary.withAlpha (0.16f));
        g.fillRect (r);
        g.setColour (primary);
        g.drawRect (r, 1.0f);
    }

    const double ph = proc.getPlayheadInTake();
    if (proc.isHostPlaying() && ph >= 0 && ph <= take.lengthSeconds())
    {
        const float x = timeToX (ph);
        if (x >= laneX() && x <= getWidth())
        {
            juce::Path line;
            line.addRectangle (x - 0.75f, (float) rulerArea().getY(), 1.5f, (float) (chordArea().getBottom() - rulerArea().getY()));
            glowPath (g, line, secondary, 8, 0.9f);
            g.setColour (secondary);
            g.fillPath (line);
        }
    }
}

void TimelineView::renderStatic (juce::Graphics& g)
{
    using namespace theme;
    const auto& take = proc.getTake();

    g.setColour (bgDeep);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);

    auto laneLabel = [&] (juce::Rectangle<int> r, const char* t)
    {
        g.setColour (faint);
        g.setFont (label (8.5f));
        g.drawText (t, 10, r.getY(), (int) laneX() - 14, r.getHeight(), juce::Justification::centredLeft);
    };
    laneLabel (vocalArea(), "VOCAL");
    laneLabel (beatArea(), "BEAT");
    laneLabel (chordArea(), "CHORDS");

    // ---- bar ruler + grid ----
    const auto ruler = rulerArea();
    if (take.bpm > 0)
    {
        const double beat = 60.0 / take.bpm;
        const double frac = take.hostStartPpq - std::floor (take.hostStartPpq);
        const double firstBeatT = frac < 1e-6 ? 0.0 : (1.0 - frac) * beat;
        const double firstBeatPpq = std::ceil (take.hostStartPpq - 1e-6);
        const int bpb = juce::jmax (1, take.beatsPerBar);
        const bool dense = beat / secPerPx < 8.0;
        for (int k = -1;; ++k)
        {
            const double t = firstBeatT + k * beat;
            if (t > take.lengthSeconds()) break;
            const float x = timeToX (t);
            if (x < laneX() || x > getWidth()) continue;
            const long beatIdx = (long) std::llround (firstBeatPpq) + k;
            const bool barLine = beatIdx % bpb == 0;
            if (! barLine && dense) continue;
            g.setColour (barLine ? border.brighter (0.15f) : border.withAlpha (0.45f));
            g.drawVerticalLine ((int) x, (float) vocalArea().getY(), (float) chordArea().getBottom());
            if (barLine)
            {
                g.setColour (muted);
                g.setFont (ui (10.0f, Weight::Bold));
                g.drawText (juce::String (beatIdx / bpb + 1), (int) x + 4, ruler.getY(), 40, ruler.getHeight(), juce::Justification::centredLeft);
            }
        }
    }

    // ---- vocal wave: muted outside parts, part colour + glow inside ----
    const auto va = vocalArea();
    const auto wp = wavePath (va.reduced (0, 6), take.vocalPeaks);
    g.saveState();
    g.reduceClipRegion (va);
    g.setColour (wave.withAlpha (0.55f));
    g.fillPath (wp);
    for (const auto& p : proc.getParts())
    {
        const float x0 = timeToX (p.start), x1 = timeToX (p.end);
        auto r = juce::Rectangle<float> (x0, (float) va.getY(), x1 - x0, (float) va.getHeight());
        const auto c = partColour (p.colourIndex);
        const bool sel = p.id == proc.selectedPart;
        g.setColour (c.withAlpha (sel ? 0.12f : 0.07f));
        g.fillRect (r);
        g.saveState();
        g.reduceClipRegion (r.toNearestInt());
        glowPath (g, wp, c, 12, sel ? 0.5f : 0.3f);
        juce::ColourGradient wg (c.withAlpha (0.35f), 0, (float) va.getY(), c.withAlpha (0.35f), 0, (float) va.getBottom(), false);
        wg.addColour (0.5, c.withAlpha (0.85f));
        g.setGradientFill (wg);
        g.fillPath (wp);
        g.setColour (c.brighter (0.4f));
        g.strokePath (wp, juce::PathStrokeType (1.0f));
        g.restoreState();
        // header strip + handles
        g.setColour (c);
        g.fillRect (r.getX(), r.getY(), r.getWidth(), 2.0f);
        g.setColour (c.withAlpha (sel ? 1.0f : 0.6f));
        g.drawRect (r, sel ? 1.5f : 1.0f);
        g.fillRoundedRectangle (r.getX() - 1.0f, r.getCentreY() - 12.0f, 3.0f, 24.0f, 1.5f);
        g.fillRoundedRectangle (r.getRight() - 2.0f, r.getCentreY() - 12.0f, 3.0f, 24.0f, 1.5f);

        auto tag = juce::Rectangle<float> (r.getX() + 6.0f, r.getY() + 7.0f, 20.0f, 18.0f);
        g.setColour (c);
        g.fillRoundedRectangle (tag, 4.0f);
        g.setColour (bgDeep);
        g.setFont (ui (11.0f, Weight::ExtraBold));
        g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + (p.id - 1) % 26)), tag, juce::Justification::centred);

        bool stacked = false;
        for (const auto& s : proc.getStacks()) stacked |= s->partId == p.id && s->state.load() == StackEntry::Done;
        g.setColour (stacked ? c : muted);
        g.setFont (label (8.5f));
        g.drawText (stacked ? "STACKED" : "NEW", (int) tag.getRight() + 6, (int) tag.getY(), 70, (int) tag.getHeight(), juce::Justification::centredLeft);

        const float cx = r.getRight() - 14.0f, cy = r.getY() + 16.0f;   // remove
        g.setColour (text.withAlpha (0.7f));
        g.drawLine (cx - 3.5f, cy - 3.5f, cx + 3.5f, cy + 3.5f, 1.4f);
        g.drawLine (cx - 3.5f, cy + 3.5f, cx + 3.5f, cy - 3.5f, 1.4f);
    }
    g.restoreState();

    // ---- beat ----
    const auto ba = beatArea();
    if (take.hasSidechain)
    {
        g.setColour (faint.withAlpha (0.8f));
        g.fillPath (wavePath (ba.reduced (0, 4), take.beatPeaks));
    }
    else
    {
        g.setColour (faint);
        g.setFont (ui (11.0f, Weight::Medium));
        g.drawText ("no side chain recorded: harmonies follow the key", ba, juce::Justification::centred);
    }

    // ---- chords ----
    const auto ca = chordArea();
    if (proc.isAnalysing())
    {
        g.setColour (muted);
        g.setFont (ui (11.0f, Weight::Medium));
        g.drawText ("reading chords, key and tempo...", ca, juce::Justification::centred);
    }
    g.saveState();
    g.reduceClipRegion (ca);
    for (const auto& c : take.chords)
    {
        const float x0 = timeToX (c.start), x1 = timeToX (c.end);
        if (x1 < ca.getX() || x0 > ca.getRight()) continue;
        auto r = juce::Rectangle<float> (x0 + 1.5f, (float) ca.getY() + 3, x1 - x0 - 3.0f, (float) ca.getHeight() - 6);
        const bool nc = c.chord.isNoChord();
        g.setColour (nc ? surface : surfaceHi);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (nc ? border : primary.withAlpha (0.45f));
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        g.setColour (nc ? faint : text);
        g.setFont (ui (12.5f, Weight::ExtraBold));
        g.drawText (c.chord.name(), r.reduced (8, 0), juce::Justification::centredLeft, true);
    }
    g.restoreState();
}

void TimelineView::mouseMove (const juce::MouseEvent& e)
{
    auto cur = juce::MouseCursor::NormalCursor;
    if (vocalArea().contains (e.getPosition()))
    {
        cur = juce::MouseCursor::IBeamCursor;
        for (const auto& p : proc.getParts())
        {
            const float x0 = timeToX (p.start), x1 = timeToX (p.end);
            if (std::abs (e.x - x0) < 6 || std::abs (e.x - x1) < 6) cur = juce::MouseCursor::LeftRightResizeCursor;
            else if (e.x > x0 && e.x < x1) cur = juce::MouseCursor::PointingHandCursor;
        }
    }
    else if (chordArea().contains (e.getPosition()))
        cur = juce::MouseCursor::PointingHandCursor;
    setMouseCursor (cur);
}

void TimelineView::mouseDown (const juce::MouseEvent& e)
{
    const auto& take = proc.getTake();
    if (take.isEmpty()) return;
    const double t = xToTime ((float) e.x);
    drag = Drag::None;

    if (e.mods.isMiddleButtonDown() || rulerArea().contains (e.getPosition()))
    {
        drag = Drag::Pan; panStartView = viewStart; return;
    }

    if (chordArea().contains (e.getPosition()))
    {
        for (size_t i = 0; i < take.chords.size(); ++i)
            if (t >= take.chords[i].start && t < take.chords[i].end) { chordMenu ((int) i, t); break; }
        return;
    }

    if (! vocalArea().contains (e.getPosition())) return;

    for (const auto& p : proc.getParts())
    {
        const float x0 = timeToX (p.start), x1 = timeToX (p.end);
        if (e.x > x1 - 24 && e.x < x1 - 4 && e.y < vocalArea().getY() + 28) { proc.removePart (p.id); repaint(); return; }
        if (std::abs (e.x - x0) < 6) { drag = Drag::ResizeStart; dragPart = p.id; return; }
        if (std::abs (e.x - x1) < 6) { drag = Drag::ResizeEnd; dragPart = p.id; return; }
        if (e.x > x0 && e.x < x1) { proc.selectedPart = p.id; proc.sendChangeMessage(); invalidate(); return; }
    }
    drag = Drag::Create;
    dragAnchor = dragNow = snap (t);
}

void TimelineView::mouseDrag (const juce::MouseEvent& e)
{
    if (drag != Drag::Create) dirty = true;
    const double t = xToTime ((float) e.x);
    switch (drag)
    {
        case Drag::Create: dragNow = snap (t); break;
        case Drag::Pan:
            viewStart = panStartView - e.getDistanceFromDragStartX() * secPerPx;
            clampView();
            break;
        case Drag::ResizeStart:
        case Drag::ResizeEnd:
            for (const auto& p : proc.getParts())
                if (p.id == dragPart)
                {
                    if (drag == Drag::ResizeStart) proc.setPartBounds (p.id, snap (t), p.end);
                    else proc.setPartBounds (p.id, p.start, snap (t));
                    break;
                }
            break;
        case Drag::None:
        default: break;
    }
    repaint();
}

void TimelineView::mouseUp (const juce::MouseEvent&)
{
    dirty = true;
    if (drag == Drag::Create)
        proc.addPart (std::min (dragAnchor, dragNow), std::max (dragAnchor, dragNow));
    drag = Drag::None;
    dragPart = -1;
    repaint();
}

void TimelineView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    dirty = true;
    if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        zoom (w.deltaY > 0 ? 1.25 : 0.8, xToTime ((float) e.x));
    else
    {
        const float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
        viewStart -= d * 200.0 * secPerPx;
        clampView();
        repaint();
    }
}

void TimelineView::chordMenu (int seg, double at)
{
    juce::PopupMenu m;
    static const Quality qs[] = { Quality::Maj, Quality::Min, Quality::Dom7, Quality::Min7, Quality::Maj7, Quality::Dim, Quality::Sus4 };
    for (int root = 0; root < 12; ++root)
    {
        juce::PopupMenu sub;
        for (int q = 0; q < 7; ++q)
            sub.addItem (1 + root * 10 + q, Chord { root, qs[q] }.name());
        m.addSubMenu (pitchClassName (root), sub);
    }
    m.addSeparator();
    m.addItem (500, "N.C. (no chord)");
    m.addItem (501, "Split here");
    m.addItem (502, "Merge with next");

    juce::Component::SafePointer<TimelineView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                     [safe, seg, at] (int r)
                     {
                         if (safe == nullptr || r == 0) return;
                         auto& p = safe->proc;
                         if (r == 500) p.setChord (seg, Chord {});
                         else if (r == 501) p.splitChord (seg, safe->snap (at));
                         else if (r == 502) p.mergeChordWithNext (seg);
                         else { int root = (r - 1) / 10, q = (r - 1) % 10; p.setChord (seg, Chord { root, qs[q] }); }
                         safe->repaint();
                     });
}

} // namespace subharm
