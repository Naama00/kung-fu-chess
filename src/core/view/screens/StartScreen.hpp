#pragma once
#include "ui/framework/BaseScreen.hpp"
#include "ui/framework/ISoundPlayer.hpp"
#include "ChessGameScreen.hpp"
#include "ui/framework/ScreenManager.hpp"
#include <memory>

class StartScreen : public BaseScreen
{
public:
    enum class GameMode
    {
        Simultaneous, // קונג-פו שחמט (זמן אמת)
        Classic       // שחמט רגיל
    };

private:
    Vector2D m_mousePos{0.0f, 0.0f};
    GameMode m_selectedMode{GameMode::Simultaneous};
    std::shared_ptr<ISoundPlayer> m_soundPlayer;
    // הגדרות גיאומטריות של כפתורי הבחירה והפעולה
    const Vector2D m_btnSize{260.0f, 55.0f};
    const Vector2D m_modeBtnSize{125.0f, 45.0f};

    const Vector2D m_simulModePos{365.0f, 380.0f};
    const Vector2D m_classicModePos{500.0f, 380.0f};
    const Vector2D m_playBtnPos{365.0f, 460.0f};
    const Vector2D m_exitBtnPos{365.0f, 530.0f};

    void drawWelcomeMessage(IRenderer &renderer)
    {
        renderer.drawText("KUNG-FU CHESS", {280.0f, 220.0f}, 42, m_theme.titleText);
        renderer.drawText("The Real-Time Chess Experience", {310.0f, 280.0f}, 18, m_theme.bodyText);
    }

    void drawModeSelector(IRenderer &renderer)
    {
        renderer.drawText("Select Game Mode:", {365.0f, 350.0f}, 16, m_theme.bodyText);

        bool simulHovered = isPointInRect(m_mousePos, m_simulModePos, m_modeBtnSize);
        bool classicHovered = isPointInRect(m_mousePos, m_classicModePos, m_modeBtnSize);

        // כפתור מצב סימולטני
        bool isSimulActive = (m_selectedMode == GameMode::Simultaneous);
        Color simulColor = isSimulActive ? m_theme.buttonHover : (simulHovered ? Color{55, 58, 70, 255} : m_theme.buttonNormal);
        renderer.drawRectangle(m_simulModePos, m_modeBtnSize, simulColor, true);
        renderer.drawRectangle(m_simulModePos, m_modeBtnSize, m_theme.border, false);
        renderer.drawText("Real-Time", {m_simulModePos.x + 15.0f, m_simulModePos.y + 28.0f}, 14, m_theme.bodyText);

        // כפתור מצב קלאסי
        bool isClassicActive = (m_selectedMode == GameMode::Classic);
        Color classicColor = isClassicActive ? m_theme.buttonHover : (classicHovered ? Color{55, 58, 70, 255} : m_theme.buttonNormal);
        renderer.drawRectangle(m_classicModePos, m_modeBtnSize, classicColor, true);
        renderer.drawRectangle(m_classicModePos, m_modeBtnSize, m_theme.border, false);
        renderer.drawText("Classic", {m_classicModePos.x + 25.0f, m_classicModePos.y + 28.0f}, 14, m_theme.bodyText);
    }

    void drawMenuButtons(IRenderer &renderer)
    {
        bool playHovered = isPointInRect(m_mousePos, m_playBtnPos, m_btnSize);
        bool exitHovered = isPointInRect(m_mousePos, m_exitBtnPos, m_btnSize);

        drawButton(renderer, "Start Game", m_playBtnPos, m_btnSize, playHovered);
        drawButton(renderer, "Exit", m_exitBtnPos, m_btnSize, exitHovered);
    }

protected:
    void drawContent(IRenderer &renderer) override
    {
        drawWelcomeMessage(renderer);
        drawModeSelector(renderer);
        drawMenuButtons(renderer);
    }

public:
    explicit StartScreen(ScreenManager &manager, std::shared_ptr<ISoundPlayer> soundPlayer = std::make_shared<NullSoundPlayer>())
        : BaseScreen(manager, "Game Launcher"), m_soundPlayer(std::move(soundPlayer))
    {
        // התאמת ערכת הנושא של מסך הפתיחה למראה יוקרתי ונקי עם ניגודיות גבוהה
        m_theme.background = Color{18, 19, 23, 255};    // רקע כהה עמוק
        m_theme.titleText = Color{240, 200, 80, 255};   // זהב חם לכותרות
        m_theme.buttonNormal = Color{35, 37, 45, 255};  // פחם כהה לכפתורים רגילים
        m_theme.buttonHover = Color{48, 120, 192, 255}; // כחול רויאל מודגש ב-Hover
        m_theme.border = Color{55, 58, 70, 255};        // מסגרות עדינות
        m_theme.bodyText = Color{210, 215, 225, 255};   // טקסט קריא בהיר
    }

    void onEnter() override {}
    void onExit() override {}

    void update(float deltaTime) override
    {
        (void)deltaTime;
    }

    void handleInput(const std::vector<InputEvent> &events) override
    {
        for (const auto &event : events)
        {
            if (event.type == InputEvent::Type::Mouse)
            {
                m_mousePos = {event.mouse.logicalX, event.mouse.logicalY};

                if (event.mouse.action == MouseEvent::Action::Press && event.mouse.button == MouseButton::Left)
                {
                    if (isPointInRect(m_mousePos, m_simulModePos, m_modeBtnSize))
                    {
                        m_selectedMode = GameMode::Simultaneous;
                    }
                    else if (isPointInRect(m_mousePos, m_classicModePos, m_modeBtnSize))
                    {
                        m_selectedMode = GameMode::Classic;
                    }
                    else if (isPointInRect(m_mousePos, m_playBtnPos, m_btnSize))
                    {
                        bool isSimultaneous = (m_selectedMode == GameMode::Simultaneous);
                        m_screenManager.pushScreen(std::make_unique<ChessGameScreen>(m_screenManager, isSimultaneous, m_soundPlayer));
                    }
                    else if (isPointInRect(m_mousePos, m_exitBtnPos, m_btnSize))
                    {
                        m_screenManager.popScreen();
                    }
                }
            }
        }
    }
};