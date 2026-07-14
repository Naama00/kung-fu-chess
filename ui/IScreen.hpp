// ממשק הבסיס המגדיר את החוזה עבור כל מסך או תפריט במערכת
#pragma once
#include <vector>

class IRenderer;
struct InputEvent;
class ScreenManager;

class IScreen {
protected:
    ScreenManager& m_screenManager;

public:
    explicit IScreen(ScreenManager& manager) : m_screenManager(manager) {}
    virtual ~IScreen() = default;

    // נקרא בעת כניסה למסך או הפיכתו לפעיל בראש המחסנית
    virtual void onEnter() = 0;
    
    // נקרא בעת עזיבת המסך או הסרתו מהמחסנית
    virtual void onExit() = 0;

    // עדכון הלוגיקה הפנימית של המסך (deltaTime בשניות)
    virtual void update(float deltaTime) = 0;

    // רינדור רכיבי המסך אל ה-Renderer
    virtual void draw(IRenderer& renderer) = 0;

    // עיבוד אירועי הקלט שהתקבלו בפריים הנוכחי
    virtual void handleInput(const std::vector<InputEvent>& events) = 0;
};