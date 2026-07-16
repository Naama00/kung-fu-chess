#pragma once
#include "ui/framework/IScreen.hpp"
#include "ui/framework/IRenderer.hpp"
#include "ui/framework/InputEvents.hpp"
#include "ui/framework/ScreenManager.hpp"
#include <string>
#include <vector>

class BaseScreen : public IScreen {
protected:
    std::string m_screenTitle;

    // הגדרת ערכת נושא (Theme) אחידה ומעוצבת
    struct UITheme {
        Color background{25, 25, 25, 255};      // רקע כהה
        Color titleText{255, 215, 0, 255};       // זהב לכותרות
        Color bodyText{230, 230, 230, 255};      // טקסט רגיל אפור בהיר
        Color buttonNormal{50, 50, 50, 255};     // כפתור במצב רגיל
        Color buttonHover{0, 120, 215, 255};     // כחול מודגש ב-Hover
        Color border{100, 100, 100, 255};        // מסגרות וקווים
    } m_theme;

    // בדיקה האם נקודה (עכבר) נמצאת בתוך מלבן
    bool isPointInRect(Vector2D point, Vector2D rectPos, Vector2D rectSize) const {
        return point.x >= rectPos.x && point.x <= rectPos.x + rectSize.x &&
               point.y >= rectPos.y && point.y <= rectPos.y + rectSize.y;
    }

    // ציור כפתור מעוצב בצורה אחידה
    void drawButton(IRenderer& renderer, const std::string& text, Vector2D pos, Vector2D size, bool isHovered) {
        Color btnColor = isHovered ? m_theme.buttonHover : m_theme.buttonNormal;
        
        // רקע הכפתור
        renderer.drawRectangle(pos, size, btnColor, true);
        // מסגרת הכפתור
        renderer.drawRectangle(pos, size, m_theme.border, false);

        // כתיבת הטקסט במרכז הכפתור
        float textX = pos.x + 20.0f; 
        float textY = pos.y + (size.y / 2.0f) + 6.0f;
        renderer.drawText(text, {textX, textY}, 16, m_theme.bodyText);
    }

    // ציור אלמנטים עיצוביים משותפים
    void drawScreenDecorations(IRenderer& renderer) {
        // קו הפרדה מתחת לכותרת
        renderer.drawLine({50.0f, 90.0f}, {950.0f, 90.0f}, m_theme.border, 2.0f);
        // מסגרת עדינה סביב הקנבס
        renderer.drawRectangle({5.0f, 5.0f}, {990.0f, 990.0f}, m_theme.border, false);
    }

    // שלב הציור הייחודי של המסך היורש (ממומש על ידי הבנים)
    virtual void drawContent(IRenderer& renderer) = 0;

public:
    // העברת ה-ScreenManager לבנאי של IScreen
    BaseScreen(ScreenManager& manager, std::string screenTitle)
        : IScreen(manager), m_screenTitle(std::move(screenTitle)) {}

    virtual ~BaseScreen() = default;

    // מימוש ה-draw של ממשק IScreen
    void draw(IRenderer& renderer) override {
        renderer.clear(m_theme.background);

        if (!m_screenTitle.empty()) {
            renderer.drawText(m_screenTitle, {50.0f, 60.0f}, 32, m_theme.titleText);
        }

        drawScreenDecorations(renderer);
        drawContent(renderer);
    }
};