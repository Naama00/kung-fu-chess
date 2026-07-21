// ui/animations/Animation.hpp
#pragma once

#include <algorithm>
#include <cmath>
#include "ui/framework/InputEvents.hpp"

namespace ui {
namespace animation {

/**
 * @brief Easing functions to calculate smooth interpolation factors.
 */
class Easing {
public:
    // Linear transition
    static float linear(float t) noexcept {
        return std::clamp(t, 0.0f, 1.0f);
    }

    // Cubic Hermite transition (Smoothstep)
    static float smoothstep(float t) noexcept {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    // Parabolic curve reaching a peak height of 1.0 at t = 0.5
    static float parabola(float t) noexcept {
        t = std::clamp(t, 0.0f, 1.0f);
        return 4.0f * t * (1.0f - t);
    }
};

/**
 * @brief Interpolates logical spatial transitions into physical rendering coordinates.
 */
class Interpolator {
public:
    struct PixelPos {
        float x;
        float y;
    };

    /**
     * @brief Dynamic interpolation based on raw elapsed timing and coordinate parameters.
     */
    static PixelPos interpolate(
        float startX, float startY,
        float endX, float endY,
        int startTimeMs, int arrivalTimeMs, int currentTimeMs,
        bool isAirborne, float cellSize) noexcept 
    {
        int duration = arrivalTimeMs - startTimeMs;
        float t = 1.0f;
        
        if (duration > 0) {
            int elapsed = currentTimeMs - startTimeMs;
            t = static_cast<float>(elapsed) / static_cast<float>(duration);
            t = std::clamp(t, 0.0f, 1.0f);
        }

        PixelPos result{};
        if (isAirborne) {
            // Linear progression across X/Y axis combined with a parabolic vertical jump
            result.x = startX + t * (endX - startX);
            result.y = startY + t * (endY - startY);

            constexpr float kJumpHeightFactor = 1.5f;
            float maxHeight = cellSize * kJumpHeightFactor;
            result.y -= maxHeight * Easing::parabola(t);
        } else {
            // Smoothed cubic movement for sliding transitions
            float smoothedT = Easing::smoothstep(t);
            result.x = startX + smoothedT * (endX - startX);
            result.y = startY + smoothedT * (endY - startY);
        }

        return result;
    }
};

/**
 * @brief Helpers for interpolating and adjusting Color values, used to
 * animate hover states, shifting gradients, and other color-based transitions.
 */
class ColorBlend {
public:
    // Linearly interpolates between two colors. t is clamped to [0, 1].
    static Color lerp(Color a, Color b, float t) noexcept {
        t = Easing::linear(t);
        return {
            static_cast<std::uint8_t>(a.r + (b.r - a.r) * t),
            static_cast<std::uint8_t>(a.g + (b.g - a.g) * t),
            static_cast<std::uint8_t>(a.b + (b.b - a.b) * t),
            static_cast<std::uint8_t>(a.a + (b.a - a.a) * t)
        };
    }

    // Blends a color toward white by 'amount' (0 = unchanged, 1 = pure white).
    // Useful for cheap "sheen" gradients on flat-colored surfaces.
    static Color lighten(Color color, float amount) noexcept {
        return lerp(color, Color{255, 255, 255, color.a}, amount);
    }

    // Blends a color toward black by 'amount' (0 = unchanged, 1 = pure black).
    static Color darken(Color color, float amount) noexcept {
        return lerp(color, Color{0, 0, 0, color.a}, amount);
    }
};

} // namespace animation
} // namespace ui