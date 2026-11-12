#pragma once
#include "PluginProcessor.h"
#include "ui/PitchEditor.h"
#include <array>

class CrisLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    CrisLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float position,
                          float startAngle, float endAngle, juce::Slider&) override;
};
class CrisVoxTunerEditor final : public juce::AudioProcessorEditor, private juce::Timer,
                                 public juce::FileDragAndDropTarget
{
public:
    explicit CrisVoxTunerEditor(CrisVoxTunerProcessor& p);
    ~CrisVoxTunerEditor() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int, int) override;
private:
    void timerCallback() override;
    void importAudio();
    void exportAudio();
    CrisVoxTunerProcessor& processor;
    CrisLookAndFeel look;
    PitchEditor piano;
    juce::ComboBox presets, key, scale, mode;
    std::array<juce::Slider, 8> knobs;
    std::array<juce::Label, 8> labels;
    juce::Slider start;
    juce::Label startLabel, status, pitchLabel;
    juce::TextButton importButton {"IMPORT WAV"}, exportButton {"EXPORT WAV"}, playButton {"PLAY"},
        originalButton {"ORIGINAL A/B"}, resetButton {"RESET NOTES"}, zoomIn {"TIME +"}, zoomOut {"TIME -"},
        verticalIn {"NOTES +"}, verticalOut {"NOTES -"};
    juce::ToggleButton sync {"HOST SYNC"}, bypass {"BYPASS"};
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> attachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> startAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAttachment, scaleAttachment, modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment, bypassAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    std::uint64_t revision = 0;
    int shownPreset = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrisVoxTunerEditor)
};
