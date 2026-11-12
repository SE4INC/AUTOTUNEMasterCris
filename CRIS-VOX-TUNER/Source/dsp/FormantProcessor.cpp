#include "FormantProcessor.h"
namespace cris
{
void FormantProcessor::prepare(int channels, double sampleRate, bool offline)
{
    rate = sampleRate;
    // More spectral resolution for offline bass voices; split work in LIVE.
    const int block = static_cast<int>(rate * (offline ? 0.12 : 0.08));
    const int interval = static_cast<int>(rate * (offline ? 0.03 : 0.02));
    stretch.configure(channels, block, interval, !offline);
}
void FormantProcessor::setPitch(double semitones, double preserve, double f0)
{
    const auto ratio = static_cast<float>(std::exp2(semitones / 12.0));
    stretch.setTransposeFactor(ratio);
    stretch.setFormantFactor(static_cast<float>(std::pow(ratio, 1.0 - std::clamp(preserve, 0.0, 1.0))), true);
    stretch.setFormantBase(static_cast<float>((f0 > 0.0 ? f0 : 130.0) / rate));
}
void FormantProcessor::process(const float* const* in, float* const* out, int count)
{
    stretch.process(in, count, out, count);
}
}
