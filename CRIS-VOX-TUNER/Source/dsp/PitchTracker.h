#pragma once
#include "Types.h"
#include <array>
namespace cris
{
class PitchTracker
{
public:
    PitchFrame update(PitchFrame frame, double dt = 0.01);
    void reset();
    static std::vector<Note> segment(const std::vector<PitchFrame>& frames, double hop);
private:
    std::array<double, 5> history {};
    int count = 0, index = 0;
    double centre = 0.0, duration = 0.0, previous = 0.0, vibratoEnergy = 0.0;
};
}
