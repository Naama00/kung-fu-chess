// מחלקה זו מייצגת כפתור לוגי. היא מנהלת את המצב הוויזואלי שלו 
// (רגיל, ריחופית - Hover, או לחוץ) ומזהה לחיצות במערכת הצירים הלוגית
#pragma once
#include "ui/framework/InputEvents.hpp"
#include <string>
#include <string_view>
#include <functional>
#include "ui/framework/IRenderer.hpp"

class Button {
private:
    Vector2D m_position;
    Vector2D m_size;
    std::string m_text;
    
    Color m_normalColor{60, 60, 60, 255};
    Color m_hoverColor{90, 90, 90, 255};
    Color m_textColor{255, 255, 255, 255};
    
    bool m_isHovered = false;
    std::function<void()> m_onClickCallback;

public:
    Button(Vector2D position, Vector2D size, std::string text, std::function<void()> onClickCallback)
        : m_position(position), m_size(size), m_text(std::move(text)), m_onClickCallback(std::move(onClickCallback)) {}

    // עדכון מצב הכפתור (למשל לצורך אנימציות עתידיות)
    void update(float deltaTime) {
        (void)deltaTime; // כרגע ללא שימוש
    }

    // ציור הכפתור באמצעות ה-Renderer האבסטרקטי
    void draw(IRenderer& renderer) const {
        Color currentBg = m_isHovered ? m_hoverColor : m_normalColor;
        
        // ציור הרקע של הכפתור
        renderer.drawRectangle(m_position, m_size, currentBg, true);
        
        // ציור מסגרת דקה
        renderer.drawRectangle(m_position, m_size, {120, 120, 120, 255}, false);
        
        // חישוב מיקום טקסט מקורב למרכז (ניתן לשפר בהתאם ליכולות רינדור הטקסט של ה-Renderer)
        Vector2D textPos{m_position.x + 15.0f, m_position.y + (m_size.y * 0.3f)};
        renderer.drawText(m_text, textPos, 16, m_textColor);
    }

    // טיפול בקלט ובדיקת אינטראקציה עם העכבר
    void handleInput(const MouseEvent& mouseEvent) {
        // בדיקה האם העכבר נמצא מעל שטח הכפתור הלוגי
        m_isHovered = (mouseEvent.logicalX >= m_position.x && 
                       mouseEvent.logicalX <= m_position.x + m_size.x &&
                       mouseEvent.logicalY >= m_position.y && 
                       mouseEvent.logicalY <= m_position.y + m_size.y);

        // אם בוצעה לחיצה כשהעכבר מעל הכפתור, מפעילים את ה-Callback
        if (m_isHovered && mouseEvent.action == MouseEvent::Action::Press && mouseEvent.button == MouseButton::Left) {
            if (m_onClickCallback) {
                m_onClickCallback();
            }
        }
    }

    // הגדרת צבעים מותאמים אישית
    void setColors(Color normal, Color hover, Color text) {
        m_normalColor = normal;
        m_hoverColor = hover;
        m_textColor = text;
    }
};