#pragma once

#include "ui/framework/IRenderer.hpp"
#include "ui/framework/InputEvents.hpp"
#include <vector>
#include <string>

class SidebarView {
public:
    SidebarView() = default;
    ~SidebarView() = default;

    // מניעת העתקה מטעמי יעילות
    SidebarView(const SidebarView&) = delete;
    SidebarView& operator=(const SidebarView&) = delete;
    SidebarView(SidebarView&&) noexcept = default;
    SidebarView& operator=(SidebarView&&) noexcept = default;

    /**
     * ציור פאנל המהלכים הצידי בהתבסס על ההיסטוריה הלוגית שהתקבלה מהמסך.
     */
    void draw(IRenderer& renderer, 
              const std::vector<std::string>& whiteHistory, 
              const std::vector<std::string>& blackHistory,
              Color bgThemeColor,
              Color borderThemeColor) const 
    {
        // 1. רקע הפאנל הראשי וקו הגבול השמאלי
        renderer.drawRectangle({800.0f, 100.0f}, {200.0f, 800.0f}, bgThemeColor, true);
        renderer.drawLine({800.0f, 100.0f}, {800.0f, 900.0f}, borderThemeColor, 2.0f);

        // --- 2. אזור מהלכי השחקן השחור (למעלה) ---
        renderer.drawRectangle({810.0f, 110.0f}, {180.0f, 360.0f}, {30, 30, 40, 255}, true);
        renderer.drawRectangle({810.0f, 110.0f}, {180.0f, 360.0f}, borderThemeColor, false);
        renderer.drawText("BLACK MOVES", {825.0f, 145.0f}, 13, {160, 160, 175, 255});
        renderer.drawLine({815.0f, 160.0f}, {985.0f, 160.0f}, {50, 50, 65, 255}, 1.0f);

        float blackLogY = 195.0f;
        for (std::size_t i = 0; i < blackHistory.size(); ++i)
        {
            const auto& logEntry = blackHistory[i];
            bool isLatest = (i == blackHistory.size() - 1);
            Color textColor = {170, 170, 185, 255};
            if (logEntry.find("rejected") != std::string::npos || logEntry.find("failed") != std::string::npos)
            {
                textColor = {240, 100, 100, 255};
            }
            else if (logEntry.find("ok") != std::string::npos || logEntry.find("Connected") != std::string::npos || logEntry.find(":") != std::string::npos)
            {
                textColor = {100, 210, 130, 255};
            }
            // הדגשת המהלך האחרון (הנוכחי)
            if (isLatest && blackHistory.size() > 1)
            {
                renderer.drawRectangle({816.0f, blackLogY - 5.0f}, {176.0f, 22.0f}, {50, 50, 75, 200}, true);
                renderer.drawRectangle({816.0f, blackLogY - 5.0f}, {176.0f, 22.0f}, {80, 80, 120, 255}, false);
                textColor = {220, 220, 240, 255};
                if (logEntry.find("rejected") != std::string::npos || logEntry.find("failed") != std::string::npos)
                    textColor = {255, 120, 120, 255};
                else if (logEntry.find(":") != std::string::npos)
                    textColor = {130, 240, 160, 255};
            }
            renderer.drawText(logEntry, {825.0f, blackLogY}, 10, textColor);
            blackLogY += 35.0f;
        }

        // --- 3. אזור מהלכי השחקן הלבן (למטה) ---
        renderer.drawRectangle({810.0f, 490.0f}, {180.0f, 360.0f}, {30, 30, 40, 255}, true);
        renderer.drawRectangle({810.0f, 490.0f}, {180.0f, 360.0f}, borderThemeColor, false);
        renderer.drawText("WHITE MOVES", {825.0f, 525.0f}, 13, {255, 255, 255, 255});
        renderer.drawLine({815.0f, 540.0f}, {985.0f, 540.0f}, {50, 50, 65, 255}, 1.0f);

        float whiteLogY = 575.0f;
        for (std::size_t i = 0; i < whiteHistory.size(); ++i)
        {
            const auto& logEntry = whiteHistory[i];
            bool isLatest = (i == whiteHistory.size() - 1);
            Color textColor = {200, 200, 210, 255};
            if (logEntry.find("rejected") != std::string::npos || logEntry.find("failed") != std::string::npos)
            {
                textColor = {240, 100, 100, 255}; // אדום לכישלון
            }
            else if (logEntry.find("ok") != std::string::npos || logEntry.find("Connected") != std::string::npos || logEntry.find(":") != std::string::npos)
            {
                textColor = {100, 210, 130, 255}; // ירוק להצלחה / מהלך תקין
            }
            // הדגשת המהלך האחרון (הנוכחי)
            if (isLatest && whiteHistory.size() > 1)
            {
                renderer.drawRectangle({816.0f, whiteLogY - 5.0f}, {176.0f, 22.0f}, {55, 55, 30, 200}, true);
                renderer.drawRectangle({816.0f, whiteLogY - 5.0f}, {176.0f, 22.0f}, {120, 120, 60, 255}, false);
                textColor = {255, 255, 220, 255};
                if (logEntry.find("rejected") != std::string::npos || logEntry.find("failed") != std::string::npos)
                    textColor = {255, 120, 120, 255};
                else if (logEntry.find(":") != std::string::npos)
                    textColor = {160, 255, 160, 255};
            }
            renderer.drawText(logEntry, {825.0f, whiteLogY}, 10, textColor);
            whiteLogY += 35.0f;
        }
    }
};