// ui/screens/BaseScreen.hpp
#pragma once
#include "ui/framework/IScreen.hpp"
#include "ui/framework/IRenderer.hpp"
#include "ui/framework/InputEvents.hpp"
#include "ui/framework/ScreenManager.hpp"
#include "ui/theme/Theme.hpp"
#include "ui/animations/Animation.hpp"
#include <string>
#include <vector>

class BaseScreen : public IScreen {
protected:
    std::string m_screenTitle;

    // Field names kept identical to the original UITheme so existing screens
    // (e.g. ChessGameScreen's m_theme.buttonNormal / m_theme.border usage)
    // keep compiling unchanged - only the source of the values changed, from
    // hardcoded literals to the shared design system in ui/theme/Theme.hpp.
    struct UITheme {
        Color background   = ui::theme::Palette::bgDeep;
        Color titleText    = ui::theme::Palette::goldBright; // noble gold for titles
        Color bodyText     = ui::theme::Palette::textPrimary;
        Color buttonNormal = ui::theme::Palette::bgSurfaceHi;
        Color buttonHover  = ui::theme::Palette::indigo;     // modern accent on hover
        Color border       = ui::theme::Palette::border;
    } m_theme;

    // Drives the slow-shifting animated background. Derived screens must
    // call tickBackground(deltaTime) once at the top of their own update()
    // for the background to animate.
    float m_backgroundTime = 0.0f;
    static constexpr float kBackgroundCycleSeconds = 14.0f;

    void tickBackground(float deltaTime) {
        m_backgroundTime += deltaTime;
        if (m_backgroundTime >= kBackgroundCycleSeconds) {
            m_backgroundTime -= kBackgroundCycleSeconds;
        }
    }

    bool isPointInRect(Vector2D point, Vector2D rectPos, Vector2D rectSize) const {
        return point.x >= rectPos.x && point.x <= rectPos.x + rectSize.x &&
               point.y >= rectPos.y && point.y <= rectPos.y + rectSize.y;
    }

    // Draws a single smooth gradient (one draw call, interpolated on the GPU)
    // that slowly cross-fades between two color "scenes" over time. Replaces
    // the old approach of manually drawing 100 stacked 10px-tall rectangles
    // every frame to fake a top-to-bottom gradient.
    void drawGradientBackground(IRenderer& renderer) {
        Vector2D size = renderer.getTargetSize();

        float t = m_backgroundTime / kBackgroundCycleSeconds;
        float pingPong = t < 0.5f ? t * 2.0f : (1.0f - t) * 2.0f; // 0 -> 1 -> 0, avoids a hard reset/jump
        float blend = ui::animation::Easing::smoothstep(pingPong);

        auto sceneA = ui::theme::backgroundSceneA();
        auto sceneB = ui::theme::backgroundSceneB();

        std::vector<GradientStop> blended;
        blended.reserve(sceneA.size());
        for (size_t i = 0; i < sceneA.size() && i < sceneB.size(); ++i) {
            blended.push_back({
                sceneA[i].position,
                ui::animation::ColorBlend::lerp(sceneA[i].color, sceneB[i].color, blend)
            });
        }

        renderer.drawGradientRect({0.0f, 0.0f}, size, blended, 120.0f, 0.0f);
    }

    // Floating "card" panel with a soft shadow and frosted-glass fill, now
    // delegating to the renderer's own shadow/glass primitives instead of
    // hand-rolling flat, sharp-cornered rectangles.
    void drawGlassPanel(IRenderer& renderer, Vector2D pos, Vector2D size) {
        renderer.drawRectShadow(pos, size, ui::theme::Radius::lg, {0, 0, 0, 100}, 10.0f, {0.0f, 8.0f});
        renderer.drawGlassPanel(pos, size, ui::theme::Radius::lg, ui::theme::Palette::bgSurface, 0.6f);
    }

    // Modern rounded button with a soft shadow, animated glow on hover, and
    // a gradient fill that shifts between buttonNormal and buttonHover.
    // hoverT is an eased 0..1 progress value (see ui::animation::HoverGroup)
    // rather than a plain bool, so callers can animate the transition instead
    // of switching instantly.
    void drawButton(IRenderer& renderer, const std::string& text, Vector2D pos, Vector2D size, float hoverT) {
        Color base  = ui::animation::ColorBlend::lerp(m_theme.buttonNormal, m_theme.buttonHover, hoverT);
        Color sheen = ui::animation::ColorBlend::lighten(base, 0.18f);

        renderer.drawRectShadow(pos, size, ui::theme::Radius::md, {0, 0, 0, 110},
                                 5.0f + hoverT * 4.0f, {0.0f, 3.0f});

        if (hoverT > 0.02f) {
            Vector2D center{pos.x + size.x / 2.0f, pos.y + size.y / 2.0f};
            renderer.drawGlow(center, size, ui::theme::Radius::md, m_theme.buttonHover, hoverT * 0.5f);
        }

        renderer.drawGradientRect(pos, size, {{0.0f, sheen}, {1.0f, base}}, 90.0f, ui::theme::Radius::md);

        Color borderColor = ui::animation::ColorBlend::lerp(m_theme.border, Color{255, 255, 255, 180}, hoverT);
        renderer.drawRoundedRectangle(pos, size, ui::theme::Radius::md, borderColor, false);

        // Text aligned roughly to the center
        float textX = pos.x + (size.x * 0.15f);
        float textY = pos.y + (size.y / 2.0f) + 6.0f;
        renderer.drawText(text, {textX, textY}, ui::theme::FontSize::md, m_theme.bodyText);
    }

    // Small "chip" style toggle button, for grids of mutually-exclusive
    // options (e.g. game mode / opponent / difficulty selectors). An active
    // option stays lit permanently in the accent color; an inactive option
    // only lights up while hovered (animated via hoverT).
    void drawToggleButton(IRenderer& renderer, const std::string& text, Vector2D pos, Vector2D size,
                           float hoverT, bool isActive) {
        Color restColor = ui::animation::ColorBlend::lighten(m_theme.buttonNormal, 0.15f * hoverT);
        Color base = isActive ? m_theme.buttonHover : restColor;
        Color sheen = ui::animation::ColorBlend::lighten(base, 0.15f);

        if (isActive || hoverT > 0.05f) {
            renderer.drawRectShadow(pos, size, ui::theme::Radius::sm, {0, 0, 0, 90}, 4.0f, {0.0f, 2.0f});
        }

        renderer.drawGradientRect(pos, size, {{0.0f, sheen}, {1.0f, base}}, 90.0f, ui::theme::Radius::sm);

        Color borderColor = isActive ? Color{255, 255, 255, 180} : m_theme.border;
        renderer.drawRoundedRectangle(pos, size, ui::theme::Radius::sm, borderColor, false);

        renderer.drawText(text, {pos.x + size.x * 0.12f, pos.y + size.y * 0.62f}, ui::theme::FontSize::sm, m_theme.bodyText);
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
        // Slow-shifting animated gradient background instead of a static color
        drawGradientBackground(renderer);

        if (!m_screenTitle.empty()) {
            renderer.drawText(m_screenTitle, {50.0f, 60.0f}, ui::theme::FontSize::xl, m_theme.titleText);
        }

        drawScreenDecorations(renderer);
        drawContent(renderer);
    }
};