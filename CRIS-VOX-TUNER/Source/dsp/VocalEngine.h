#pragma once
#include "PitchDetector.h"
#include "PitchTracker.h"
#include "PitchCorrection.h"
#include "FormantProcessor.h"
#include <array>
namespace cris
{
Analysis analyse(const AudioData& input, const Cancel& cancel = {});
std::vector<double> correctionCurve(const Analysis& analysis, const Parameters& p);
AudioData render(const AudioData& input, const Analysis& analysis, const Parameters& p, const Cancel& cancel = {});

class LiveEngine
{
public:
    void prepare(int channels, double sampleRate);
    void reset();
    void process(float* const* data, int channels, int samples, const Parameters& p, float mix, float gain, bool bypass);
    int latency() const { return shifter.latency(); }
    PitchFrame lastFrame() const { return frame; }
    double lastShift() const { return shift; }
private:
    PitchDetector detector;
    PitchTracker tracker;
    PitchCorrection corrector;
    FormantProcessor shifter;
    PitchFrame frame;
    double rate = 48000.0, shift = 0.0;
    float wetMix = 1.0f, outputGain = 1.0f, voicedMix = 0.0f;
    int write = 0, delaySize = 0;
    std::vector<std::vector<float>> dryDelay;
    std::vector<float> voiceDelay;
    std::array<std::array<float, 128>, 2> input {}, output {}, dry {};
    std::array<float, 128> mask {};
};
}
