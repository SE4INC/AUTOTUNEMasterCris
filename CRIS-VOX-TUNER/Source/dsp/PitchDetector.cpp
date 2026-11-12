#include "PitchDetector.h"
#include <numeric>

namespace cris
{
void PitchDetector::prepare(double sampleRate)
{
    rate = std::max(8000.0, sampleRate);
    factor = std::max(1, static_cast<int>(rate / 12000.0));
    analysisRate = rate / factor;
    pole = std::exp(-2.0 * pi * 1800.0 / rate);
    hop = std::max(1, static_cast<int>(analysisRate * 0.01));
    reset();
}
void PitchDetector::reset()
{
    ring.fill(0.0f);
    decimation = write = filled = since = 0;
    inputCount = 0;
    lp1 = lp2 = 0.0;
}
bool PitchDetector::push(float sample, PitchFrame& result)
{
    ++inputCount;
    const double x = std::isfinite(sample) ? sample : 0.0;
    lp1 = (1.0 - pole) * x + pole * lp1;
    lp2 = (1.0 - pole) * lp1 + pole * lp2;
    if (++decimation < factor) return false;
    decimation = 0;
    ring[static_cast<std::size_t>(write)] = static_cast<float>(lp2);
    write = (write + 1) % 1024;
    filled = std::min(1024, filled + 1);
    if (++since < hop || filled < 1024) return false;
    since = 0;
    result = detect();
    result.time = std::max(0.0, inputCount / rate - frameDelaySeconds());
    return true;
}
PitchFrame PitchDetector::detect() const
{
    PitchFrame result;
    std::array<double, 1024> x {};
    double mean = 0.0;
    for (int i = 0; i < 1024; ++i)
    {
        x[static_cast<std::size_t>(i)] = ring[static_cast<std::size_t>((write + i) % 1024)];
        mean += x[static_cast<std::size_t>(i)] / 1024.0;
    }
    double energy = 0.0;
    for (auto& v : x) { v -= mean; energy += v * v; }
    result.rms = std::sqrt(energy / 1024.0);
    if (result.rms < 0.003) return result;
    const int low = std::max(2, static_cast<int>(analysisRate / 700.0));
    const int high = std::min(480, static_cast<int>(analysisRate / 55.0));
    std::array<double, 482> difference {};
    double cumulative = 0.0;
    difference[0] = 1.0;
    for (int tau = 1; tau <= high + 1; ++tau)
    {
        double sum = 0.0;
        for (int i = 0; i < 512; ++i)
        {
            const double d = x[static_cast<std::size_t>(i)] - x[static_cast<std::size_t>(i + tau)];
            sum += d * d;
        }
        cumulative += sum;
        difference[static_cast<std::size_t>(tau)] = cumulative > 1.0e-15 ? sum * tau / cumulative : 1.0;
    }
    int best = -1;
    for (int tau = low; tau < high; ++tau)
    {
        if (difference[static_cast<std::size_t>(tau)] < 0.15)
        {
            while (tau < high && difference[static_cast<std::size_t>(tau + 1)] < difference[static_cast<std::size_t>(tau)]) ++tau;
            best = tau;
            break;
        }
    }
    if (best < 0) return result;
    const double a = difference[static_cast<std::size_t>(best - 1)];
    const double b = difference[static_cast<std::size_t>(best)];
    const double c = difference[static_cast<std::size_t>(best + 1)];
    const double denominator = a - 2.0 * b + c;
    const double delta = std::abs(denominator) > 1.0e-12 ? std::clamp(0.5 * (a - c) / denominator, -0.5, 0.5) : 0.0;
    result.hz = analysisRate / (best + delta);
    result.confidence = std::clamp(1.0 - b, 0.0, 1.0);
    result.voiced = result.hz >= 55.0 && result.hz <= 700.0 && result.confidence >= 0.85;
    if (result.voiced) result.midi = midiFromHz(result.hz);
    return result;
}
}
