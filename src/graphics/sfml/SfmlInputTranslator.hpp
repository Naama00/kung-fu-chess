#pragma once
#include "ui/framework/IInputTranslator.hpp"
#include "ui/framework/InputEvents.hpp"
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <vector>
#include <optional>

class SfmlInputTranslator : public IInputTranslator {
private:
    sf::RenderWindow& m_window;

    static Key translateKey(sf::Keyboard::Key sfKey) {
        switch (sfKey) {
            case sf::Keyboard::Key::Escape: return Key::Escape;
            case sf::Keyboard::Key::Space:  return Key::Space;
            case sf::Keyboard::Key::W:      return Key::W;
            case sf::Keyboard::Key::S:      return Key::S;
            case sf::Keyboard::Key::A:      return Key::A;
            case sf::Keyboard::Key::D:      return Key::D;
            case sf::Keyboard::Key::Left:   return Key::Left;
            case sf::Keyboard::Key::Right:  return Key::Right;
            default:                        return Key::Unknown;
        }
    }

    static MouseButton translateMouseButton(sf::Mouse::Button sfButton) {
        switch (sfButton) {
            case sf::Mouse::Button::Left:   return MouseButton::Left;
            case sf::Mouse::Button::Right:  return MouseButton::Right;
            case sf::Mouse::Button::Middle: return MouseButton::Middle;
            default:                return MouseButton::None;
        }
    }

public:
    explicit SfmlInputTranslator(sf::RenderWindow& window) : m_window(window) {}

    void pollEvents(std::vector<InputEvent>& outEvents) override {
        // ב-SFML 3 פונקציית pollEvent מחזירה std::optional<sf::Event>
        while (const std::optional<sf::Event> event = m_window.pollEvent()) {
            
            // 1. טיפול בסגירת חלון
            if (event->is<sf::Event::Closed>()) {
                m_window.close();
                continue;
            }

            // 2. טיפול בשינוי גודל חלון (ה-Viewport מנוהל אוטומטית ובאופן דינמי ב-Renderer)
            if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                continue;
            }

            // 3. אירועי עכבר - לחיצה ושחרור כפתור
            if (const auto* mouseButtonPressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                InputEvent ie;
                ie.type = InputEvent::Type::Mouse;
                ie.mouse.action = MouseEvent::Action::Press;
                ie.mouse.button = translateMouseButton(mouseButtonPressed->button);

                sf::Vector2i pixelPos(mouseButtonPressed->position.x, mouseButtonPressed->position.y);
                sf::Vector2f logicalPos = m_window.mapPixelToCoords(pixelPos);
                ie.mouse.logicalX = logicalPos.x;
                ie.mouse.logicalY = logicalPos.y;

                outEvents.push_back(ie);
            }
            else if (const auto* mouseButtonReleased = event->getIf<sf::Event::MouseButtonReleased>()) {
                InputEvent ie;
                ie.type = InputEvent::Type::Mouse;
                ie.mouse.action = MouseEvent::Action::Release;
                ie.mouse.button = translateMouseButton(mouseButtonReleased->button);

                sf::Vector2i pixelPos(mouseButtonReleased->position.x, mouseButtonReleased->position.y);
                sf::Vector2f logicalPos = m_window.mapPixelToCoords(pixelPos);
                ie.mouse.logicalX = logicalPos.x;
                ie.mouse.logicalY = logicalPos.y;

                outEvents.push_back(ie);
            }
            // אירוע תנועת עכבר
            else if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>()) {
                InputEvent ie;
                ie.type = InputEvent::Type::Mouse;
                ie.mouse.action = MouseEvent::Action::Move;
                ie.mouse.button = MouseButton::None;

                sf::Vector2i pixelPos(mouseMoved->position.x, mouseMoved->position.y);
                sf::Vector2f logicalPos = m_window.mapPixelToCoords(pixelPos);
                ie.mouse.logicalX = logicalPos.x;
                ie.mouse.logicalY = logicalPos.y;

                outEvents.push_back(ie);
            }

            // 4. אירועי מקלדת
            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                InputEvent ie;
                ie.type = InputEvent::Type::Keyboard;
                ie.key.rawCode = static_cast<int>(keyPressed->code);
                ie.key.key = translateKey(keyPressed->code);
                
                outEvents.push_back(ie);
            }
        }
    }
};