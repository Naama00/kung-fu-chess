// graphics/sfml/SfmlRenderer.hpp
#pragma once
#include "ui/framework/IRenderer.hpp"
#include "ui/framework/AssetManager.hpp"
#include "SfmlAssets.hpp"
#include <SFML/Graphics.hpp>
#include <string_view>
#include <memory>
#include <cmath>
#include <algorithm>

class SfmlRenderer : public IRenderer {
private:
    sf::RenderWindow& m_window;
    Vector2D m_logicalRange{1000.0f, 1000.0f};
    sf::View m_view;
    AssetManager m_assetManager;
    std::string m_defaultFontId;

    sf::Color toSfColor(Color color) const {
        return sf::Color(color.r, color.g, color.b, color.a);
    }

    sf::Vector2f toSfVec(Vector2D vec) const {
        return sf::Vector2f(vec.x, vec.y);
    }

    // ------------------------------------------------------------------
    // Helpers for modern UI drawing (rounded corners, gradients, shadows)
    // ------------------------------------------------------------------

    // Builds the boundary points of a rectangle with rounded corners, going
    // clockwise starting from the top-left arc. Shared by every method below
    // so rounded rectangles, gradients, shadows and glows all look consistent.
    std::vector<sf::Vector2f> buildRoundedRectPoints(Vector2D position, Vector2D size,
                                                       float radius, unsigned int cornerSegments = 8) const {
        float r = std::max(0.0f, std::min({radius, size.x / 2.0f, size.y / 2.0f}));

        struct Corner { float cx, cy, startDeg; };
        Corner corners[4] = {
            {position.x + r,          position.y + r,          180.0f}, // top-left
            {position.x + size.x - r, position.y + r,          270.0f}, // top-right
            {position.x + size.x - r, position.y + size.y - r,   0.0f}, // bottom-right
            {position.x + r,          position.y + size.y - r,  90.0f}  // bottom-left
        };

        std::vector<sf::Vector2f> points;
        points.reserve(4 * (cornerSegments + 1));

        for (const auto& corner : corners) {
            for (unsigned int i = 0; i <= cornerSegments; ++i) {
                float angle = (corner.startDeg + 90.0f * (static_cast<float>(i) / cornerSegments)) * 3.14159265f / 180.0f;
                points.push_back({
                    corner.cx + r * std::cos(angle),
                    corner.cy + r * std::sin(angle)
                });
            }
        }
        return points;
    }

    // Draws a solid-colored rounded-rect fan. Accepts custom render states so
    // callers (e.g. drawGlow) can request additive blending.
    void drawRoundedFan(Vector2D position, Vector2D size, float radius, Color color,
                         const sf::RenderStates& states = sf::RenderStates::Default) {
        auto points = buildRoundedRectPoints(position, size, radius);
        sf::VertexArray fan(sf::PrimitiveType::TriangleFan, points.size() + 2);
        sf::Vector2f center{position.x + size.x / 2.0f, position.y + size.y / 2.0f};
        sf::Color sfCol = toSfColor(color);

        fan[0].position = center;
        fan[0].color = sfCol;
        for (size_t i = 0; i < points.size(); ++i) {
            fan[i + 1].position = points[i];
            fan[i + 1].color = sfCol;
        }
        fan[points.size() + 1].position = points[0];
        fan[points.size() + 1].color = sfCol;

        m_window.draw(fan, states);
    }

    // Linearly interpolates a color across a multi-stop gradient at position t (0..1).
    static Color sampleGradientStops(const std::vector<GradientStop>& stops, float t) {
        if (stops.empty()) return {0, 0, 0, 255};
        if (t <= stops.front().position) return stops.front().color;
        if (t >= stops.back().position) return stops.back().color;

        for (size_t i = 0; i + 1 < stops.size(); ++i) {
            const auto& a = stops[i];
            const auto& b = stops[i + 1];
            if (t >= a.position && t <= b.position) {
                float span = b.position - a.position;
                float localT = span > 0.0001f ? (t - a.position) / span : 0.0f;
                return {
                    static_cast<std::uint8_t>(a.color.r + (b.color.r - a.color.r) * localT),
                    static_cast<std::uint8_t>(a.color.g + (b.color.g - a.color.g) * localT),
                    static_cast<std::uint8_t>(a.color.b + (b.color.b - a.color.b) * localT),
                    static_cast<std::uint8_t>(a.color.a + (b.color.a - a.color.a) * localT)
                };
            }
        }
        return stops.back().color;
    }

