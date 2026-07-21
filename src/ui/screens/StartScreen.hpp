// ui/screens/StartScreen.hpp
#pragma once
#include "ui/screens/BaseScreen.hpp"
#include "ui/framework/ISoundPlayer.hpp"
#include "ui/screens/ChessGameScreen.hpp"
#include "ui/framework/ScreenManager.hpp"
#include <memory>

class StartScreen : public BaseScreen
{
public:
    enum class GameMode { Simultaneous, Classic };
    enum class OpponentType { LocalPlayer, AI, Online };

private:
    Vector2D m_mousePos{0.0f, 0.0f};
    GameMode m_selectedMode{GameMode::Simultaneous};
    OpponentType m_selectedOpponent{OpponentType::AI};
    ChessGameScreen::AiDifficulty m_selectedDifficulty{ChessGameScreen::AiDifficulty::Medium};
    std::shared_ptr<ISoundPlayer> m_soundPlayer;

    // Geometric settings for the launcher interface
    const Vector2D m_panelPos{250.0f, 230.0f};
    const Vector2D m_panelSize{500.0f, 510.0f};

    const Vector2D m_btnSize{380.0f, 50.0f};
    const Vector2D m_modeBtnSize{180.0f, 45.0f};
    const Vector2D m_opponentBtnSize{115.0f, 45.0f};    
    const Vector2D m_difficultyBtnSize{115.0f, 45.0f};  

    // Internal positions within the floating panel
    const Vector2D m_simulModePos{310.0f, 320.0f};
    const Vector2D m_classicModePos{510.0f, 320.0f};

    const Vector2D m_pvpOpponentPos{310.0f, 420.0f};
    const Vector2D m_aiOpponentPos{440.0f, 420.0f};
    const Vector2D m_onlineOpponentPos{570.0f, 420.0f};

    const Vector2D m_easyDifficultyPos{310.0f, 520.0f};
    const Vector2D m_mediumDifficultyPos{440.0f, 520.0f};
    const Vector2D m_hardDifficultyPos{570.0f, 520.0f};

    const Vector2D m_playBtnPos{310.0f, 600.0f};
    const Vector2D m_exitBtnPos{310.0f, 660.0f};
    
    void drawWelcomeMessage(IRenderer &renderer)
    {
        renderer.drawText("KUNG-FU CHESS", {280.0f, 130.0f}, 42, m_theme.titleText);
        renderer.drawText("The Real-Time Chess Experience", {340.0f, 190.0f}, 14, m_theme.bodyText);
    }

    void drawModeSelector(IRenderer &renderer)
    {
        renderer.drawText("Select Game Mode:", {310.0f, 295.0f}, 14, m_theme.bodyText);

        bool simulHovered = isPointInRect(m_mousePos, m_simulModePos, m_modeBtnSize);
        bool classicHovered = isPointInRect(m_mousePos, m_classicModePos, m_modeBtnSize);

        bool isSimulActive = (m_selectedMode == GameMode::Simultaneous);
        Color simulColor = isSimulActive ? m_theme.buttonHover : (simulHovered ? Color{55, 58, 70, 255} : m_theme.buttonNormal);
        renderer.drawRectangle(m_simulModePos, m_modeBtnSize, simulColor, true);
        renderer.drawRectangle(m_simulModePos, m_modeBtnSize, isSimulActive ? Color{255, 255, 255, 180} : m_theme.border, false);
        renderer.drawText("Real-Time", {m_simulModePos.x + 45.0f, m_simulModePos.y + 28.0f}, 14, m_theme.bodyText);

        bool isClassicActive = (m_selectedMode == GameMode::Classic);
        Color classicColor = isClassicActive ? m_theme.buttonHover : (classicHovered ? Color{55, 58, 70, 255} : m_theme.buttonNormal);
        renderer.drawRectangle(m_classicModePos, m_modeBtnSize, classicColor, true);
        renderer.drawRectangle(m_classicModePos, m_modeBtnSize, isClassicActive ? Color{255, 255, 255, 180} : m_theme.border, false);
        renderer.drawText("Classic", {m_classicModePos.x + 55.0f, m_classicModePos.y + 28.0f}, 14, m_theme.bodyText);
    }

