#include "PitchCorrection.h"
#include <array>
namespace cris
{
bool PitchCorrection::allowed(int midi, int key, int scale)
{
    const int pitch = ((midi - key) % 12 + 12) % 12;
    constexpr std::array<int, 12> major {1,0,1,0,1,1,0,1,0,1,0,1};
    constexpr std::array<int, 12> minor {1,0,1,1,0,1,0,1,1,0,1,0};
    constexpr std::array<int, 12> pentatonic {1,0,0,1,0,1,0,1,0,0,1,0};
    if (scale == 1) return major[static_cast<std::size_t>(pitch)] != 0;
    if (scale == 2) return minor[static_cast<std::size_t>(pitch)] != 0;
    if (scale == 3) return pentatonic[static_cast<std::size_t>(pitch)] != 0;
    return true;
}
double PitchCorrection::nearest(double midi, int key, int scale)
{
    double best = std::round(midi), distance = 1000.0;
    const int centre = static_cast<int>(std::round(midi));
    for (int n = centre - 12; n <= centre + 12; ++n)
        if (allowed(n, key, scale) && std::abs(n - midi) < distance)
        { best = n; distance = std::abs(n - midi); }
    return best;
}
double PitchCorrection::update(const PitchFrame& f, const Parameters& p, double dt, double manualMidi)
{
    double desired = 0.0;
    if (f.voiced)
    {
        const double preserve = std::clamp(static_cast<double>(p.humanize), 0.0, 1.0);
        const double measured = f.midi * (1.0 - preserve) + f.centre * preserve;
        const double candidate = manualMidi >= 0.0 ? manualMidi : nearest(measured, p.key, p.scale);
        if (manualMidi >= 0.0 || target < 0.0 || !allowed(static_cast<int>(target), p.key, p.scale)
            || std::abs(measured - candidate) + 0.15 < std::abs(measured - target)) target = candidate;
        const double error = target - measured;
        const double tolerance = std::max(0.0, static_cast<double>(p.flexCents)) / 100.0;
        const double edge = std::clamp((std::abs(error) - tolerance) / 0.10, 0.0, 1.0);
        const double sustainedStrength = f.sustained ? (0.5 + 0.5 * p.drift) : 1.0;
        desired = std::clamp(error, -12.0, 12.0) * p.correction * edge * sustainedStrength;
    }
    else target = -1.0;
    const double tau = std::max(0.001, p.speedMs * 0.001);
    shift += (1.0 - std::exp(-dt / tau)) * (desired - shift);
    return shift;
}
}
