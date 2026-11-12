#pragma once
#include "Types.h"
#include <array>

namespace cris
{
// Streaming low-pass/decimation followed by YIN; no heap allocation.
class PitchDetector
{
public:
    void prepare(double sampleRate);
    void reset();
    bool push(float sample, PitchFrame& result);
    double frameDelaySeconds() const { return 512.0 / analysisRate; }
private:
    PitchFrame detect() const;
    std::array<float, 1024> ring {};
    double analysisRate = 12000.0, rate = 48000.0, pole = 0.0, lp1 = 0.0, lp2 = 0.0;
    int factor = 4, decimation = 0, write = 0, filled = 0, hop = 120, since = 0;
    std::uint64_t inputCount = 0;
};
}
