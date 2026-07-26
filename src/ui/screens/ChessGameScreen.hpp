// ui/screens/ChessGameScreen.hpp
#pragma once

#include "ui/screens/BaseScreen.hpp"
#include "ui/framework/ScreenManager.hpp"
#include "ui/framework/ISoundPlayer.hpp"
#include "ui/components/Button.hpp"
#include "ui/components/ParticleSystem.hpp"
#include "ui/components/SidebarView.hpp"
#include "ui/components/HeaderView.hpp"
#include "ui/components/FooterView.hpp"
#include "ui/components/BoardView.hpp"
#include "ui/components/GameOverlaysView.hpp"
#include "engine/core/GameEngine.hpp"
#include "players/human/HumanPlayer.hpp"
#include "players/IPlayer.hpp"
#include "players/ai/IAIDecisionStrategy.hpp" // הוסף כדי להכיר את IAIDecisionStrategy
#include "players/network/NetworkPlayer.hpp"
#include "players/network/NetworkSession.hpp"
#include "ui/framework/EventBus.hpp"
#include <boost/asio.hpp>
#include <future>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <optional>

class ChessGameScreen : public BaseScreen {
public:
    enum class AiDifficulty { Easy, Medium, Hard };

    ChessGameScreen(ScreenManager &manager,
                    bool isSimultaneousMode,
                    bool isAiOpponent,
                    AiDifficulty aiDifficulty,
                    std::shared_ptr<ISoundPlayer> soundPlayer = std::make_shared<NullSoundPlayer>(),
                    bool isNetworkMode = false,
                    std::string host = "127.0.0.1",
                    std::string port = "8080",
                    bool isSpectator = false,
                    std::uint64_t spectateMatchId = 0,
                    std::uint64_t onlineRoomCode = 0);

    ChessGameScreen(ScreenManager &manager,
                    bool isSimultaneousMode,
                    std::shared_ptr<ISoundPlayer> soundPlayer,
                    std::shared_ptr<kungfu::NetworkSession> authSession,
                    std::uint64_t onlineRoomCode = 0);

    ChessGameScreen(ScreenManager &manager,
                    bool isSimultaneousMode,
                    bool isAiOpponent = false,
                    std::shared_ptr<ISoundPlayer> soundPlayer = std::make_shared<NullSoundPlayer>());

    ~ChessGameScreen() override;

    void onEnter() override;
    void onExit() override;
    void update(float deltaTime) override;
    void handleInput(const std::vector<InputEvent> &events) override;

protected:
    void drawContent(IRenderer &renderer) override;

private:
    struct BoardPos {
        int row = -1, col = -1;
        bool isValid() const { return row >= 0 && row < 8 && col >= 0 && col < 8; }
        bool operator==(const BoardPos &o) const { return row == o.row && col == o.col; }
        bool operator!=(const BoardPos &o) const { return !(*this == o); }
    };

    struct PieceAnimation {
        bool isJumping = false;
        float jumpTimer = 0.0f;
    };

    void initializeScreen();
    void resetGame();
    void togglePause();
    void handleJump(const kungfu::Position &pos);
    void handleBoardClick(int row, int col);
    void processAiTurn(float deltaTime);
    void spawnCaptureExplosion(const kungfu::Position &boardPos, kungfu::PlayerColor attackerColor);
    void applySyncedBoard(const std::string &boardText);
    void drawOverlays(IRenderer &renderer, const kungfu::view::GameSnapshot &snapshot);

    std::string getPlayerName(kungfu::PlayerColor color) const;
    kungfu::PieceType getPieceTypeAt(const kungfu::Position &pos) const;
    int calculateAbsoluteMaterialScore(const kungfu::view::GameSnapshot &snapshot, kungfu::PlayerColor color) const;
    std::optional<kungfu::PlayerColor> determineWinnerColor(const kungfu::view::GameSnapshot &snapshot) const;
    std::string getMoveNotationString(kungfu::PieceType type, const BoardPos &from, const BoardPos &to) const;
    void addHistoryLog(kungfu::PlayerColor color, const std::string &logText);
    std::unique_ptr<kungfu::IAIDecisionStrategy> createAiStrategy() const;

    // Engine & Gameplay State
    std::shared_ptr<kungfu::GameEngine> m_gameEngine;
    std::shared_ptr<kungfu::HumanPlayer> m_humanPlayer;
    std::shared_ptr<kungfu::IPlayer> m_aiPlayer;
    kungfu::GameConfig m_config;
    std::shared_ptr<ISoundPlayer> m_soundPlayer;
    std::shared_ptr<kungfu::EventBus> m_eventBus;

    int m_whiteScore = 39;
    int m_blackScore = 39;

    // Network State
    bool m_isNetworkMode = false;
    bool m_wasMatchStarted = false;
    std::shared_ptr<kungfu::NetworkPlayer> m_networkPlayer;
    std::shared_ptr<kungfu::NetworkSession> m_authSession;
    boost::asio::io_context m_ioContext;
    std::thread m_networkThread;

    float m_searchTimer = 0.0f;
    float m_matchFoundBannerTimer = 0.0f;
    std::unique_ptr<Button> m_cancelMatchmakingButton;

    // AI & Match Settings
    bool m_isPaused = false;
    bool m_isAiOpponent = false;
    AiDifficulty m_aiDifficulty = AiDifficulty::Medium;
    float m_aiDecisionTimer = 1.0f;
    bool m_aiActionPending = false;
    bool m_aiThinking = false;
    std::future<std::vector<kungfu::ActionRequest>> m_aiFuture;

    // UI & Visual Components
    std::vector<std::string> m_whiteHistory, m_blackHistory;
    std::unique_ptr<Button> m_pauseButton, m_sidebarRestartButton, m_sidebarMenuButton, m_rematchButton, m_menuButton;

    const float m_boardStartX = 0.0f, m_boardStartY = 100.0f, m_boardRangeX = 800.0f, m_boardRangeY = 800.0f;

    float m_totalTime = 0.0f, m_lastClickTime = 0.0f;
    BoardPos m_lastClickedTile{-1, -1}, m_hoveredTile{-1, -1};
    bool m_isHovering = false;
    PieceAnimation m_selectedPieceAnim;
    float m_pauseTransitionProgress = 0.0f;

    ParticleSystem m_particleSystem;
    SidebarView m_sidebarView;
    HeaderView m_headerView;
    FooterView m_footerView;
    BoardView m_boardView;
    GameOverlaysView m_overlaysView;
};