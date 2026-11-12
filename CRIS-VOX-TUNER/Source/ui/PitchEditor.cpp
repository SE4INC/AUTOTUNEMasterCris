#include "PitchEditor.h"
namespace { const juce::Colour gold(0xffd9b76d), panel(0xff15171b), muted(0xff92969e); }
PitchEditor::PitchEditor(cris::ClipService& s) : clips(s) { setWantsKeyboardFocus(true); }
void PitchEditor::setSnapshot(std::shared_ptr<const cris::ClipSnapshot> s)
{
    const bool newFile = s && (!snapshot || snapshot->path != s->path);
    snapshot = std::move(s);
    if (newFile)
    {
        viewStart = 0.0; viewDuration = std::min(12.0, std::max(2.0, snapshot->original->duration())); selected = -1;
        if (!snapshot->analysis.notes.empty())
        {
            double sum = 0.0; for (const auto& n : snapshot->analysis.notes) sum += n.originalMidi;
            bottomMidi = std::floor(sum / snapshot->analysis.notes.size() - rangeMidi * 0.5);
        }
    }
    repaint();
}
void PitchEditor::setPlayhead(double t) { playhead = t; repaint(); }
juce::Rectangle<float> PitchEditor::grid() const { return {62.0f, 26.0f, std::max(1.0f, getWidth() - 72.0f), std::max(1.0f, getHeight() - 38.0f)}; }
float PitchEditor::timeX(double t) const { const auto r = grid(); return r.getX() + static_cast<float>((t - viewStart) / viewDuration) * r.getWidth(); }
double PitchEditor::xTime(float x) const { const auto r = grid(); return viewStart + (x - r.getX()) / r.getWidth() * viewDuration; }
float PitchEditor::midiY(double m) const { const auto r = grid(); return r.getBottom() - static_cast<float>((m - bottomMidi) / rangeMidi) * r.getHeight(); }
double PitchEditor::yMidi(float y) const { const auto r = grid(); return bottomMidi + (r.getBottom() - y) / r.getHeight() * rangeMidi; }
juce::Rectangle<float> PitchEditor::noteBounds(const cris::Note& n) const
{
    const double midi = n.id == selected && isMouseButtonDown() ? dragMidi : (n.manualMidi >= 0.0 ? n.manualMidi : std::round(n.originalMidi));
    return {timeX(n.start), midiY(midi + 0.45), std::max(3.0f, timeX(n.end) - timeX(n.start)), static_cast<float>(grid().getHeight() / rangeMidi * 0.9)};
}
void PitchEditor::paint(juce::Graphics& g)
{
    g.fillAll(panel);
    const auto r = grid();
    g.setFont(juce::FontOptions(11.0f));
    constexpr const char* names[] {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    for (int m = static_cast<int>(bottomMidi); m <= bottomMidi + rangeMidi; ++m)
    {
        const int pc = (m % 12 + 12) % 12;
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        const float y = midiY(m + 0.5), height = static_cast<float>(r.getHeight() / rangeMidi);
        g.setColour(black ? juce::Colour(0xff111216) : juce::Colour(0xff1c1e23));
        g.fillRect(r.getX(), y, r.getWidth(), height);
        g.setColour(black ? juce::Colour(0xff25272d) : juce::Colour(0xffc0c0b9));
        g.fillRect(4.0f, y, black ? 37.0f : 53.0f, std::max(1.0f, height - 1.0f));
        if (pc == 0 || rangeMidi < 18.0)
        {
            g.setColour(black ? muted : juce::Colour(0xff282a30));
            g.drawText(juce::String(names[pc]) + juce::String(m / 12 - 1), 7, static_cast<int>(y), 44, static_cast<int>(height), juce::Justification::centredLeft);
        }
        g.setColour(juce::Colour(0xff2c2e34)); g.drawHorizontalLine(static_cast<int>(y), r.getX(), r.getRight());
    }
    const double tick = viewDuration <= 5.0 ? 0.5 : (viewDuration <= 20.0 ? 1.0 : 5.0);
    for (double t = std::ceil(viewStart / tick) * tick; t <= viewStart + viewDuration; t += tick)
    {
        const float x = timeX(t);
        g.setColour(juce::Colour(0xff30323a)); g.drawVerticalLine(static_cast<int>(x), r.getY(), r.getBottom());
        g.setColour(muted); g.drawText(juce::String(t, 1) + "s", static_cast<int>(x) + 3, 2, 52, 21, juce::Justification::centredLeft);
    }
    if (!snapshot)
    {
        g.setColour(gold); g.setFont(juce::FontOptions(20.0f));
        g.drawText("Import a vocal to edit its notes", r.toNearestInt(), juce::Justification::centred);
        g.setColour(muted); g.setFont(juce::FontOptions(13.0f));
        g.drawText("LIVE INPUT processes your mixer track without importing audio", r.toNearestInt().translated(0, 30), juce::Justification::centred);
        return;
    }
    juce::Graphics::ScopedSaveState saved(g);
    g.reduceClipRegion(r.toNearestInt());
    for (const auto& n : snapshot->analysis.notes)
    {
        auto bounds = noteBounds(n);
        g.setColour(gold.withAlpha(n.id == selected ? 0.45f : 0.18f)); g.fillRoundedRectangle(bounds, 3.0f);
        g.setColour(gold.withAlpha(n.id == selected ? 1.0f : 0.6f)); g.drawRoundedRectangle(bounds, 3.0f, n.id == selected ? 2.0f : 1.0f);
    }
    auto drawCurve = [&](bool corrected)
    {
        juce::Path path; bool started = false;
        for (std::size_t i = 0; i < snapshot->analysis.frames.size(); ++i)
        {
            const auto& f = snapshot->analysis.frames[i];
            if (!f.voiced || f.time < viewStart || f.time > viewStart + viewDuration) { started = false; continue; }
            const double midi = f.midi + (corrected && i < snapshot->curve.size() ? snapshot->curve[i] : 0.0);
            const auto x = timeX(f.time), y = midiY(midi);
            if (!started) { path.startNewSubPath(x, y); started = true; } else path.lineTo(x, y);
        }
        g.setColour(corrected ? gold : juce::Colour(0xffa3a6ad));
        g.strokePath(path, juce::PathStrokeType(corrected ? 1.8f : 1.0f));
    };
    drawCurve(false); drawCurve(true);
    if (playhead >= viewStart && playhead <= viewStart + viewDuration)
    { g.setColour(juce::Colours::white.withAlpha(0.8f)); g.drawVerticalLine(static_cast<int>(timeX(playhead)), r.getY(), r.getBottom()); }
}
void PitchEditor::zoomHorizontal(double factor)
{ viewDuration = std::clamp(viewDuration / factor, 0.5, 180.0); repaint(); }
void PitchEditor::zoomVertical(double factor)
{ const double centre = bottomMidi + rangeMidi * 0.5; rangeMidi = std::clamp(rangeMidi / factor, 8.0, 60.0); bottomMidi = centre - rangeMidi * 0.5; repaint(); }
void PitchEditor::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus(); selected = -1;
    if (snapshot) for (const auto& n : snapshot->analysis.notes)
        if (noteBounds(n).expanded(0.0f, 3.0f).contains(e.position))
        {
            selected = n.id; dragMidi = n.manualMidi >= 0.0 ? n.manualMidi : std::round(n.originalMidi);
            dragStartMidi = dragMidi; dragStartY = e.position.y; break;
        }
    repaint();
}
void PitchEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (selected < 0) return;
    const double raw = dragStartMidi + (dragStartY - e.position.y) * rangeMidi / grid().getHeight();
    dragMidi = std::clamp(e.mods.isShiftDown() ? std::round(raw * 10.0) / 10.0 : std::round(raw), 24.0, 96.0);
    repaint();
}
void PitchEditor::mouseUp(const juce::MouseEvent& e)
{ if (selected >= 0 && e.getDistanceFromDragStart() > 2) clips.moveNote(selected, dragMidi); }
void PitchEditor::mouseDoubleClick(const juce::MouseEvent&)
{
    if (!snapshot || selected < 0) return;
    // Service uses a negative target to restore automatic targeting.
    clips.moveNote(selected, -1.0);
}
void PitchEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCtrlDown()) zoomVertical(std::exp(w.deltaY));
    else if (e.mods.isShiftDown()) zoomHorizontal(std::exp(w.deltaY));
    else
    {
        viewStart = std::clamp(viewStart - (w.deltaY + w.deltaX) * viewDuration * 0.2, 0.0,
                              snapshot ? std::max(0.0, snapshot->original->duration() - 0.5) : 0.0);
        repaint();
    }
}
bool PitchEditor::keyPressed(const juce::KeyPress& key)
{
    if (selected < 0 || !snapshot) return false;
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) { clips.moveNote(selected, -1.0); return true; }
    if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
    {
        for (const auto& n : snapshot->analysis.notes) if (n.id == selected)
            clips.moveNote(selected, (n.manualMidi >= 0.0 ? n.manualMidi : std::round(n.originalMidi)) + (key == juce::KeyPress::upKey ? 1.0 : -1.0));
        return true;
    }
    return false;
}
