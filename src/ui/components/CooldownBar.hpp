// מד צינון ויזואלי המיועד להצגה מעל או מתחת לכלי השחמט הפעילים. 
// הוא מקבל את אחוז ההתקדמות ומצייר מלבן שמתמלא או מתרוקן בהתאם
#pragma once
#include "ui/framework/InputEvents.hpp"
#include "ui/framework/IRenderer.hpp"

class CooldownBar {
private:
    Vector2D m_size{40.0f, 6.0f}; // ממדים קבועים לוגיים של מד הצינון
    Color m_backgroundColor{40, 40, 40, 200};
    Color m_fillColor{0, 220, 100, 255}; // צבע ירוק למצב מוכן או טעינה
    Color m_cooldownColor{220, 50, 50, 255}; // צבע אדום בזמן צינון פעיל

public:
    CooldownBar() = default;

    /**
     * ציור מד הצינון.
     * @param renderer המצייר האבסטרקטי.
     * @param centerPosition המיקום המרכזי של הכלי (המד יצוירו מעט מתחתיו).
     * @param cooldownProgress ערך בין 0.0f ל-1.0f (כאשר 1.0f מייצג צינון מלא ופעיל, ו-0.0f מייצג כלי מוכן לתנועה).
     */
    void draw(IRenderer& renderer, Vector2D centerPosition, float cooldownProgress) const {
        if (cooldownProgress <= 0.0f) {
            return; // אם הכלי לא בצינון, אין צורך לצייר את המד
        }

        // חישוב מיקום המלבן כך שיהיה מיושר למרכז הכלי ומעט מתחתיו
        Vector2D barPos{
            centerPosition.x - (m_size.x / 2.0f),
            centerPosition.y + 25.0f // היסט אנכי מתחת לכלי
        };

        // 1. ציור רקע המד (מלבן אפור כהה)
        renderer.drawRectangle(barPos, m_size, m_backgroundColor, true);

        // 2. חישוב רוחב החלק המלא לפי אחוז הצינון שנותר
        float filledWidth = m_size.x * (1.0f - cooldownProgress);
        Vector2D fillSize{filledWidth, m_size.y};

        // 3. ציור מד המילוי (בצבע הצינון)
        renderer.drawRectangle(barPos, fillSize, m_cooldownColor, true);

        // 4. ציור מסגרת דקה סביב המד
        renderer.drawRectangle(barPos, m_size, {200, 200, 200, 255}, false);
    }

    void setSize(Vector2D size) {
        m_size = size;
    }
};