    void drawOpponentSelector(IRenderer &renderer)
    {
        renderer.drawText("Select Opponent:", {310.0f, 395.0f}, 14, m_theme.bodyText);

        bool pvpHovered = isPointInRect(m_mousePos, m_pvpOpponentPos, m_opponentBtnSize);
        bool aiHovered = isPointInRect(m_mousePos, m_aiOpponentPos, m_opponentBtnSize);
        bool onlineHovered = isPointInRect(m_mousePos, m_onlineOpponentPos, m_opponentBtnSize);

        bool isPvpActive = (m_selectedOpponent == OpponentType::LocalPlayer);
        Color pvpColor = isPvpActive ? m_theme.buttonHover : (pvpHovered ? Color{55, 58, 70, 255} : m_theme.buttonNormal);
        renderer.drawRectangle(m_pvpOpponentPos, m_opponentBtnSize, pvpColor, true);
        renderer.drawRectangle(m_pvpOpponentPos, m_opponentBtnSize, isPvpActive ? Color{255, 255, 255, 180} : m_theme.border, false);
        renderer.drawText("Local", {m_pvpOpponentPos.x + 35.0f, m_pvpOpponentPos.y + 28.0f}, 14, m_theme.bodyText);

        bool isAiActive = (m_selectedOpponent == OpponentType::AI);
        Color aiColor = isAiActive ? m_theme.buttonHover : (aiHovered ? Color{55, 58, 70, 255} : m_theme.buttonNormal);
        renderer.drawRectangle(m_aiOpponentPos, m_opponentBtnSize, aiColor, true);
        renderer.drawRectangle(m_aiOpponentPos, m_opponentBtnSize, isAiActive ? Color{255, 255, 255, 180} : m_theme.border, false);
        renderer.drawText("vs AI", {m_aiOpponentPos.x + 35.0f, m_aiOpponentPos.y + 28.0f}, 14, m_theme.bodyText);

        bool isOnlineActive = (m_selectedOpponent == OpponentType::Online);
        Color onlineColor = isOnlineActive ? m_theme.buttonHover : (onlineHovered ? Color{55, 58, 70, 255} : m_theme.buttonNormal);
        renderer.drawRectangle(m_onlineOpponentPos, m_opponentBtnSize, onlineColor, true);
        renderer.drawRectangle(m_onlineOpponentPos, m_opponentBtnSize, isOnlineActive ? Color{255, 255, 255, 180} : m_theme.border, false);
        renderer.drawText("Online", {m_onlineOpponentPos.x + 32.0f, m_onlineOpponentPos.y + 28.0f}, 14, m_theme.bodyText);
    }

