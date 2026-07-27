// ui/screens/StartScreen.cpp
#include "ui/screens/StartScreen.hpp"
#include "ui/framework/ScreenManager.hpp"
#include "players/network/ClientAuth.hpp"
#include <iostream>

StartScreen::StartScreen(ScreenManager &manager,
                         std::shared_ptr<ISoundPlayer> soundPlayer,
                         std::shared_ptr<kungfu::NetworkSession> authSession)
    : BaseScreen(manager, ""), m_soundPlayer(std::move(soundPlayer)), m_authSession(std::move(authSession))
{
    m_theme.background = Color{12, 13, 17, 255};
    m_theme.titleText = Color{240, 200, 80, 255};
    m_theme.buttonNormal = Color{35, 37, 45, 255};
    m_theme.buttonHover = Color{48, 120, 192, 255};
    m_theme.border = Color{55, 58, 70, 255};
    m_theme.bodyText = Color{210, 215, 225, 255};
}

void StartScreen::drawWelcomeMessage(IRenderer &renderer)
{
    renderer.drawText("KUNG-FU CHESS", {280.0f, 130.0f}, 42, m_theme.titleText);
    renderer.drawText("The Real-Time Chess Experience", {340.0f, 190.0f}, 14, m_theme.bodyText);
}

void StartScreen::drawModeSelector(IRenderer &renderer)
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

void StartScreen::drawOpponentSelector(IRenderer &renderer)
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

