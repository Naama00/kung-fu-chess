#pragma once

#include "ui/framework/IRenderer.hpp"
#include "ui/framework/InputEvents.hpp"
#include "ui/components/Button.hpp"
#include <string>

class HeaderView {
public:
    HeaderView() = default;
    ~HeaderView() = default;

    HeaderView(const HeaderView&) = delete;
    HeaderView& operator=(const HeaderView&) = delete;

    /**
     * Draw the top panel and its associated control buttons.
     */
    void draw(IRenderer& renderer,
              const std::string& title,
              Color bgColor,
              Color borderColor,
              Color titleColor,
              const Button& pauseBtn,
              const Button& restartBtn,
              const Button& menuBtn) const
    {
        // Draw the top panel and bottom border line
        renderer.drawRectangle({0.0f, 0.0f}, {1000.0f, 95.0f}, bgColor, true);
        renderer.drawLine({0.0f, 95.0f}, {1000.0f, 95.0f}, borderColor, 2.0f);

        // Draw the main title
        renderer.drawText(title, {30.0f, 60.0f}, 24, titleColor);

        // Draw the control buttons
        pauseBtn.draw(renderer);
        restartBtn.draw(renderer);
        menuBtn.draw(renderer);
    }
};