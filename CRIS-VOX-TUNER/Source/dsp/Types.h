#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace cris
{
constexpr double pi = 3.14159265358979323846;
inline double midiFromHz(double hz) { return 69.0 + 12.0 * std::log2(hz / 440.0); }
inline double hzFromMidi(double midi) { return 440.0 * std::exp2((midi - 69.0) / 12.0); }
inline double centsBetween(double a, double b) { return 1200.0 * std::log2(a / b); }
struct Parameters
{
    float correction = 0.8f, speedMs = 35.0f, humanize = 0.6f, flexCents = 12.0f;
    float formant = 1.0f, drift = 0.65f;
    int key = 0, scale = 0;
};
struct PitchFrame
{
    double time = 0.0, hz = 0.0, midi = 0.0, confidence = 0.0;
    double rms = 0.0, centre = 0.0, vibratoCents = 0.0;
    bool voiced = false, sustained = false;
};
struct Note
{
    int id = 0;
    double start = 0.0, end = 0.0, originalMidi = 0.0;
    double manualMidi = -1.0;
};
struct AudioData
{
    double sampleRate = 48000.0;
    std::vector<std::vector<float>> channels;
    std::size_t size() const { return channels.empty() ? 0 : channels.front().size(); }
    double duration() const { return static_cast<double>(size()) / sampleRate; }
};
struct Analysis
{
    std::vector<PitchFrame> frames;
    std::vector<Note> notes;
    double hopSeconds = 0.01;
};
using Cancel = std::function<bool()>;
inline bool cancelled(const Cancel& c) { return c && c(); }
}
