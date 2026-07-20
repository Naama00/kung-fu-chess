// ui/screens/BaseScreen.hpp
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

    struct UITheme {
        Color background{15, 16, 22, 255};       // Basic dark background
        Color titleText{240, 200, 80, 255};      // Noble gold for titles
        Color bodyText{210, 215, 225, 255};      // Soft light gray for text
        Color buttonNormal{35, 37, 45, 255};     // Button in normal state
        Color buttonHover{48, 120, 192, 255};    // Active blue on hover
        Color border{65, 68, 85, 255};           // Fine borders
    } m_theme;

    bool isPointInRect(Vector2D point, Vector2D rectPos, Vector2D rectSize) const {
        return point.x >= rectPos.x && point.x <= rectPos.x + rectSize.x &&
               point.y >= rectPos.y && point.y <= rectPos.y + rectSize.y;
    }

    // Draw a rich, smooth gradient background (from deep blue to charcoal black)
    void drawGradientBackground(IRenderer& renderer, Color topColor, Color bottomColor) {
        float screenHeight = 1000.0f;
        float stripHeight = 10.0f; 
        int numStrips = static_cast<int>(screenHeight / stripHeight);
        
        for (int i = 0; i < numStrips; ++i) {
            float t = static_cast<float>(i) / numStrips;
            Color current;
            current.r = static_cast<uint8_t>(topColor.r + t * (bottomColor.r - topColor.r));
            current.g = static_cast<uint8_t>(topColor.g + t * (bottomColor.g - topColor.g));
            current.b = static_cast<uint8_t>(topColor.b + t * (bottomColor.b - topColor.b));
            current.a = 255;
            renderer.drawRectangle({0.0f, i * stripHeight}, {1000.0f, stripHeight}, current, true);
        }
    }

    // Draw a semi-transparent floating "card" with a shadow (Glassmorphism Card)
    void drawGlassPanel(IRenderer& renderer, Vector2D pos, Vector2D size) {
        // Draw a soft shadow under the panel
        renderer.drawRectangle({pos.x + 8.0f, pos.y + 8.0f}, size, {0, 0, 0, 80}, true);
        // Panel background (dark and semi-transparent)
        renderer.drawRectangle(pos, size, {25, 27, 35, 230}, true);
        // Modern, thin frame line
        renderer.drawRectangle(pos, size, {65, 68, 85, 255}, false);
    }

    // Draw a lightly rounded modern button
    void drawButton(IRenderer& renderer, const std::string& text, Vector2D pos, Vector2D size, bool isHovered) {
        Color btnColor = isHovered ? m_theme.buttonHover : m_theme.buttonNormal;
        
        // Button background
        renderer.drawRectangle(pos, size, btnColor, true);
        // Button border
        renderer.drawRectangle(pos, size, isHovered ? Color{255, 255, 255, 180} : m_theme.border, false);

        // Text aligned roughly to the center
        float textX = pos.x + (size.x * 0.15f); 
        float textY = pos.y + (size.y / 2.0f) + 6.0f;
        renderer.drawText(text, {textX, textY}, 16, m_theme.bodyText);
    }

    void drawScreenDecorations(IRenderer& renderer) {
        // Thin, subtle frame around the screen edges
        renderer.drawRectangle({5.0f, 5.0f}, {990.0f, 990.0f}, {55, 58, 70, 100}, false);
    }

    virtual void drawContent(IRenderer& renderer) = 0;

public:
    BaseScreen(ScreenManager& manager, std::string screenTitle)
        : IScreen(manager), m_screenTitle(std::move(screenTitle)) {}

    virtual ~BaseScreen() = default;

    void draw(IRenderer& renderer) override {
        // Draw a half-night gradient background instead of a solid black color
        drawGradientBackground(renderer, {15, 20, 32, 255}, {10, 11, 14, 255});

        if (!m_screenTitle.empty()) {
            renderer.drawText(m_screenTitle, {50.0f, 60.0f}, 24, m_theme.titleText);
        }

        drawScreenDecorations(renderer);
        drawContent(renderer);
    }
};