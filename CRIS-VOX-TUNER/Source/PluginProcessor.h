#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "dsp/VocalEngine.h"
#include "model/ClipService.h"
#include "model/PresetManager.h"

class CrisVoxTunerProcessor final : public juce::AudioProcessor,
                                    private juce::AudioProcessorValueTreeState::Listener
{
public:
    CrisVoxTunerProcessor();
    ~CrisVoxTunerProcessor() override;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "CRIS VOX TUNER"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return getSampleRate() > 0.0 ? getLatencySamples() / getSampleRate() : 0.0; }
    int getNumPrograms() override { return static_cast<int>(cris::PresetManager::all().size()); }
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int size) override;
    cris::Parameters parameters() const;
    void setParameter(const juce::String& id, float value);
    void exportClip(const juce::File& file);
    juce::AudioProcessorValueTreeState state;
    cris::ClipService clips;
    std::atomic<bool> preview {false}, original {false};
    std::atomic<double> previewSeconds {0.0};
    std::atomic<float> inputLevel {0.0f}, outputLevel {0.0f}, detectedHz {0.0f}, correctionCents {0.0f};
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void parameterChanged(const juce::String& id, float value) override;
    void process(juce::AudioBuffer<float>& buffer, bool hostBypass);
    float value(const char* id) const;
    cris::LiveEngine live;
    std::atomic<int> currentProgram {0};
    std::array<std::atomic<float>*, 16> raw {};
    double clipCursor = 0.0, clipRate = 48000.0;
    float clipMix = 1.0f, clipGain = 1.0f;
    bool wasPreview = false;
    int activeMode = -1;
    std::uint64_t seenRevision = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrisVoxTunerProcessor)
};
