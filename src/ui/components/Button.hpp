// This class represents a logical button. It manages its visual state
// (normal, hover, or pressed) and detects clicks on the logical axis system
#pragma once
#include "ui/framework/InputEvents.hpp"
#include <string>
#include <string_view>
#include <functional>
#include <algorithm>
#include "ui/framework/IRenderer.hpp"
#include "ui/animations/Animation.hpp"
#include "ui/theme/Theme.hpp"

class Button {
private:
    Vector2D m_position;
    Vector2D m_size;
    std::string m_text;

    Color m_normalColor = ui::theme::Palette::bgSurfaceHi;
    Color m_hoverColor  = ui::theme::Palette::indigo;
    Color m_textColor   = ui::theme::Palette::textPrimary;

    bool m_isHovered = false;
    float m_hoverProgress = 0.0f; // animated 0..1, smoothed instead of snapping instantly
    static constexpr float kHoverAnimSpeed = 6.0f; // higher = snappier transition

    std::function<void()> m_onClickCallback;

public:
    Button(Vector2D position, Vector2D size, std::string text, std::function<void()> onClickCallback)
        : m_position(position), m_size(size), m_text(std::move(text)), m_onClickCallback(std::move(onClickCallback)) {}

    // Smoothly animates the hover transition instead of switching instantly.
    void update(float deltaTime) {
        float target = m_isHovered ? 1.0f : 0.0f;
        float step = kHoverAnimSpeed * deltaTime;

        if (m_hoverProgress < target) {
            m_hoverProgress = std::min(target, m_hoverProgress + step);
        } else if (m_hoverProgress > target) {
            m_hoverProgress = std::max(target, m_hoverProgress - step);
        }
    }

    // Draw the button using the abstract Renderer
    void draw(IRenderer& renderer) const {
        float eased = ui::animation::Easing::smoothstep(m_hoverProgress);

        Color base  = ui::animation::ColorBlend::lerp(m_normalColor, m_hoverColor, eased);
        Color sheen = ui::animation::ColorBlend::lighten(base, 0.18f);

        // Soft drop shadow, growing slightly on hover to read as "lifted"
        renderer.drawRectShadow(m_position, m_size, ui::theme::Radius::md, {0, 0, 0, 110},
                                 5.0f + eased * 4.0f, {0.0f, 3.0f});

        // Glow ring appears as hover intensifies, using the hover color as tint
        if (eased > 0.02f) {
            Vector2D center{m_position.x + m_size.x / 2.0f, m_position.y + m_size.y / 2.0f};
            renderer.drawGlow(center, m_size, ui::theme::Radius::md, m_hoverColor, eased * 0.55f);
        }

        // Gradient fill with a subtle vertical sheen; the angle drifts a little
        // on hover so the highlight seems to catch the light as it lifts.
        float angle = 90.0f - eased * 15.0f;
        renderer.drawGradientRect(m_position, m_size, {{0.0f, sheen}, {1.0f, base}}, angle, ui::theme::Radius::md);

        // Thin border, brightens on hover
        Color borderColor = ui::animation::ColorBlend::lerp({120, 120, 120, 255}, {220, 220, 230, 255}, eased);
        renderer.drawRoundedRectangle(m_position, m_size, ui::theme::Radius::md, borderColor, false);

        // Calculate text position centered horizontally and vertically within the button
        Vector2D textPos{m_position.x + ui::theme::Spacing::md, m_position.y + (m_size.y * 0.3f)};
        renderer.drawText(m_text, textPos, ui::theme::FontSize::md, m_textColor);
    }

    // Handle mouse input events to detect hover and click actions
    void handleInput(const MouseEvent& mouseEvent) {
        // Check if the mouse is hovering over the button
        m_isHovered = (mouseEvent.logicalX >= m_position.x && 
                       mouseEvent.logicalX <= m_position.x + m_size.x &&
                       mouseEvent.logicalY >= m_position.y && 
                       mouseEvent.logicalY <= m_position.y + m_size.y);

        // If the button is hovered and the left mouse button is pressed, trigger the click callback
        if (m_isHovered && mouseEvent.action == MouseEvent::Action::Press && mouseEvent.button == MouseButton::Left) {
            if (m_onClickCallback) {
                m_onClickCallback();
            }
        }
    }

    // Set custom colors for the button's normal, hover, and text states.
    // Kept for backward compatibility with existing call sites (e.g.
    // ChessGameScreen assigns semantic colors: green = resume, red = quit,
    // blue = restart). These now become animated gradient endpoints instead
    // of a binary color switch, so every existing call site gets the new
    // rounded/shadow/glow/gradient treatment for free.
    void setColors(Color normal, Color hover, Color text) {
        m_normalColor = normal;
        m_hoverColor = hover;
        m_textColor = text;
    }
};