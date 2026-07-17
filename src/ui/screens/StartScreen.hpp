#pragma once
#include "ui/framework/BaseScreen.hpp"
#include "ui/framework/ISoundPlayer.hpp"
#include "ui/screens/ChessGameScreen.hpp"
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

    enum class OpponentType
    {
        LocalPlayer, // שחקן מקומי (PvP)
        AI           // מחשב (PvC)
    };

private:
    Vector2D m_mousePos{0.0f, 0.0f};
    GameMode m_selectedMode{GameMode::Simultaneous};
    OpponentType m_selectedOpponent{OpponentType::AI};
    ChessGameScreen::AiDifficulty m_selectedDifficulty{ChessGameScreen::AiDifficulty::Medium}; // משתנה רמת קושי
    std::shared_ptr<ISoundPlayer> m_soundPlayer;

    // הגדרות גיאומטריות של כפתורי הבחירה והפעולה
    const Vector2D m_btnSize{260.0f, 55.0f};
    const Vector2D m_modeBtnSize{125.0f, 45.0f};
    const Vector2D m_difficultyBtnSize{80.0f, 45.0f};

    // מיקומי בחירת מצב משחק
    const Vector2D m_simulModePos{365.0f, 320.0f};
    const Vector2D m_classicModePos{500.0f, 320.0f};

    // מיקומי בחירת יריב
    const Vector2D m_pvpOpponentPos{365.0f, 420.0f};
    const Vector2D m_aiOpponentPos{500.0f, 420.0f};

    // מיקומי בחירת רמת קושי
    const Vector2D m_easyDifficultyPos{365.0f, 520.0f};
    const Vector2D m_mediumDifficultyPos{455.0f, 520.0f};
    const Vector2D m_hardDifficultyPos{545.0f, 520.0f};

    // מיקומי כפתורי שליטה ראשיים
    const Vector2D m_playBtnPos{365.0f, 620.0f};
    const Vector2D m_exitBtnPos{365.0f, 690.0f};

    void drawWelcomeMessage(IRenderer &renderer)
    {
        renderer.drawText("KUNG-FU CHESS", {280.0f, 220.0f}, 42, m_theme.titleText);
        renderer.drawText("The Real-Time Chess Experience", {310.0f, 280.0f}, 18, m_theme.bodyText);
    }

    void drawModeSelector(IRenderer &renderer)
    {
        renderer.drawText("Select Game Mode:", {365.0f, 295.0f}, 16, m_theme.bodyText);

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

    void drawOpponentSelector(IRenderer &renderer)
    {
        renderer.drawText("Select Opponent:", {365.0f, 395.0f}, 16, m_theme.bodyText);

        bool pvpHovered = isPointInRect(m_mousePos, m_pvpOpponentPos, m_modeBtnSize);
        bool aiHovered = isPointInRect(m_mousePos, m_aiOpponentPos, m_modeBtnSize);

        // כפתור מצב שחקן מקומי
        bool isPvpActive = (m_selectedOpponent == OpponentType::LocalPlayer);
        Color pvpColor = isPvpActive ? m_theme.buttonHover : (pvpHovered ? Color{55, 58, 70, 255} : m_theme.buttonNormal);
        renderer.drawRectangle(m_pvpOpponentPos, m_modeBtnSize, pvpColor, true);
        renderer.drawRectangle(m_pvpOpponentPos, m_modeBtnSize, m_theme.border, false);
        renderer.drawText("Local PvP", {m_pvpOpponentPos.x + 20.0f, m_pvpOpponentPos.y + 28.0f}, 14, m_theme.bodyText);

        // כפתור מצב שחקן מול ה-AI
        bool isAiActive = (m_selectedOpponent == OpponentType::AI);
        Color aiColor = isAiActive ? m_theme.buttonHover : (aiHovered ? Color{55, 58, 70, 255} : m_theme.buttonNormal);
        renderer.drawRectangle(m_aiOpponentPos, m_modeBtnSize, aiColor, true);
        renderer.drawRectangle(m_aiOpponentPos, m_modeBtnSize, m_theme.border, false);
        renderer.drawText("vs Computer", {m_aiOpponentPos.x + 13.0f, m_aiOpponentPos.y + 28.0f}, 14, m_theme.bodyText);
    }

    void drawDifficultySelector(IRenderer &renderer)
    {
        if (m_selectedOpponent != OpponentType::AI)
            return;

        renderer.drawText("AI Difficulty:", {365.0f, 495.0f}, 16, m_theme.bodyText);

        bool easyHovered = isPointInRect(m_mousePos, m_easyDifficultyPos, m_difficultyBtnSize);
        bool mediumHovered = isPointInRect(m_mousePos, m_mediumDifficultyPos, m_difficultyBtnSize);
        bool hardHovered = isPointInRect(m_mousePos, m_hardDifficultyPos, m_difficultyBtnSize);

        // Easy
        bool isEasyActive = (m_selectedDifficulty == ChessGameScreen::AiDifficulty::Easy);
        Color easyColor = isEasyActive ? m_theme.buttonHover : (easyHovered ? Color{55, 58, 70, 255} : m_theme.buttonNormal);
        renderer.drawRectangle(m_easyDifficultyPos, m_difficultyBtnSize, easyColor, true);
        renderer.drawRectangle(m_easyDifficultyPos, m_difficultyBtnSize, m_theme.border, false);
        renderer.drawText("Easy", {m_easyDifficultyPos.x + 18.0f, m_easyDifficultyPos.y + 28.0f}, 14, m_theme.bodyText);

        // Medium
        bool isMediumActive = (m_selectedDifficulty == ChessGameScreen::AiDifficulty::Medium);
        Color mediumColor = isMediumActive ? m_theme.buttonHover : (mediumHovered ? Color{55, 58, 70, 255} : m_theme.buttonNormal);
        renderer.drawRectangle(m_mediumDifficultyPos, m_difficultyBtnSize, mediumColor, true);
        renderer.drawRectangle(m_mediumDifficultyPos, m_difficultyBtnSize, m_theme.border, false);
        renderer.drawText("Med", {m_mediumDifficultyPos.x + 22.0f, m_mediumDifficultyPos.y + 28.0f}, 14, m_theme.bodyText);

        // Hard
        bool isHardActive = (m_selectedDifficulty == ChessGameScreen::AiDifficulty::Hard);
        Color hardColor = isHardActive ? m_theme.buttonHover : (hardHovered ? Color{55, 58, 70, 255} : m_theme.buttonNormal);
        renderer.drawRectangle(m_hardDifficultyPos, m_difficultyBtnSize, hardColor, true);
        renderer.drawRectangle(m_hardDifficultyPos, m_difficultyBtnSize, m_theme.border, false);
        renderer.drawText("Hard", {m_hardDifficultyPos.x + 18.0f, m_hardDifficultyPos.y + 28.0f}, 14, m_theme.bodyText);
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
        drawOpponentSelector(renderer);
        drawDifficultySelector(renderer);
        drawMenuButtons(renderer);
    }

public:
    explicit StartScreen(ScreenManager &manager, std::shared_ptr<ISoundPlayer> soundPlayer = std::make_shared<NullSoundPlayer>())
        : BaseScreen(manager, "Game Launcher"), m_soundPlayer(std::move(soundPlayer))
    {
        m_theme.background = Color{18, 19, 23, 255};
        m_theme.titleText = Color{240, 200, 80, 255};
        m_theme.buttonNormal = Color{35, 37, 45, 255};
        m_theme.buttonHover = Color{48, 120, 192, 255};
        m_theme.border = Color{55, 58, 70, 255};
        m_theme.bodyText = Color{210, 215, 225, 255};
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
                    else if (isPointInRect(m_mousePos, m_pvpOpponentPos, m_modeBtnSize))
                    {
                        m_selectedOpponent = OpponentType::LocalPlayer;
                    }
                    else if (isPointInRect(m_mousePos, m_aiOpponentPos, m_modeBtnSize))
                    {
                        m_selectedOpponent = OpponentType::AI;
                    }
                    else if (m_selectedOpponent == OpponentType::AI && isPointInRect(m_mousePos, m_easyDifficultyPos, m_difficultyBtnSize))
                    {
                        m_selectedDifficulty = ChessGameScreen::AiDifficulty::Easy;
                    }
                    else if (m_selectedOpponent == OpponentType::AI && isPointInRect(m_mousePos, m_mediumDifficultyPos, m_difficultyBtnSize))
                    {
                        m_selectedDifficulty = ChessGameScreen::AiDifficulty::Medium;
                    }
                    else if (m_selectedOpponent == OpponentType::AI && isPointInRect(m_mousePos, m_hardDifficultyPos, m_difficultyBtnSize))
                    {
                        m_selectedDifficulty = ChessGameScreen::AiDifficulty::Hard;
                    }
                    else if (isPointInRect(m_mousePos, m_playBtnPos, m_btnSize))
                    {
                        bool isSimultaneous = (m_selectedMode == GameMode::Simultaneous);
                        bool isAiOpponent = (m_selectedOpponent == OpponentType::AI);
                        m_screenManager.pushScreen(std::make_unique<ChessGameScreen>(
                            m_screenManager, isSimultaneous, isAiOpponent, m_selectedDifficulty, m_soundPlayer));
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