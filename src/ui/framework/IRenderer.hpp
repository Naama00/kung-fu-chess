// This is the pure interface defining the logical drawing commands of the system
#pragma once
#include "ui/framework/InputEvents.hpp"
#include <string_view>

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
     * @param endAngle זווית סיום במעלות.
     */
    virtual void drawSector(Vector2D center, float radius, float startAngle, float endAngle, Color color, bool fill = true) = 0;
    // ציור תמונה/טקסטורה מבוססת מזהה טקסטואלי
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
};