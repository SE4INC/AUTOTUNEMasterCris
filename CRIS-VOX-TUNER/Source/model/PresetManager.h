#pragma once
#include "dsp/Types.h"
#include <array>
namespace cris
{
struct Preset { const char* name; Parameters parameters; };
class PresetManager
{
public:
    static const std::array<Preset, 9>& all()
    {
        static const std::array<Preset, 9> presets {{
            {"NATURAL MALE VOCAL", {0.80f, 35.0f, 0.60f, 12.0f, 1.0f, 0.65f, 0, 0}},
            {"DEEP VOICE", {0.75f, 50.0f, 0.75f, 15.0f, 1.0f, 0.70f, 0, 0}},
            {"SOFT MELODIC", {0.65f, 65.0f, 0.85f, 18.0f, 1.0f, 0.50f, 0, 0}},
            {"NATURAL RAP", {0.55f, 28.0f, 0.70f, 20.0f, 1.0f, 0.40f, 0, 0}},
            {"TRAP MELODIC", {0.95f, 12.0f, 0.20f, 3.0f, 1.0f, 0.90f, 0, 0}},
            {"STRONG CORRECTION", {1.00f, 1.0f, 0.00f, 0.0f, 1.0f, 1.00f, 0, 0}},
            {"DUKI - URBAN INSPIRED", {1.00f, 7.0f, 0.10f, 1.0f, 1.0f, 0.95f, 0, 0}},
            {"ANUEL AA - DEEP INSPIRED", {0.95f, 15.0f, 0.25f, 4.0f, 1.0f, 0.90f, 0, 0}},
            {"BAD BUNNY - MELODIC INSPIRED", {0.90f, 22.0f, 0.40f, 6.0f, 1.0f, 0.80f, 0, 0}}
        }};
        return presets;
    }
};
}
