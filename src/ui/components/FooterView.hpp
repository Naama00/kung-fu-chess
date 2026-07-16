#pragma once

#include "ui/framework/IRenderer.hpp"
#include "ui/framework/InputEvents.hpp"
#include "engine/common/Enums.hpp"
#include <string>

class FooterView {
public:
    FooterView() = default;
    ~FooterView() = default;

    FooterView(const FooterView&) = delete;
    FooterView& operator=(const FooterView&) = delete;

    /**
     * ציור פאנל המידע התחתון, חישוב והצגת התוצאות ומצב המשחק.
     */
    void draw(IRenderer& renderer,
              int whiteScore,
              int blackScore,
              bool isGameOver,
              bool isPaused,
              bool allowSimultaneous,
              kungfu::PlayerColor currentTurn,
              Color bgColor,
              Color borderColor) const
    {
        // ציור פאנל תחתון וקו גבול עליון
        renderer.drawRectangle({0.0f, 905.0f}, {1000.0f, 95.0f}, bgColor, true);
        renderer.drawLine({0.0f, 905.0f}, {1000.0f, 905.0f}, borderColor, 2.0f);

        // הצגת ניקוד השחקנים
        renderer.drawText("WHITE Score: " + std::to_string(whiteScore) + " / 39", {40.0f, 960.0f}, 15, {255, 255, 255, 255});
        renderer.drawText("BLACK Score: " + std::to_string(blackScore) + " / 39", {340.0f, 960.0f}, 15, {170, 170, 180, 255});

        // הצגת סטטוס המשחק
        renderer.drawText("GAME STATUS: ", {640.0f, 960.0f}, 13, {150, 150, 170, 255});
        
        if (isGameOver)
        {
            renderer.drawText("Game Over!", {750.0f, 960.0f}, 14, {240, 80, 80, 255});
        }
        else if (isPaused)
        {
            renderer.drawText("Paused", {750.0f, 960.0f}, 14, {240, 200, 80, 255});
        }
        else
        {
            if (!allowSimultaneous)
            {
                if (currentTurn == kungfu::PlayerColor::White)
                {
                    renderer.drawText("White's Turn", {750.0f, 960.0f}, 14, {240, 240, 250, 255});
                }
                else
                {
                    renderer.drawText("Black's Turn", {750.0f, 960.0f}, 14, {150, 150, 160, 255});
                }
            }
            else
            {
                renderer.drawText("Real-Time Battle!", {750.0f, 960.0f}, 14, {80, 180, 240, 255});
            }
        }
    }
};