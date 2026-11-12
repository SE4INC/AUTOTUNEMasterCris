#include "PluginEditor.h"
namespace
{
const juce::Colour gold(0xffd9b76d), text(0xffeeeae0), muted(0xff969ba6), panel(0xff191b20);
constexpr std::array<const char*, 8> controlIds {"correction", "speed", "humanize", "flex", "mix", "output", "formant", "drift"};
constexpr std::array<const char*, 8> controlNames {"CORRECTION", "SPEED", "HUMANIZE", "FLEX TUNE", "MIX", "OUTPUT", "FORMANT", "PITCH DRIFT"};
}
CrisLookAndFeel::CrisLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff101115));
    setColour(juce::ComboBox::backgroundColourId, panel);
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff45464c));
    setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff665332));
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff292b30));
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff8b713d));
    setColour(juce::TextButton::textColourOffId, text);
    setColour(juce::Slider::textBoxTextColourId, text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ToggleButton::textColourId, text);
    setColour(juce::ToggleButton::tickColourId, gold);
}
void CrisLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                      float position, float startAngle, float endAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)).reduced(8.0f);
    const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    juce::Path background, active;
    background.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    active.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, startAngle + position * (endAngle - startAngle), true);
    g.setColour(juce::Colour(0xff35373d)); g.strokePath(background, juce::PathStrokeType(4.0f));
    g.setColour(gold); g.strokePath(active, juce::PathStrokeType(4.0f));
    const auto face = juce::Rectangle<float>(centre.x - radius + 8.0f, centre.y - radius + 8.0f, (radius - 8.0f) * 2.0f, (radius - 8.0f) * 2.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff77705e), face.getTopLeft(), juce::Colour(0xff252526), face.getBottomRight(), false));
    g.fillEllipse(face);
    g.setColour(juce::Colour(0xffb6a27b).withAlpha(0.5f)); g.drawEllipse(face, 1.0f);
    const float angle = startAngle + position * (endAngle - startAngle);
    g.setColour(text);
    g.drawLine(centre.x + std::sin(angle) * radius * 0.45f, centre.y - std::cos(angle) * radius * 0.45f,
               centre.x + std::sin(angle) * radius * 0.76f, centre.y - std::cos(angle) * radius * 0.76f, 2.5f);
}
CrisVoxTunerEditor::CrisVoxTunerEditor(CrisVoxTunerProcessor& p)
    : AudioProcessorEditor(p), processor(p), piano(p.clips)
{
    setLookAndFeel(&look);
    setResizable(true, true); setResizeLimits(960, 740, 1600, 1100); setSize(1180, 820);
    addAndMakeVisible(piano);
    for (auto* c : {&presets, &key, &scale, &mode}) addAndMakeVisible(c);
    for (int i = 0; i < p.getNumPrograms(); ++i) presets.addItem(p.getProgramName(i), i + 1);
    presets.onChange = [this] { processor.setCurrentProgram(presets.getSelectedId() - 1); };
    key.addItemList({"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 1);
    scale.addItemList({"CHROMATIC", "MAJOR", "NATURAL MINOR", "MINOR PENTATONIC"}, 1);
    mode.addItemList({"LIVE INPUT", "CLIP EDITOR"}, 1);
    keyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state, "key", key);
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state, "scale", scale);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state, "mode", mode);
    for (std::size_t i = 0; i < knobs.size(); ++i)
    {
        auto& knob = knobs[i];
        knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 24);
        knob.setNumDecimalPlacesToDisplay(i == 5 ? 1 : 0);
        knob.setTextValueSuffix(i == 1 ? " ms" : (i == 3 ? " ct" : (i == 5 ? " dB" : "%")));
        knob.setTooltip(controlNames[i]);
        addAndMakeVisible(knob);
        labels[i].setText(controlNames[i], juce::dontSendNotification);
        labels[i].setFont(juce::FontOptions(12.0f)); labels[i].setColour(juce::Label::textColourId, text);
        labels[i].setJustificationType(juce::Justification::centred); addAndMakeVisible(labels[i]);
        attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state, controlIds[i], knob);
    }
    for (auto* button : {&importButton, &exportButton, &playButton, &originalButton, &resetButton, &zoomIn, &zoomOut, &verticalIn, &verticalOut}) addAndMakeVisible(button);
    importButton.onClick = [this] { importAudio(); };
    exportButton.onClick = [this] { exportAudio(); };
    playButton.onClick = [this] { processor.preview.store(!processor.preview.load()); };
    originalButton.setClickingTogglesState(true);
    originalButton.onClick = [this] { processor.original.store(originalButton.getToggleState()); };
    resetButton.onClick = [this] { processor.clips.resetEdits(); };
    zoomIn.onClick = [this] { piano.zoomHorizontal(1.5); };
    zoomOut.onClick = [this] { piano.zoomHorizontal(1.0 / 1.5); };
    verticalIn.onClick = [this] { piano.zoomVertical(1.25); };
    verticalOut.onClick = [this] { piano.zoomVertical(0.8); };
    addAndMakeVisible(sync); addAndMakeVisible(bypass);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state, "sync", sync);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state, "bypass", bypass);
    start.setSliderStyle(juce::Slider::LinearHorizontal); start.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 24);
    start.setTextValueSuffix(" s"); addAndMakeVisible(start);
    startAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state, "start", start);
    startLabel.setText("CLIP START", juce::dontSendNotification); startLabel.setFont(juce::FontOptions(11.0f));
    startLabel.setColour(juce::Label::textColourId, muted); addAndMakeVisible(startLabel);
    for (auto* label : {&status, &pitchLabel}) { label->setColour(juce::Label::textColourId, muted); label->setFont(juce::FontOptions(12.0f)); addAndMakeVisible(label); }
    originalButton.setToggleState(processor.original.load(), juce::dontSendNotification);
    timerCallback();
    startTimerHz(20);
}
CrisVoxTunerEditor::~CrisVoxTunerEditor() { stopTimer(); setLookAndFeel(nullptr); }
void CrisVoxTunerEditor::resized()
{
    const int w = getWidth(), h = getHeight();
    presets.setBounds(w - 420, 22, 300, 34); bypass.setBounds(w - 110, 24, 100, 30);
    mode.setBounds(22, 85, 175, 32); key.setBounds(250, 85, 90, 32); scale.setBounds(410, 85, 205, 32);
    pitchLabel.setBounds(640, 85, w - 660, 32);
    const int editorHeight = h - 480;
    piano.setBounds(20, 143, w - 40, editorHeight);
    const int controlsY = 143 + editorHeight + 12;
    importButton.setBounds(22, controlsY, 118, 30); playButton.setBounds(148, controlsY, 74, 30);
    originalButton.setBounds(230, controlsY, 128, 30); exportButton.setBounds(366, controlsY, 118, 30);
    resetButton.setBounds(492, controlsY, 115, 30);
    zoomOut.setBounds(w - 350, controlsY, 76, 30); zoomIn.setBounds(w - 268, controlsY, 76, 30);
    verticalOut.setBounds(w - 186, controlsY, 78, 30); verticalIn.setBounds(w - 102, controlsY, 78, 30);
    const int knobY = controlsY + 53, knobWidth = (w - 80) / 8;
    for (int i = 0; i < 8; ++i)
    {
        const int x = 24 + i * knobWidth;
        labels[static_cast<std::size_t>(i)].setBounds(x, knobY, knobWidth, 24);
        knobs[static_cast<std::size_t>(i)].setBounds(x + 2, knobY + 24, knobWidth - 4, 140);
    }
    sync.setBounds(24, h - 91, 125, 28); startLabel.setBounds(160, h - 90, 86, 28); start.setBounds(248, h - 91, 300, 28);
    status.setBounds(22, h - 49, w - 145, 30);
}
void CrisVoxTunerEditor::paint(juce::Graphics& g)
{
    const float w = static_cast<float>(getWidth()), h = static_cast<float>(getHeight());
    g.fillAll(juce::Colour(0xff101115));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff28262a), 0, 0, juce::Colour(0xff101115), w, 180, false));
    g.fillRect(0.0f, 0.0f, w, 74.0f);
    g.setColour(gold);
    for (int i = 0; i < 5; ++i)
    { const float height = 10.0f + (i == 2 ? 22.0f : (i == 1 || i == 3 ? 12.0f : 0.0f)); g.fillRoundedRectangle(24.0f + i * 7.0f, 36.0f - height * 0.5f, 3.0f, height, 1.5f); }
    g.setColour(text); g.setFont(juce::FontOptions(27.0f, juce::Font::bold));
    g.drawText("CRIS VOX TUNER", 72, 18, 345, 40, juce::Justification::centredLeft);
    g.setColour(muted); g.setFont(juce::FontOptions(12.0f)); g.drawText("1.0", 420, 32, 45, 20, juce::Justification::centredLeft);
    g.setColour(panel); g.fillRoundedRectangle(18.0f, 77.0f, w - 36.0f, 48.0f, 8.0f);
    g.setColour(muted); g.drawText("KEY", 210, 85, 35, 32, juce::Justification::centredLeft);
    g.drawText("SCALE", 358, 85, 50, 32, juce::Justification::centredLeft);
    g.setColour(gold); g.drawText("ORIGINAL / CORRECTED", 25, 123, 230, 19, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff34363e)); g.drawHorizontalLine(getHeight() - 107, 22, w - 22);
    const auto level = [&](float peak, float x)
    {
        const float db = juce::Decibels::gainToDecibels(peak, -60.0f);
        g.setColour(juce::Colour(0xff292c31)); g.fillRoundedRectangle(x, h - 90, 10, 57, 3);
        g.setColour(peak > 1.0f ? juce::Colours::orangered : gold);
        const float size = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f) * 57.0f;
        g.fillRoundedRectangle(x, h - 33 - size, 10, size, 3);
    };
    level(processor.inputLevel.load(), w - 70); level(processor.outputLevel.load(), w - 46);
}
void CrisVoxTunerEditor::timerCallback()
{
    if (shownPreset != processor.getCurrentProgram())
    { shownPreset = processor.getCurrentProgram(); presets.setSelectedId(shownPreset + 1, juce::dontSendNotification); }
    const auto clip = processor.clips.snapshot();
    if (clip && clip->revision != revision) { revision = clip->revision; piano.setSnapshot(clip); }
    else if (!clip && revision != 0) { revision = 0; piano.setSnapshot({}); }
    status.setText(processor.clips.status(), juce::dontSendNotification);
    const double hz = processor.detectedHz.load();
    pitchLabel.setText(hz > 0.0 ? juce::String(hz, 1) + " Hz    " + juce::String(processor.correctionCents.load(), 0) + " cents" : "MONOPHONIC VOCAL  /  A = 440 Hz", juce::dontSendNotification);
    const bool clipMode = processor.state.getRawParameterValue("mode")->load() > 0.5f;
    playButton.setEnabled(clip && clipMode && !sync.getToggleState());
    playButton.setButtonText(processor.preview.load() ? "STOP" : "PLAY");
    exportButton.setEnabled(clip && !processor.clips.busy());
    resetButton.setEnabled(clip && !processor.clips.busy());
    piano.setPlayhead(processor.preview.load() || sync.getToggleState() ? processor.previewSeconds.load() : -1.0);
    repaint(0, getHeight() - 110, getWidth(), 110);
}
void CrisVoxTunerEditor::importAudio()
{
    chooser = std::make_unique<juce::FileChooser>("Import a monophonic vocal (up to 180 seconds)", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<CrisVoxTunerEditor>(this)](const juce::FileChooser& c)
        {
            if (safe && c.getResult().existsAsFile())
            { safe->processor.preview.store(false); safe->processor.clips.importFile(c.getResult()); safe->processor.setParameter("mode", 1); }
        });
}
void CrisVoxTunerEditor::exportAudio()
{
    chooser = std::make_unique<juce::FileChooser>("Export corrected vocal", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("CRIS_VOX_corrected.wav"), "*.wav");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe = juce::Component::SafePointer<CrisVoxTunerEditor>(this)](const juce::FileChooser& c)
        { if (safe && c.getResult() != juce::File()) safe->processor.exportClip(c.getResult().withFileExtension("wav")); });
}
bool CrisVoxTunerEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    return files.size() == 1 && juce::File(files[0]).hasFileExtension("wav;aif;aiff;flac");
}
void CrisVoxTunerEditor::filesDropped(const juce::StringArray& files, int, int)
{ if (isInterestedInFileDrag(files)) { processor.preview.store(false); processor.clips.importFile(juce::File(files[0])); processor.setParameter("mode", 1); } }
