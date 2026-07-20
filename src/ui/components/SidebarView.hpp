#pragma once

#include "ui/framework/IRenderer.hpp"
#include "ui/framework/InputEvents.hpp"
#include <vector>
#include <string>

class SidebarView {
private:
    // Data structure for defining a unique visual style for each panel
    struct PanelStyle {
        Color headerText;
        Color defaultText;
        Color highlightFill;
        Color highlightBorder;
        Color latestText;
        Color latestSuccessText;
    };

    /**
     * Private helper function that prevents code duplication (DRY).
     * Draws a single move-history panel based on a vertical position and style.
     */
    void drawHistoryPanel(IRenderer& renderer,
                          const std::string& title,
                          const std::vector<std::string>& history,
                          float startY,
                          Color borderThemeColor,
                          const PanelStyle& style) const
    {
        // 1. Panel background and outer border
        renderer.drawRectangle({810.0f, startY}, {180.0f, 360.0f}, {30, 30, 40, 255}, true);
        renderer.drawRectangle({810.0f, startY}, {180.0f, 360.0f}, borderThemeColor, false);
        
        // 2. Panel title and separator line
        renderer.drawText(title, {825.0f, startY + 35.0f}, 13, style.headerText);
        renderer.drawLine({815.0f, startY + 50.0f}, {985.0f, startY + 50.0f}, {50, 50, 65, 255}, 1.0f);

        // 3. Iterate over the move list and draw them
        float logY = startY + 85.0f;
        for (std::size_t i = 0; i < history.size(); ++i)
        {
            const auto& logEntry = history[i];
            bool isLatest = (i == history.size() - 1);
            Color textColor = style.defaultText;

            // Determine the base color from the row content
            if (logEntry.find("rejected") != std::string::npos || logEntry.find("failed") != std::string::npos)
            {
                textColor = {240, 100, 100, 255}; // red for failure
            }
            else if (logEntry.find("ok") != std::string::npos || logEntry.find("Connected") != std::string::npos || logEntry.find(":") != std::string::npos)
            {
                textColor = {100, 210, 130, 255}; // green for success
            }

            // Special visual emphasis for the last move in the list
            if (isLatest && history.size() > 1)
            {
                renderer.drawRectangle({816.0f, logY - 5.0f}, {176.0f, 22.0f}, style.highlightFill, true);
                renderer.drawRectangle({816.0f, logY - 5.0f}, {176.0f, 22.0f}, style.highlightBorder, false);
                
                textColor = style.latestText;
                
                if (logEntry.find("rejected") != std::string::npos || logEntry.find("failed") != std::string::npos)
                {
                    textColor = {255, 120, 120, 255};
                }
                else if (logEntry.find(":") != std::string::npos)
                {
                    textColor = style.latestSuccessText;
                }
            }

            renderer.drawText(logEntry, {825.0f, logY}, 10, textColor);
            logY += 35.0f;
        }
    }

public:
    SidebarView() = default;
    ~SidebarView() = default;

    // Prevent copying for efficiency
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
        // 1. Main panel background and left border line
        renderer.drawRectangle({800.0f, 100.0f}, {200.0f, 800.0f}, bgThemeColor, true);
        renderer.drawLine({800.0f, 100.0f}, {800.0f, 900.0f}, borderThemeColor, 2.0f);

        // 2. Define the precise style sets for each side
        PanelStyle blackStyle{
            {160, 160, 175, 255}, // headerText
            {170, 170, 185, 255}, // defaultText
            {50, 50, 75, 200},    // highlightFill
            {80, 80, 120, 255},   // highlightBorder
            {220, 220, 240, 255}, // latestText
            {130, 240, 160, 255}  // latestSuccessText
        };

        PanelStyle whiteStyle{
            {255, 255, 255, 255}, // headerText
            {200, 200, 210, 255}, // defaultText
            {55, 55, 30, 200},    // highlightFill
            {120, 120, 60, 255},  // highlightBorder
            {255, 255, 220, 255}, // latestText
            {160, 255, 160, 255}  // latestSuccessText
        };

        // 3. Call the shared helper function for both panels where needed
        drawHistoryPanel(renderer, "BLACK MOVES", blackHistory, 110.0f, borderThemeColor, blackStyle);
        drawHistoryPanel(renderer, "WHITE MOVES", whiteHistory, 490.0f, borderThemeColor, whiteStyle);
    }
};