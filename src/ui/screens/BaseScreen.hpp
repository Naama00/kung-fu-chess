// ui/screens/BaseScreen.hpp
#pragma once

#include "ui/framework/IScreen.hpp"
#include "ui/framework/IRenderer.hpp"
#include "ui/framework/InputEvents.hpp"
#include "ui/theme/Theme.hpp"
#include <string>

class ScreenManager;

class BaseScreen : public IScreen {
public:
    BaseScreen(ScreenManager& manager, std::string screenTitle);
    ~BaseScreen() override = default;

    void draw(IRenderer& renderer) override;

protected:
    struct UITheme {
        Color background   = ui::theme::Palette::bgDeep;
        Color titleText    = ui::theme::Palette::goldBright;
        Color bodyText     = ui::theme::Palette::textPrimary;
        Color textMuted    = ui::theme::Palette::textMuted;  
        Color buttonNormal = ui::theme::Palette::bgSurfaceHi;
        Color buttonHover  = ui::theme::Palette::indigo;
        Color border       = ui::theme::Palette::border;
    };

    void tickBackground(float deltaTime);
    bool isPointInRect(Vector2D point, Vector2D rectPos, Vector2D rectSize) const;

    void drawGradientBackground(IRenderer& renderer);
    void drawGlassPanel(IRenderer& renderer, Vector2D pos, Vector2D size);
    void drawButton(IRenderer& renderer, const std::string& text, Vector2D pos, Vector2D size, float hoverT);
    void drawToggleButton(IRenderer& renderer, const std::string& text, Vector2D pos, Vector2D size, float hoverT, bool isActive);
    void drawScreenDecorations(IRenderer& renderer);

    virtual void drawContent(IRenderer& renderer) = 0;

    std::string m_screenTitle;
    UITheme m_theme;
    float m_backgroundTime = 0.0f;
    static constexpr float kBackgroundCycleSeconds = 14.0f;
};