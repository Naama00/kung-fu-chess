// This is the pure interface defining the logical drawing commands of the system
#pragma once
#include "ui/framework/InputEvents.hpp"
#include <string_view>
#include <vector>

// A single stop in a color gradient (used by drawGradientRect).
// 'position' is 0.0 (start) to 1.0 (end) along the gradient axis.
struct GradientStop {
    float position;
    Color color;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // Frame lifecycle
    virtual void beginFrame() = 0;
    virtual void clear(Color color) = 0;
    virtual void endFrame() = 0;

    // Present the frame drawn on the physical display surface (for example cv::imshow).
    virtual void presentFrame() = 0;

    // Check whether the display surface (window) is still open from the user/OS perspective.
    virtual bool isWindowOpen() const = 0;

    // Basic drawing operations
    virtual void drawRectangle(Vector2D position, Vector2D size, Color color, bool fill = true) = 0;
    virtual void drawLine(Vector2D start, Vector2D end, Color color, float thickness = 1.0f) = 0;
    virtual void drawCircle(Vector2D center, float radius, Color color, bool fill = true) = 0;
        /**
     * Draw a circular sector for effects such as timers, circular cooldowns, etc.
     * @param startAngle Start angle in degrees (for example, 90- represents the top).
     * @param endAngle End angle in degrees (for example, 0- represents the right).
     * @param fill Whether to fill the sector or just draw the outline.
     */
    virtual void drawSector(Vector2D center, float radius, float startAngle, float endAngle, Color color, bool fill = true) = 0;
    // Draw a sprite (image) from the asset manager with optional rotation and source rectangle.
    virtual void drawSprite(std::string_view assetId,
                            Vector2D position,
                            Vector2D size,
                            float rotationDegrees = 0.0f,
                            const Vector2D* srcOffset = nullptr,
                            const Vector2D* srcSize = nullptr) = 0;

    // Render text
    virtual void drawText(std::string_view text, Vector2D position, int fontSize, Color color) = 0;

    // Retrieve the actual physical drawing window dimensions
    virtual Vector2D getTargetSize() const = 0;

    // ------------------------------------------------------------------
    // Modern UI drawing capabilities
    // ------------------------------------------------------------------

    /**
     * @brief Draw a rectangle with rounded corners.
     * @param radius Corner radius in logical pixels. Automatically clamped
     *        to half of the smaller dimension of 'size' to avoid overlap.
     */
    virtual void drawRoundedRectangle(Vector2D position, Vector2D size, float radius,
                                       Color color, bool fill = true) = 0;

    /**
     * @brief Draw a rectangle filled with a linear gradient between two or more stops.
     * @param angleDegrees Direction of the gradient (0 = left-to-right, 90 = top-to-bottom).
     * @param cornerRadius Optional rounded corners (0 = sharp corners).
     */
    virtual void drawGradientRect(Vector2D position, Vector2D size,
                                   const std::vector<GradientStop>& stops,
                                   float angleDegrees = 90.0f,
                                   float cornerRadius = 0.0f) = 0;

    /**
     * @brief Draw a soft drop shadow behind a rectangular shape.
     * This does NOT draw the shape itself - call drawRoundedRectangle afterwards.
     * @param blurRadius How far the shadow fades out, in logical pixels.
     * @param offset Shadow displacement relative to the shape (e.g. {0, 4} for a shadow below).
     */
    virtual void drawRectShadow(Vector2D position, Vector2D size, float cornerRadius,
                                 Color shadowColor, float blurRadius, Vector2D offset = {0.0f, 0.0f}) = 0;

    /**
     * @brief Draw a soft glow around a rectangular or circular shape (e.g. focus rings,
     * highlighted/active state). Distinct from drawRectShadow: a glow is centered on the
     * shape's edge and typically uses a saturated accent color instead of black.
     */
    virtual void drawGlow(Vector2D center, Vector2D size, float cornerRadius,
                           Color glowColor, float intensity) = 0;

    /**
     * @brief Push a semi-transparent color overlay to simulate frosted glass
     * (glassmorphism) behind subsequent draw calls in this region.
     * Implementations without true blur support may approximate with a
     * translucent tinted rectangle - see implementation notes.
     */
    virtual void drawGlassPanel(Vector2D position, Vector2D size, float cornerRadius,
                                 Color tint, float blurStrength) = 0;
};