void StartScreen::drawDifficultySelector(IRenderer &renderer)
{
    if (m_selectedOpponent != OpponentType::AI)
        return;

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

void StartScreen::drawOnlineRoleSelector(IRenderer &renderer)
{
    if (m_selectedOpponent != OpponentType::Online)
        return;

    renderer.drawText("Online Mode:", {310.0f, 495.0f}, 14, m_theme.bodyText);

    bool playHovered = isPointInRect(m_mousePos, m_onlineRolePlayPos, m_onlineRoleBtnSize);
    bool specHovered = isPointInRect(m_mousePos, m_onlineRoleSpecPos, m_onlineRoleBtnSize);

    bool isPlayActive = (m_selectedOnlineRole == OnlineRole::Play);
    Color playColor = isPlayActive ? m_theme.buttonHover : (playHovered ? Color{55, 58, 70, 255} : m_theme.buttonNormal);
    renderer.drawRectangle(m_onlineRolePlayPos, m_onlineRoleBtnSize, playColor, true);
    renderer.drawRectangle(m_onlineRolePlayPos, m_onlineRoleBtnSize, isPlayActive ? Color{255, 255, 255, 180} : m_theme.border, false);
    renderer.drawText("Play Game", {m_onlineRolePlayPos.x + 45.0f, m_onlineRolePlayPos.y + 28.0f}, 14, m_theme.bodyText);

    bool isSpecActive = (m_selectedOnlineRole == OnlineRole::Spectate);
    Color specColor = isSpecActive ? m_theme.buttonHover : (specHovered ? Color{55, 58, 70, 255} : m_theme.buttonNormal);
    renderer.drawRectangle(m_onlineRoleSpecPos, m_onlineRoleBtnSize, specColor, true);
    renderer.drawRectangle(m_onlineRoleSpecPos, m_onlineRoleBtnSize, isSpecActive ? Color{255, 255, 255, 180} : m_theme.border, false);
    renderer.drawText("Spectate Room", {m_onlineRoleSpecPos.x + 35.0f, m_onlineRoleSpecPos.y + 28.0f}, 14, m_theme.bodyText);

    if (isPlayActive)
    {
        renderer.drawText("Enter Room Code (0 for Random Match):", {310.0f, 570.0f}, 12, m_theme.bodyText);
        Color borderC = m_isRoomCodeActive ? m_theme.buttonHover : m_theme.border;
        renderer.drawRectangle({310.0f, 585.0f}, {380.0f, 35.0f}, {18, 19, 23, 255}, true);
        renderer.drawRectangle({310.0f, 585.0f}, {380.0f, 35.0f}, borderC, false);
        renderer.drawText(m_onlineRoomCodeText, {325.0f, 608.0f}, 14, {255, 255, 255, 255});
    }
    else if (isSpecActive)
    {
        renderer.drawText("Select Live Match to Spectate:", {310.0f, 570.0f}, 12, m_theme.bodyText);

        if (m_liveRooms.empty())
        {
            renderer.drawRectangle({310.0f, 585.0f}, {380.0f, 35.0f}, {18, 19, 23, 255}, true);
            renderer.drawRectangle({310.0f, 585.0f}, {380.0f, 35.0f}, m_theme.border, false);
            renderer.drawText("No active matches online...", {325.0f, 608.0f}, 11, m_theme.textMuted);
        }
        else
        {
            if (m_selectedRoomIndex >= m_liveRooms.size())
            {
                m_selectedRoomIndex = 0;
            }

            const auto &room = m_liveRooms[m_selectedRoomIndex];
            std::string label = "Match #" + std::to_string(room.matchId) + ": " + room.whitePlayer + " vs. " + room.blackPlayer;

            bool isRoomSelected = (m_selectedSpectateRoomId == room.matchId);
            Color btnBg = isRoomSelected ? m_theme.buttonHover : (isPointInRect(m_mousePos, Vector2D{310.0f, 585.0f}, Vector2D{380.0f, 35.0f}) ? Color{55, 58, 70, 255} : Color{18, 19, 23, 255});

            renderer.drawRectangle({310.0f, 585.0f}, {380.0f, 35.0f}, btnBg, true);
            renderer.drawRectangle({310.0f, 585.0f}, {380.0f, 35.0f}, isRoomSelected ? Color{255, 255, 255, 180} : m_theme.border, false);
            renderer.drawText(label, {322.0f, 608.0f}, 10, m_theme.bodyText);

            if (m_liveRooms.size() > 1)
            {
                renderer.drawText("(Click to cycle through " + std::to_string(m_liveRooms.size()) + " matches)", {310.0f, 625.0f}, 10, m_theme.textMuted);
            }
        }
    }
}

void StartScreen::drawMenuButtons(IRenderer &renderer)
{
    bool playHovered = isPointInRect(m_mousePos, m_playBtnPos, m_btnSize);
    bool exitHovered = isPointInRect(m_mousePos, m_exitBtnPos, m_btnSize);

    drawButton(renderer, "             Start Game", m_playBtnPos, m_btnSize, playHovered);
    drawButton(renderer, "                Exit", m_exitBtnPos, m_btnSize, exitHovered);
}

void StartScreen::drawContent(IRenderer &renderer)
{
    drawWelcomeMessage(renderer);
    drawGlassPanel(renderer, m_panelPos, m_panelSize);
    drawModeSelector(renderer);
    drawOpponentSelector(renderer);

    if (m_selectedOpponent == OpponentType::AI)
    {
        drawDifficultySelector(renderer);
    }
    else if (m_selectedOpponent == OpponentType::Online)
    {
        drawOnlineRoleSelector(renderer);
    }

    drawMenuButtons(renderer);
}

void StartScreen::onEnter() {}
void StartScreen::onExit() {}

void StartScreen::update(float deltaTime)
{
    tickBackground(deltaTime);

    if (m_authSession && m_authSession->player && m_authSession->player->hasMatchStarted())
    {
        m_screenManager.pushScreen(std::make_unique<ChessGameScreen>(
            m_screenManager,
            (m_selectedMode == GameMode::Simultaneous),
            m_soundPlayer,
            m_authSession,
            0));
        return;
    }

    if (m_selectedOpponent == OpponentType::Online && m_selectedOnlineRole == OnlineRole::Spectate)
    {
        if (!m_lobbyService)
        {
            m_lobbyService = std::make_unique<kungfu::LobbyService>();
        }
        m_lobbyService->update(deltaTime);
        m_liveRooms = m_lobbyService->getActiveRooms();
    }
    else
    {
        if (m_lobbyService)
        {
            m_lobbyService->stop();
            m_lobbyService.reset();
        }
        m_liveRooms.clear();
    }
}

void StartScreen::handleInput(const std::vector<InputEvent> &events)
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
                else if (m_selectedOpponent == OpponentType::Online && isPointInRect(m_mousePos, m_onlineRolePlayPos, m_onlineRoleBtnSize))
                {
                    m_selectedOnlineRole = OnlineRole::Play;
                }
                else if (m_selectedOpponent == OpponentType::Online && isPointInRect(m_mousePos, m_onlineRoleSpecPos, m_onlineRoleBtnSize))
                {
                    m_selectedOnlineRole = OnlineRole::Spectate;
                }
                else if (m_selectedOpponent == OpponentType::Online && m_selectedOnlineRole == OnlineRole::Play &&
                         isPointInRect(m_mousePos, {310.0f, 585.0f}, {380.0f, 35.0f}))
                {
                    m_isRoomCodeActive = true;
                }
                else if (m_selectedOpponent == OpponentType::Online && m_selectedOnlineRole == OnlineRole::Spectate &&
                         !m_liveRooms.empty() && isPointInRect(m_mousePos, {310.0f, 585.0f}, {380.0f, 35.0f}))
                {
                    if (m_selectedRoomIndex >= m_liveRooms.size())
                    {
                        m_selectedRoomIndex = 0;
                    }
                    m_selectedSpectateRoomId = m_liveRooms[m_selectedRoomIndex].matchId;
                    m_isRoomCodeActive = false;
                    m_selectedRoomIndex = (m_selectedRoomIndex + 1) % m_liveRooms.size();
                }
                else
                {
                    m_isRoomCodeActive = false;
                }

                if (isPointInRect(m_mousePos, m_playBtnPos, m_btnSize))
                {
                    bool isSimultaneous = (m_selectedMode == GameMode::Simultaneous);
                    bool isAiOpponent = (m_selectedOpponent == OpponentType::AI);
                    bool isNetworkMode = (m_selectedOpponent == OpponentType::Online);
                    bool isSpectator = (isNetworkMode && m_selectedOnlineRole == OnlineRole::Spectate);

                    std::uint64_t spectateMatchId = isSpectator ? m_selectedSpectateRoomId : 0;

                    std::uint64_t roomCode = 0;
                    try
                    {
                        if (!m_onlineRoomCodeText.empty())
                        {
                            roomCode = std::stoull(m_onlineRoomCodeText);
                        }
                    }
                    catch (...)
                    {
                        roomCode = 0;
                    }

                    if (isNetworkMode && !isSpectator && m_authSession)
                    {
                        if (m_authSession->player)
                        {
                            m_authSession->player->resetMatchState();
                        }
                        m_screenManager.pushScreen(std::make_unique<ChessGameScreen>(
                            m_screenManager,
                            isSimultaneous,
                            m_soundPlayer,
                            m_authSession,
                            roomCode));
                    }
                    else
                    {
                        m_screenManager.pushScreen(std::make_unique<ChessGameScreen>(
                            m_screenManager,
                            isSimultaneous,
                            isAiOpponent,
                            m_selectedDifficulty,
                            m_soundPlayer,
                            isNetworkMode,
                            kungfu::ClientConfig::getHost(),
                            kungfu::ClientConfig::getPort(),
                            isSpectator,
                            spectateMatchId,
                            roomCode));
                    }
                }
                else if (isPointInRect(m_mousePos, m_exitBtnPos, m_btnSize))
                {
                    m_screenManager.popScreen();
                }
            }
        }
        else if (event.type == InputEvent::Type::Keyboard)
        {
            if (m_isRoomCodeActive)
            {
                if (event.key.key == Key::Backspace)
                {
                    if (!m_onlineRoomCodeText.empty())
                    {
                        m_onlineRoomCodeText.pop_back();
                    }
                }
                else
                {
                    char c = '\0';
                    int code = event.key.rawCode;

                    char rawChar = static_cast<char>(code & 0xFF);
                    if (rawChar >= '0' && rawChar <= '9')
                    {
                        c = rawChar;
                    }

                    if (c != '\0' && m_onlineRoomCodeText.length() < 10)
                    {
                        if (m_onlineRoomCodeText == "0")
                            m_onlineRoomCodeText = "";
                        m_onlineRoomCodeText += c;
                    }
                }
            }
        }
    }
}