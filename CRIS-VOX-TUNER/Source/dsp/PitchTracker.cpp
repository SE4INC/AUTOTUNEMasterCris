#include "PitchTracker.h"
namespace cris
{
void PitchTracker::reset()
{
    history.fill(0.0);
    count = index = 0;
    centre = duration = previous = vibratoEnergy = 0.0;
}
PitchFrame PitchTracker::update(PitchFrame frame, double dt)
{
    if (!frame.voiced) { reset(); return frame; }
    history[static_cast<std::size_t>(index)] = frame.midi;
    index = (index + 1) % 5;
    count = std::min(5, count + 1);
    auto sorted = history;
    for (int a = 1; a < count; ++a)
        for (int b = a; b > 0 && sorted[static_cast<std::size_t>(b)] < sorted[static_cast<std::size_t>(b - 1)]; --b)
            std::swap(sorted[static_cast<std::size_t>(b)], sorted[static_cast<std::size_t>(b - 1)]);
    const double median = sorted[static_cast<std::size_t>(count / 2)];
    // Only reject large isolated errors: do not flatten the actual vibrato curve.
    if (count == 5 && std::abs(frame.midi - median) > 5.0) frame.midi = median;
    frame.hz = hzFromMidi(frame.midi);
    if (count == 1 || std::abs(frame.midi - previous) > 0.8)
    {
        duration = 0.0;
        centre = frame.midi;
    }
    else duration += dt;
    const double alpha = 1.0 - std::exp(-dt / 0.12);
    centre += alpha * (frame.midi - centre);
    const double residual = frame.midi - centre;
    vibratoEnergy += (1.0 - std::exp(-dt / 0.25)) * (residual * residual - vibratoEnergy);
    previous = frame.midi;
    frame.centre = centre;
    frame.sustained = duration > 0.18;
    frame.vibratoCents = std::sqrt(std::max(0.0, vibratoEnergy)) * 100.0;
    return frame;
}
std::vector<Note> PitchTracker::segment(const std::vector<PitchFrame>& frames, double hop)
{
    std::vector<Note> notes;
    std::vector<double> pitches;
    double start = 0.0, last = 0.0;
    int quantized = -1;
    auto finish = [&]
    {
        if (!pitches.empty() && last + hop - start >= 0.05)
        {
            std::sort(pitches.begin(), pitches.end());
            notes.push_back({static_cast<int>(notes.size()), start, last + hop,
                             pitches[pitches.size() / 2], -1.0});
        }
        pitches.clear();
        quantized = -1;
    };
    for (const auto& f : frames)
    {
        if (!f.voiced) { finish(); continue; }
        const int q = static_cast<int>(std::lround(f.centre));
        if (quantized >= 0 && (q != quantized && std::abs(f.centre - quantized) > 0.65)) finish();
        if (pitches.empty()) { start = std::max(0.0, f.time - hop * 0.5); quantized = q; }
        last = f.time;
        pitches.push_back(f.midi);
    }
    finish();
    return notes;
}
}
