#pragma once
#include "model/ClipService.h"
#include <juce_gui_basics/juce_gui_basics.h>
class PitchEditor final : public juce::Component
{
public:
    explicit PitchEditor(cris::ClipService& service);
    void setSnapshot(std::shared_ptr<const cris::ClipSnapshot> value);
    void setPlayhead(double seconds);
    void zoomHorizontal(double factor);
    void zoomVertical(double factor);
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;
private:
    juce::Rectangle<float> grid() const;
    juce::Rectangle<float> noteBounds(const cris::Note& n) const;
    double xTime(float x) const;
    float timeX(double t) const;
    float midiY(double midi) const;
    double yMidi(float y) const;
    cris::ClipService& clips;
    std::shared_ptr<const cris::ClipSnapshot> snapshot;
    double viewStart = 0.0, viewDuration = 8.0, bottomMidi = 34.0, rangeMidi = 24.0, playhead = -1.0;
    int selected = -1;
    double dragMidi = 0.0, dragStartMidi = 0.0;
    float dragStartY = 0.0f;
};
