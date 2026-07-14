// זהו הממשק הטהור המגדיר את פקודות הציור הלוגיות של המערכת
#pragma once
#include "InputEvents.hpp"
#include <string_view>

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // מחזור חיים של פריים
    virtual void beginFrame() = 0;
    virtual void clear(Color color) = 0;
    virtual void endFrame() = 0;

    // פעולות ציור בסיסיות (פרימיטיביים)
    virtual void drawRectangle(Vector2D position, Vector2D size, Color color, bool fill = true) = 0;
    virtual void drawLine(Vector2D start, Vector2D end, Color color, float thickness = 1.0f) = 0;
    virtual void drawCircle(Vector2D center, float radius, Color color, bool fill = true) = 0;

    // ציור תמונה/טקסטורה מבוססת מזהה טקסטואלי
    virtual void drawSprite(std::string_view assetId, 
                            Vector2D position, 
                            Vector2D size, 
                            float rotationDegrees = 0.0f,
                            const Vector2D* srcOffset = nullptr,
                            const Vector2D* srcSize = nullptr) = 0;

    // רינדור טקסט
    virtual void drawText(std::string_view text, Vector2D position, int fontSize, Color color) = 0;

    // קבלת ממדי חלון הציור הפיזי בפועל
    virtual Vector2D getTargetSize() const = 0;
};