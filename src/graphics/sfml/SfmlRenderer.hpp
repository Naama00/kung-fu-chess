// graphics/sfml/SfmlRenderer.hpp
#pragma once
#include "ui/framework/IRenderer.hpp"
#include "ui/framework/AssetManager.hpp"
#include "SfmlAssets.hpp"
#include <SFML/Graphics.hpp>
#include <string_view>
#include <memory>
#include <cmath>

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

    AssetManager& getAssetManager() {
        return m_assetManager;
    }
};