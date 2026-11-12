#pragma once
#include "Types.h"
#include <signalsmith-stretch/signalsmith-stretch.h>
namespace cris
{
class FormantProcessor
{
public:
    void prepare(int channels, double sampleRate, bool offline);
    void setPitch(double semitones, double preserve, double f0);
    void reset() { stretch.reset(); }
    void process(const float* const* input, float* const* output, int count);
    int latency() const { return stretch.inputLatency() + stretch.outputLatency(); }
    int inputLatency() const { return stretch.inputLatency(); }
private:
    signalsmith::stretch::SignalsmithStretch<float> stretch {42};
    double rate = 48000.0;
};
}