    // Projects a point onto the gradient's axis and normalizes it to [0,1]
    // relative to the rectangle's own extent along that axis.
    static float projectAlongAxis(Vector2D point, Vector2D position, Vector2D size, float angleDegrees) {
        float rad = angleDegrees * 3.14159265f / 180.0f;
        Vector2D dir{std::cos(rad), std::sin(rad)};
        Vector2D center{position.x + size.x / 2.0f, position.y + size.y / 2.0f};
        Vector2D half{size.x / 2.0f, size.y / 2.0f};

        float extent = std::abs(dir.x) * half.x + std::abs(dir.y) * half.y;
        if (extent < 0.0001f) return 0.5f;

        float rel = (point.x - center.x) * dir.x + (point.y - center.y) * dir.y;
        return std::clamp(0.5f + rel / (2.0f * extent), 0.0f, 1.0f);
    }

public:
    SfmlRenderer(sf::RenderWindow& window, std::string defaultFontId = "default_font")
        : m_window(window), m_defaultFontId(std::move(defaultFontId)) {
        
        m_view.setSize({m_logicalRange.x, m_logicalRange.y});
        m_view.setCenter({m_logicalRange.x / 2.0f, m_logicalRange.y / 2.0f});
    }

    void beginFrame() override {
        // ----------------------------------------------------
        // Fix for the ghosting issue when stretching the screen (Pillarbox Ghosting)
        // ----------------------------------------------------
        // 1. Reset the View to default to clear the entire physical window area (including borders)
        m_window.setView(m_window.getDefaultView());
        m_window.clear(sf::Color::Black); // Completely clear the entire buffer

        // 2. Retrieve the current physical window size
        sf::Vector2u windowSize = m_window.getSize();
        float windowWidth = static_cast<float>(windowSize.x);
        float windowHeight = static_cast<float>(windowSize.y);

        // 3. Calculate the perfect aspect ratio (1:1) for the square board
        float targetAspectRatio = 1.0f; 
        float windowAspectRatio = windowWidth / windowHeight;

        float viewportX = 0.0f;
        float viewportY = 0.0f;
        float viewportWidth = 1.0f;
        float viewportHeight = 1.0f;

        if (windowAspectRatio > targetAspectRatio) {
            // The window is too wide -> add black borders on the sides
            viewportWidth = targetAspectRatio / windowAspectRatio;
            viewportX = (1.0f - viewportWidth) / 2.0f;
        } else {
            // The window is too tall -> add black borders on the top and bottom
            viewportHeight = windowAspectRatio / targetAspectRatio;
            viewportY = (1.0f - viewportHeight) / 2.0f;
        }

        // 4. Update the viewport of the current View instance in the Renderer (SFML 3 compatible)
        m_view.setViewport(sf::FloatRect({viewportX, viewportY}, {viewportWidth, viewportHeight}));

        // 5. Set the updated and scaled View in the window for the next draw
        m_window.setView(m_view);
    }

    void clear(Color color) override {
        m_window.clear(toSfColor(color));
    }

    void endFrame() override {
    }

    void presentFrame() override {
        m_window.display();
    }

    bool isWindowOpen() const override {
        return m_window.isOpen();
    }

    void drawRectangle(Vector2D position, Vector2D size, Color color, bool fill) override {
        sf::RectangleShape rect(toSfVec(size));
        rect.setPosition(toSfVec(position));
        
        if (fill) {
            rect.setFillColor(toSfColor(color));
            rect.setOutlineThickness(0.0f);
        } else {
            rect.setFillColor(sf::Color::Transparent);
            rect.setOutlineColor(toSfColor(color));
            rect.setOutlineThickness(1.5f);
        }
        m_window.draw(rect);
    }

    void drawLine(Vector2D start, Vector2D end, Color color, float thickness) override {
        sf::Vector2f p1 = toSfVec(start);
        sf::Vector2f p2 = toSfVec(end);
        sf::Vector2f direction = p2 - p1;
        float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);

        if (length < 0.01f) return;

        sf::RectangleShape line({length, thickness});
        line.setPosition(p1);
        line.setFillColor(toSfColor(color));
        
        float angle = std::atan2(direction.y, direction.x) * 180.0f / 3.14159265f;
        line.setRotation(sf::degrees(angle));

