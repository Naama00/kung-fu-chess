// ui/screens/StartScreen.hpp
#pragma once

#include "ui/screens/BaseScreen.hpp"
#include "ui/framework/ISoundPlayer.hpp"
#include "ui/screens/ChessGameScreen.hpp"
#include "players/network/NetworkPlayer.hpp"
#include "players/network/NetworkSession.hpp"
#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <vector>
#include <thread>

class StartScreen : public BaseScreen {
public:
    enum class GameMode { Simultaneous, Classic };
    enum class OpponentType { LocalPlayer, AI, Online };
    enum class OnlineRole { Play, Spectate };

    explicit StartScreen(ScreenManager &manager, 
                         std::shared_ptr<ISoundPlayer> soundPlayer = std::make_shared<NullSoundPlayer>(),
                         std::shared_ptr<kungfu::NetworkSession> authSession = nullptr);

    ~StartScreen() override;

    void onEnter() override;
    void onExit() override;
    void update(float deltaTime) override;
    void handleInput(const std::vector<InputEvent> &events) override;

protected:
    void drawContent(IRenderer &renderer) override;

private:
    void drawWelcomeMessage(IRenderer &renderer);
    void drawModeSelector(IRenderer &renderer);
    void drawOpponentSelector(IRenderer &renderer);
    void drawDifficultySelector(IRenderer &renderer);
    void drawOnlineRoleSelector(IRenderer &renderer);
    void drawMenuButtons(IRenderer &renderer);

    Vector2D m_mousePos{0.0f, 0.0f};
    GameMode m_selectedMode{GameMode::Simultaneous};
    OpponentType m_selectedOpponent{OpponentType::AI};
    ChessGameScreen::AiDifficulty m_selectedDifficulty{ChessGameScreen::AiDifficulty::Medium};

    OnlineRole m_selectedOnlineRole{OnlineRole::Play};
    std::string m_onlineRoomCodeText = "0";
    bool m_isRoomCodeActive = false;

    std::uint64_t m_selectedSpectateRoomId = 0;
    std::size_t m_selectedRoomIndex = 0;
    std::shared_ptr<kungfu::NetworkPlayer> m_lobbyNetPlayer;
    boost::asio::io_context m_lobbyIoContext;
    std::thread m_lobbyNetThread;
    std::vector<kungfu::NetworkPlayer::ClientMatchInfo> m_liveRooms;
    float m_lobbyQueryTimer = 0.0f;

    std::shared_ptr<ISoundPlayer> m_soundPlayer;
    std::shared_ptr<kungfu::NetworkSession> m_authSession;

    const Vector2D m_panelPos{250.0f, 230.0f};
    const Vector2D m_panelSize{500.0f, 510.0f};

    const Vector2D m_btnSize{380.0f, 50.0f};
    const Vector2D m_modeBtnSize{180.0f, 45.0f};
    const Vector2D m_opponentBtnSize{115.0f, 45.0f};
    const Vector2D m_difficultyBtnSize{115.0f, 45.0f};
    const Vector2D m_onlineRoleBtnSize{180.0f, 45.0f};

    const Vector2D m_simulModePos{310.0f, 320.0f};
    const Vector2D m_classicModePos{510.0f, 320.0f};

    const Vector2D m_pvpOpponentPos{310.0f, 420.0f};
    const Vector2D m_aiOpponentPos{440.0f, 420.0f};
    const Vector2D m_onlineOpponentPos{570.0f, 420.0f};

    const Vector2D m_easyDifficultyPos{310.0f, 520.0f};
    const Vector2D m_mediumDifficultyPos{440.0f, 520.0f};
    const Vector2D m_hardDifficultyPos{570.0f, 520.0f};

    const Vector2D m_onlineRolePlayPos{310.0f, 520.0f};
    const Vector2D m_onlineRoleSpecPos{510.0f, 520.0f};

    const Vector2D m_playBtnPos{310.0f, 635.0f};
    const Vector2D m_exitBtnPos{310.0f, 690.0f};
};