    void drawDifficultySelector(IRenderer &renderer)
    {
        if (m_selectedOpponent != OpponentType::AI) return;

        renderer.drawText("AI Difficulty:", {310.0f, 495.0f}, 14, m_theme.bodyText);

        bool easyHovered = isPointInRect(m_mousePos, m_easyDifficultyPos, m_difficultyBtnSize);
        bool mediumHovered = isPointInRect(m_mousePos, m_mediumDifficultyPos, m_difficultyBtnSize);
        bool hardHovered = isPointInRect(m_mousePos, m_hardDifficultyPos, m_difficultyBtnSize);

        bool isEasyActive = (m_selectedDifficulty == ChessGameScreen::AiDifficulty::Easy);
        Color easyColor = isEasyActive ? m_theme.buttonHover : (easyHovered ? Color{55, 58, 70, 255} : m_theme.buttonNormal);
        renderer.drawRectangle(m_easyDifficultyPos, m_difficultyBtnSize, easyColor, true);
        renderer.drawRectangle(m_easyDifficultyPos, m_difficultyBtnSize, isEasyActive ? Color{255, 255, 255, 180} : m_theme.border, false);
        renderer.drawText("Easy", {m_easyDifficultyPos.x + 38.0f, m_easyDifficultyPos.y + 28.0f}, 14, m_theme.bodyText);

        bool isMediumActive = (m_selectedDifficulty == ChessGameScreen::AiDifficulty::Medium);
        Color mediumColor = isMediumActive ? m_theme.buttonHover : (mediumHovered ? Color{55, 58, 70, 255} : m_theme.buttonNormal);
        renderer.drawRectangle(m_mediumDifficultyPos, m_difficultyBtnSize, mediumColor, true);
        renderer.drawRectangle(m_mediumDifficultyPos, m_difficultyBtnSize, isMediumActive ? Color{255, 255, 255, 180} : m_theme.border, false);
        renderer.drawText("Med", {m_mediumDifficultyPos.x + 40.0f, m_mediumDifficultyPos.y + 28.0f}, 14, m_theme.bodyText);

        bool isHardActive = (m_selectedDifficulty == ChessGameScreen::AiDifficulty::Hard);
        Color hardColor = isHardActive ? m_theme.buttonHover : (hardHovered ? Color{55, 58, 70, 255} : m_theme.buttonNormal);
        renderer.drawRectangle(m_hardDifficultyPos, m_difficultyBtnSize, hardColor, true);
        renderer.drawRectangle(m_hardDifficultyPos, m_difficultyBtnSize, isHardActive ? Color{255, 255, 255, 180} : m_theme.border, false);
        renderer.drawText("Hard", {m_hardDifficultyPos.x + 38.0f, m_hardDifficultyPos.y + 28.0f}, 14, m_theme.bodyText);
    }

    void drawMenuButtons(IRenderer &renderer)
    {
        bool playHovered = isPointInRect(m_mousePos, m_playBtnPos, m_btnSize);
        bool exitHovered = isPointInRect(m_mousePos, m_exitBtnPos, m_btnSize);

        drawButton(renderer, "             Start Game", m_playBtnPos, m_btnSize, playHovered);
        drawButton(renderer, "                Exit", m_exitBtnPos, m_btnSize, exitHovered);
    }

protected:
    void drawContent(IRenderer &renderer) override
    {
        drawWelcomeMessage(renderer);
        
        // Draw a central glass panel for the launcher
        drawGlassPanel(renderer, m_panelPos, m_panelSize);
        
        drawModeSelector(renderer);
        drawOpponentSelector(renderer);
        drawDifficultySelector(renderer);
        drawMenuButtons(renderer);
    }

public:
    explicit StartScreen(ScreenManager &manager, std::shared_ptr<ISoundPlayer> soundPlayer = std::make_shared<NullSoundPlayer>())
        : BaseScreen(manager, ""), m_soundPlayer(std::move(soundPlayer))
    {
        m_theme.background = Color{12, 13, 17, 255};
        m_theme.titleText = Color{240, 200, 80, 255};
        m_theme.buttonNormal = Color{35, 37, 45, 255};
        m_theme.buttonHover = Color{48, 120, 192, 255};
        m_theme.border = Color{55, 58, 70, 255};
        m_theme.bodyText = Color{210, 215, 225, 255};
    }

    void onEnter() override {}
    void onExit() override {}

    void update(float deltaTime) override { 
        tickBackground(deltaTime); 
        (void)deltaTime; }

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
                    else if (isPointInRect(m_mousePos, m_pvpOpponentPos, m_opponentBtnSize))
                    {
                        m_selectedOpponent = OpponentType::LocalPlayer;
                    }
                    else if (isPointInRect(m_mousePos, m_aiOpponentPos, m_opponentBtnSize))
                    {
                        m_selectedOpponent = OpponentType::AI;
                    }
                    else if (isPointInRect(m_mousePos, m_onlineOpponentPos, m_opponentBtnSize))
                    {
                        m_selectedOpponent = OpponentType::Online;
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
                        bool isNetworkMode = (m_selectedOpponent == OpponentType::Online);

                        m_screenManager.pushScreen(std::make_unique<ChessGameScreen>(
                            m_screenManager, isSimultaneous, isAiOpponent, m_selectedDifficulty, m_soundPlayer, isNetworkMode));
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