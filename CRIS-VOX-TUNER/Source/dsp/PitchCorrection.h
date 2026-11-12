#pragma once
#include "Types.h"
namespace cris
{
class PitchCorrection
{
public:
    static bool allowed(int midi, int key, int scale);
    static double nearest(double midi, int key, int scale);
    double update(const PitchFrame& frame, const Parameters& p, double dt, double manualMidi = -1.0);
    void reset() { shift = 0.0; target = -1.0; }
private:
    double shift = 0.0, target = -1.0;
};
}
