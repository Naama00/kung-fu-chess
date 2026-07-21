// ui/theme/Theme.hpp
#pragma once
#include "ui/framework/InputEvents.hpp"
#include "ui/framework/IRenderer.hpp" // for GradientStop
#include <vector>

namespace ui {
namespace theme {

// ------------------------------------------------------------------------
// Base palette: a deep, low-saturation dark background with a warm gold
// accent (a nod to classic chess sets/trophies) plus a cool indigo accent
// (modern web UI feel), so the app reads as both "chess" and "current".
// ------------------------------------------------------------------------
struct Palette {
    // Backgrounds
    static constexpr Color bgDeep      {13, 15, 20, 255};   // near-black navy, base canvas
    static constexpr Color bgSurface   {24, 27, 36, 255};   // panels / cards
    static constexpr Color bgSurfaceHi {32, 36, 48, 255};   // raised panels / default button fill

    // Accents
    static constexpr Color gold        {212, 175, 55, 255}; // primary accent - chess gold
    static constexpr Color goldBright  {240, 196, 64, 255};
    static constexpr Color indigo      {99, 102, 241, 255}; // secondary accent - modern indigo
    static constexpr Color indigoBright{129, 140, 248, 255};

    // Board-inspired neutrals (kept separate from UI chrome colors so the
    // board itself can stay visually distinct from menus/panels)
    static constexpr Color ivory       {245, 241, 232, 255};
    static constexpr Color walnut      {59, 42, 32, 255};

    // Text
    static constexpr Color textPrimary {245, 245, 247, 255};
    static constexpr Color textMuted   {156, 163, 175, 255};

    // Structure
    static constexpr Color border      {58, 62, 78, 255};

    // Status
    static constexpr Color success     {74, 222, 128, 255};
    static constexpr Color danger      {248, 113, 113, 255};
};

// ------------------------------------------------------------------------
// Layout tokens - consistent corner radii, spacing, and font sizes so
// components don't each invent their own magic numbers.
// ------------------------------------------------------------------------
struct Radius {
    static constexpr float sm   = 6.0f;
    static constexpr float md   = 12.0f;
    static constexpr float lg   = 20.0f;
    static constexpr float pill = 999.0f; // fully rounded (auto-clamped by the renderer)
};

struct Spacing {
    static constexpr float xs = 4.0f;
    static constexpr float sm = 8.0f;
    static constexpr float md = 16.0f;
    static constexpr float lg = 24.0f;
    static constexpr float xl = 40.0f;
};

struct FontSize {
    static constexpr int sm = 14;
    static constexpr int md = 16;
    static constexpr int lg = 22;
    static constexpr int xl = 32;
};

// ------------------------------------------------------------------------
// Gradient presets, consumed by drawGradientRect(). Kept as functions
// (rather than static vectors) since GradientStop holds a std::vector and
// static-init order across translation units would otherwise be unsafe.
// ------------------------------------------------------------------------
inline std::vector<GradientStop> goldButtonGradient() {
    return { {0.0f, Palette::gold}, {1.0f, Palette::goldBright} };
}

inline std::vector<GradientStop> indigoButtonGradient() {
    return { {0.0f, Palette::indigo}, {1.0f, Palette::indigoBright} };
}

// Two slow-shifting background "scenes" that BaseScreen cross-fades between
// over time. Both share the same dark base at the edges so the transition
// never gets bright enough to hurt foreground text/board readability -
// only the mid-tone shifts between a cool indigo hint and a warm gold hint.
inline std::vector<GradientStop> backgroundSceneA() {
    return {
        {0.0f,  Color{13, 15, 20, 255}},
        {0.55f, Color{20, 18, 34, 255}},  // cool indigo undertone
        {1.0f,  Color{13, 15, 20, 255}}
    };
}

inline std::vector<GradientStop> backgroundSceneB() {
    return {
        {0.0f,  Color{13, 15, 20, 255}},
        {0.55f, Color{28, 20, 15, 255}},  // warm gold-adjacent undertone
        {1.0f,  Color{13, 15, 20, 255}}
    };
}

} // namespace theme
} // namespace ui