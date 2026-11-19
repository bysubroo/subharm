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
    const double len = juce::jmax (1.0, proc.getTake().lengthSeconds());
    secPerPx = len / juce::jmax (100.0f, getWidth() - laneX() - 4.0f);
    viewStart = 0.0;
    fitted = true;
    repaint();
}

void TimelineView::zoom (double factor, double anchor)
{
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

void TimelineView::drawWave (juce::Graphics& g, juce::Rectangle<int> r, const std::vector<float>& peaks, juce::Colour c)
{
    const auto& take = proc.getTake();
    if (peaks.empty()) return;
    const double peakSec = Take::kPeakBlock / take.sampleRate;
    const float mid = (float) r.getCentreY(), half = r.getHeight() * 0.46f;
    float maxPeak = 0.02f;
    for (float p : peaks) maxPeak = std::max (maxPeak, p);
    g.setColour (c);
    for (int x = r.getX(); x < r.getRight(); ++x)
    {
        const double t0 = xToTime ((float) x), t1 = xToTime ((float) x + 1.0f);
        if (t1 < 0 || t0 > take.lengthSeconds()) continue;
        int i0 = juce::jmax (0, (int) (t0 / peakSec)), i1 = juce::jmin ((int) peaks.size() - 1, (int) (t1 / peakSec));
        float m = 0;
        for (int i = i0; i <= i1; ++i) m = std::max (m, peaks[(size_t) i]);
        const float h = std::sqrt (m / maxPeak) * half;   // sqrt: quiet bits stay visible
        g.fillRect ((float) x, mid - h, 1.0f, juce::jmax (1.0f, 2 * h));
    }
}

void TimelineView::paint (juce::Graphics& g)
{
    using namespace theme;
    const auto& take = proc.getTake();
    g.fillAll (panel);

    auto label = [&] (juce::Rectangle<int> r, const char* t)
    {
        g.setColour (dim);
        g.setFont (font (10.0f, true));
        g.drawText (t, 6, r.getY(), (int) laneX() - 10, r.getHeight(), juce::Justification::centredLeft);
    };
    label (vocalArea(), "YOUR VOCAL");
    label (beatArea(), "BEAT");
    label (chordArea(), "CHORDS");

    for (auto r : { vocalArea(), beatArea(), chordArea() })
    {
        g.setColour (bg.brighter (0.03f));
        g.fillRoundedRectangle (r.toFloat(), 4.0f);
    }

    if (take.isEmpty())
    {
        g.setColour (dim);
        g.setFont (font (14.0f));
        juce::String msg;
        switch (proc.getRecState())
        {
            case SubHarmProcessor::RecState::Armed:     msg = "Armed: press play in your DAW. Stop the DAW when the part is done."; break;
            case SubHarmProcessor::RecState::Recording: msg = "Recording... " + juce::String (proc.getRecordedSeconds(), 1) + " s"; break;
            case SubHarmProcessor::RecState::Idle:
            case SubHarmProcessor::RecState::Finished:
            default: msg = "Set SubHarm's side chain to your beat (or a chords bus), hit RECORD THIS TRACK and press play.";
        }
        g.drawFittedText (msg, vocalArea().reduced (20), juce::Justification::centred, 2);
        return;
    }

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
            g.setColour (barLine ? line.brighter (0.3f) : line);
            g.drawVerticalLine ((int) x, (float) vocalArea().getY(), (float) chordArea().getBottom());
            if (barLine)
            {
                g.setColour (dim);
                g.setFont (font (10.0f));
                g.drawText (juce::String (beatIdx / bpb + 1), (int) x + 3, ruler.getY(), 40, ruler.getHeight(), juce::Justification::centredLeft);
            }
        }
    }

    // ---- waves ----
    drawWave (g, vocalArea().reduced (0, 6), take.vocalPeaks, wave);
    if (take.hasSidechain) drawWave (g, beatArea().reduced (0, 4), take.beatPeaks, faint);
    else
    {
        g.setColour (faint); g.setFont (font (11.0f));
        g.drawText ("no side chain recorded: harmonies follow the key", beatArea(), juce::Justification::centred);
    }

    // ---- parts ----
    const auto va = vocalArea();
    g.saveState();
    g.reduceClipRegion (va);
    for (const auto& p : proc.getParts())
    {
        const float x0 = timeToX (p.start), x1 = timeToX (p.end);
        auto r = juce::Rectangle<float> (x0, (float) va.getY(), x1 - x0, (float) va.getHeight());
        auto c = partColour (p.colourIndex);
        const bool sel = p.id == proc.selectedPart;
        g.setColour (c.withAlpha (sel ? 0.22f : 0.13f));
        g.fillRect (r);
        g.setColour (c.withAlpha (sel ? 1.0f : 0.7f));
        g.drawRect (r, sel ? 2.0f : 1.0f);
        g.fillRect (r.getX(), r.getY(), 3.0f, r.getHeight());
        g.fillRect (r.getRight() - 3.0f, r.getY(), 3.0f, r.getHeight());

        g.setFont (font (12.0f, true));
        g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + (p.id - 1) % 26)), (int) r.getX() + 7, (int) r.getY() + 3, 20, 16, juce::Justification::centredLeft);
        g.setColour (text.withAlpha (0.8f));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xc3\x97")), (int) r.getRight() - 20, (int) r.getY() + 2, 16, 16, juce::Justification::centred);

        bool stacked = false;
        for (const auto& s : proc.getStacks()) stacked |= s->partId == p.id && s->state.load() == StackEntry::Done;
        if (stacked)
        {
            g.setColour (c);
            g.fillEllipse (r.getX() + 24.0f, r.getY() + 7.0f, 7.0f, 7.0f);
        }
    }
    if (drag == Drag::Create)
    {
        const float x0 = timeToX (std::min (dragAnchor, dragNow)), x1 = timeToX (std::max (dragAnchor, dragNow));
        g.setColour (accent.withAlpha (0.18f));
        g.fillRect (x0, (float) va.getY(), x1 - x0, (float) va.getHeight());
    }
    g.restoreState();

    // ---- chords ----
    const auto ca = chordArea();
    if (proc.isAnalysing())
    {
        g.setColour (dim); g.setFont (font (11.0f));
        g.drawText ("reading chords, key and tempo...", ca, juce::Justification::centred);
    }
    g.saveState();
    g.reduceClipRegion (ca);
    for (size_t i = 0; i < take.chords.size(); ++i)
    {
        const auto& c = take.chords[i];
        const float x0 = timeToX (c.start), x1 = timeToX (c.end);
        if (x1 < ca.getX() || x0 > ca.getRight()) continue;
        auto r = juce::Rectangle<float> (x0 + 1, (float) ca.getY() + 2, x1 - x0 - 2, (float) ca.getHeight() - 4);
        g.setColour (c.chord.isNoChord() ? panelHi : panelHi.brighter (0.1f));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (c.chord.isNoChord() ? faint : text);
        g.setFont (font (12.0f, true));
        g.drawText (c.chord.name(), r.reduced (4, 0), juce::Justification::centredLeft, true);
    }
    g.restoreState();

    // ---- playhead ----
    const double ph = proc.getPlayheadInTake();
    if (proc.isHostPlaying() && ph >= 0 && ph <= take.lengthSeconds())
    {
        const float x = timeToX (ph);
        if (x >= laneX())
        {
            g.setColour (text);
            g.drawVerticalLine ((int) x, 0.0f, (float) getHeight());
        }
    }
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
        if (e.x > x1 - 22 && e.x < x1 - 4 && e.y < vocalArea().getY() + 20) { proc.removePart (p.id); repaint(); return; }
        if (std::abs (e.x - x0) < 6) { drag = Drag::ResizeStart; dragPart = p.id; return; }
        if (std::abs (e.x - x1) < 6) { drag = Drag::ResizeEnd; dragPart = p.id; return; }
        if (e.x > x0 && e.x < x1) { proc.selectedPart = p.id; proc.sendChangeMessage(); repaint(); return; }
    }
    drag = Drag::Create;
    dragAnchor = dragNow = snap (t);
}

void TimelineView::mouseDrag (const juce::MouseEvent& e)
{
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
    if (drag == Drag::Create)
        proc.addPart (std::min (dragAnchor, dragNow), std::max (dragAnchor, dragNow));
    drag = Drag::None;
    dragPart = -1;
    repaint();
}

void TimelineView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
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