        m_window.draw(line);
    }

    void drawCircle(Vector2D center, float radius, Color color, bool fill) override {
        sf::CircleShape circle(radius);
        circle.setOrigin({radius, radius});
        circle.setPosition(toSfVec(center));

        if (fill) {
            circle.setFillColor(toSfColor(color));
            circle.setOutlineThickness(0.0f);
        } else {
            circle.setFillColor(sf::Color::Transparent);
            circle.setOutlineColor(toSfColor(color));
            circle.setOutlineThickness(1.5f);
        }
        
        circle.setPointCount(30);
        m_window.draw(circle);
    }

    void drawSector(Vector2D center, float radius, float startAngle, float endAngle, Color color, bool fill) override {
        unsigned int points = 30; 
        sf::Color sfCol = toSfColor(color);

        float startRad = startAngle * 3.14159265f / 180.0f;
        float endRad = endAngle * 3.14159265f / 180.0f;
        float angleDiff = endRad - startRad;

        if (fill) {
            sf::VertexArray fan(sf::PrimitiveType::TriangleFan, points + 2);
            fan[0].position = toSfVec(center);
            fan[0].color = sfCol;

            for (unsigned int i = 0; i <= points; ++i) {
                float angle = startRad + angleDiff * (static_cast<float>(i) / points);
                fan[i + 1].position = {
                    center.x + radius * std::cos(angle),
                    center.y + radius * std::sin(angle)
                };
                fan[i + 1].color = sfCol;
            }
            m_window.draw(fan);
        } else {
            sf::VertexArray lines(sf::PrimitiveType::LineStrip, points + 2);
            lines[0].position = toSfVec(center);
            lines[0].color = sfCol;

            for (unsigned int i = 0; i <= points; ++i) {
                float angle = startRad + angleDiff * (static_cast<float>(i) / points);
                lines[i + 1].position = {
                    center.x + radius * std::cos(angle),
                    center.y + radius * std::sin(angle)
                };
                lines[i + 1].color = sfCol;
            }
            m_window.draw(lines);
        }
    }

    void drawSprite(std::string_view assetId,
                    Vector2D position,
                    Vector2D size,
                    float rotationDegrees,
                    const Vector2D* srcOffset,
                    const Vector2D* srcSize) override {
        try {
            auto& asset = m_assetManager.getAsset<SfmlTextureAsset>(assetId);
            sf::Sprite sprite(asset.texture);

            if (srcOffset && srcSize) {
                sprite.setTextureRect(sf::IntRect(
                    {static_cast<int>(srcOffset->x), static_cast<int>(srcOffset->y)},
                    {static_cast<int>(srcSize->x), static_cast<int>(srcSize->y)}
                ));
            }

            sf::FloatRect bounds = sprite.getLocalBounds();
            if (bounds.size.x > 0.0f && bounds.size.y > 0.0f) {
                sprite.setScale({size.x / bounds.size.x, size.y / bounds.size.y});
            }

            // Create a dynamic three-dimensional shadow beneath the pieces
            if (assetId.length() == 2) {
                sf::Sprite shadowSprite = sprite;
                shadowSprite.setColor(sf::Color(0, 0, 0, 90)); 
                
                float shadowOffset = size.x * 0.08f; 
                shadowSprite.setPosition({position.x + shadowOffset, position.y + shadowOffset});
                
                if (std::abs(rotationDegrees) > 0.01f) {
                    shadowSprite.setOrigin({bounds.size.x / 2.0f, bounds.size.y / 2.0f});
                    shadowSprite.move({size.x / 2.0f, size.y / 2.0f});
                    shadowSprite.setRotation(sf::degrees(rotationDegrees));
                }
                m_window.draw(shadowSprite);
            }

            sprite.setPosition(toSfVec(position));
            if (std::abs(rotationDegrees) > 0.01f) {
                sprite.setOrigin({bounds.size.x / 2.0f, bounds.size.y / 2.0f});
                sprite.move({size.x / 2.0f, size.y / 2.0f});
                sprite.setRotation(sf::degrees(rotationDegrees));
            }

            m_window.draw(sprite);
        } catch (...) {
            drawRectangle(position, size, {255, 0, 255, 255}, true);
        }
    }

    void drawText(std::string_view text, Vector2D position, int fontSize, Color color) override {
        try {
            auto& fontAsset = m_assetManager.getAsset<SfmlFontAsset>(m_defaultFontId);
            sf::Text sfText(fontAsset.font);
            sfText.setString(std::string(text));
            sfText.setCharacterSize(static_cast<unsigned int>(fontSize));
            sfText.setFillColor(toSfColor(color));
            sfText.setPosition(toSfVec(position));

            m_window.draw(sfText);
        } catch (...) {
        }
    }

    Vector2D getTargetSize() const override {
        sf::Vector2u size = m_window.getSize();
        return { static_cast<float>(size.x), static_cast<float>(size.y) };
    }

    // ------------------------------------------------------------------
    // Modern UI drawing capabilities
    // ------------------------------------------------------------------

    void drawRoundedRectangle(Vector2D position, Vector2D size, float radius,
                               Color color, bool fill) override {
        if (fill) {
            drawRoundedFan(position, size, radius, color);
            return;
        }

        auto points = buildRoundedRectPoints(position, size, radius);
        sf::VertexArray outline(sf::PrimitiveType::LineStrip, points.size() + 1);
        sf::Color sfCol = toSfColor(color);

        for (size_t i = 0; i < points.size(); ++i) {
            outline[i].position = points[i];
            outline[i].color = sfCol;
        }
        outline[points.size()].position = points[0];
        outline[points.size()].color = sfCol;

        m_window.draw(outline);
    }

    void drawGradientRect(Vector2D position, Vector2D size,
                           const std::vector<GradientStop>& stops,
                           float angleDegrees, float cornerRadius) override {
        if (stops.empty()) return;

        auto points = buildRoundedRectPoints(position, size, cornerRadius);
        sf::VertexArray fan(sf::PrimitiveType::TriangleFan, points.size() + 2);
        Vector2D center{position.x + size.x / 2.0f, position.y + size.y / 2.0f};

        auto colorAt = [&](Vector2D p) {
            return toSfColor(sampleGradientStops(stops, projectAlongAxis(p, position, size, angleDegrees)));
        };

        fan[0].position = toSfVec(center);
        fan[0].color = colorAt(center);

        for (size_t i = 0; i < points.size(); ++i) {
            fan[i + 1].position = points[i];
            fan[i + 1].color = colorAt({points[i].x, points[i].y});
        }
        fan[points.size() + 1].position = points[0];
        fan[points.size() + 1].color = fan[1].color;

        m_window.draw(fan);
    }

    void drawRectShadow(Vector2D position, Vector2D size, float cornerRadius,
                         Color shadowColor, float blurRadius, Vector2D offset) override {
        // "Poor man's blur": several overlapping rounded-rect layers, growing
        // in size and fading in alpha, approximate a soft shadow without a
        // shader pass. Cheap, and good enough for UI-scale shadows.
        const int layers = 8;
        Vector2D shadowPos{position.x + offset.x, position.y + offset.y};

        for (int i = layers; i >= 1; --i) {
            float t = static_cast<float>(i) / layers;
            float grow = blurRadius * t;
            float alphaFactor = (1.0f - t) * (1.0f - t);

            Vector2D layerPos{shadowPos.x - grow, shadowPos.y - grow};
            Vector2D layerSize{size.x + grow * 2.0f, size.y + grow * 2.0f};
            float layerRadius = cornerRadius + grow;

            Color layerColor = shadowColor;
            layerColor.a = static_cast<std::uint8_t>(std::clamp(shadowColor.a * alphaFactor * 0.5f, 0.0f, 255.0f));

            drawRoundedFan(layerPos, layerSize, layerRadius, layerColor);
        }
    }

    void drawGlow(Vector2D center, Vector2D size, float cornerRadius,
                  Color glowColor, float intensity) override {
        Vector2D position{center.x - size.x / 2.0f, center.y - size.y / 2.0f};
        const int layers = 6;

        sf::RenderStates states;
        states.blendMode = sf::BlendAdd; // additive blend makes it "glow" against the background

        for (int i = layers; i >= 1; --i) {
            float t = static_cast<float>(i) / layers;
            float grow = 12.0f * t * intensity;
            float alphaFactor = (1.0f - t) * (1.0f - t) * intensity;

            Vector2D layerPos{position.x - grow, position.y - grow};
            Vector2D layerSize{size.x + grow * 2.0f, size.y + grow * 2.0f};
            float layerRadius = cornerRadius + grow;

            Color layerColor = glowColor;
            layerColor.a = static_cast<std::uint8_t>(std::clamp(glowColor.a * alphaFactor, 0.0f, 255.0f));

            drawRoundedFan(layerPos, layerSize, layerRadius, layerColor, states);
        }
    }

    void drawGlassPanel(Vector2D position, Vector2D size, float cornerRadius,
                         Color tint, float blurStrength) override {
        // NOTE: this is an approximation, not a true frosted-glass blur.
        // A real blur needs the background already rendered to an offscreen
        // sf::RenderTexture, then a two-pass Gaussian blur shader applied to
        // it before this panel is composited on top. That requires reworking
        // draw order (this renderer currently draws directly to the window),
        // so it's left as a future upgrade. This version fakes the effect
        // with a translucent tint plus a soft light border.
        Color fillTint = tint;
        fillTint.a = static_cast<std::uint8_t>(std::clamp(tint.a * (0.35f + 0.15f * blurStrength), 0.0f, 255.0f));
        drawRoundedFan(position, size, cornerRadius, fillTint);

        Color borderColor{255, 255, 255, static_cast<std::uint8_t>(20 + 60 * std::clamp(blurStrength, 0.0f, 1.0f))};
        drawRoundedRectangle(position, size, cornerRadius, borderColor, false);
    }

    AssetManager& getAssetManager() {
        return m_assetManager;
    }
};