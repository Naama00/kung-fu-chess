// ui/screens/BaseScreen.cpp
#include "ui/screens/BaseScreen.hpp"
#include "ui/framework/ScreenManager.hpp"
#include "ui/animations/Animation.hpp"
#include <vector>
#include <utility>

BaseScreen::BaseScreen(ScreenManager& manager, std::string screenTitle)
    : IScreen(manager), m_screenTitle(std::move(screenTitle)) {}

void BaseScreen::tickBackground(float deltaTime) {
    m_backgroundTime += deltaTime;
    if (m_backgroundTime >= kBackgroundCycleSeconds) {
        m_backgroundTime -= kBackgroundCycleSeconds;
    }
}

bool BaseScreen::isPointInRect(Vector2D point, Vector2D rectPos, Vector2D rectSize) const {
    return point.x >= rectPos.x && point.x <= rectPos.x + rectSize.x &&
           point.y >= rectPos.y && point.y <= rectPos.y + rectSize.y;
}

void BaseScreen::drawGradientBackground(IRenderer& renderer) {
    Vector2D size = renderer.getTargetSize();

    float t = m_backgroundTime / kBackgroundCycleSeconds;
    float pingPong = t < 0.5f ? t * 2.0f : (1.0f - t) * 2.0f;
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

void BaseScreen::drawGlassPanel(IRenderer& renderer, Vector2D pos, Vector2D size) {
    renderer.drawRectShadow(pos, size, ui::theme::Radius::lg, {0, 0, 0, 100}, 10.0f, {0.0f, 8.0f});
    renderer.drawGlassPanel(pos, size, ui::theme::Radius::lg, ui::theme::Palette::bgSurface, 0.6f);
}

void BaseScreen::drawButton(IRenderer& renderer, const std::string& text, Vector2D pos, Vector2D size, float hoverT) {
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

    float textX = pos.x + (size.x * 0.15f);
    float textY = pos.y + (size.y / 2.0f) + 6.0f;
    renderer.drawText(text, {textX, textY}, ui::theme::FontSize::md, m_theme.bodyText);
}

void BaseScreen::drawToggleButton(IRenderer& renderer, const std::string& text, Vector2D pos, Vector2D size,
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

void BaseScreen::drawScreenDecorations(IRenderer& renderer) {
    renderer.drawRectangle({5.0f, 5.0f}, {990.0f, 990.0f}, {55, 58, 70, 100}, false);
}

void BaseScreen::draw(IRenderer& renderer) {
    drawGradientBackground(renderer);

    if (!m_screenTitle.empty()) {
        renderer.drawText(m_screenTitle, {50.0f, 60.0f}, ui::theme::FontSize::xl, m_theme.titleText);
    }

    drawScreenDecorations(renderer);
    drawContent(renderer);
}