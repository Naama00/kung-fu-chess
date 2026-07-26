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
#include "engine/core/MatchController.hpp"
#include "players/network/ClientConfig.hpp"
#include <memory>
#include <string>
#include <vector>
#include <optional>

class ChessGameScreen : public BaseScreen {
public:
    using AiDifficulty = kungfu::AiDifficulty;

    ChessGameScreen(ScreenManager &manager,
                bool isSimultaneousMode,
                bool isAiOpponent,
                AiDifficulty aiDifficulty,
                std::shared_ptr<ISoundPlayer> soundPlayer = std::make_shared<NullSoundPlayer>(),
                bool isNetworkMode = false,
                std::string host = kungfu::ClientConfig::kDefaultHost,
                std::string port = kungfu::ClientConfig::kDefaultPort,
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

    ~ChessGameScreen() override = default;

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
    void subscribeToEvents();
    void handleBoardClick(int row, int col);
    void spawnCaptureExplosion(const kungfu::Position &boardPos, kungfu::PlayerColor attackerColor);
    void drawOverlays(IRenderer &renderer, const kungfu::view::GameSnapshot &snapshot);
    int calculateAbsoluteMaterialScore(const kungfu::view::GameSnapshot &snapshot, kungfu::PlayerColor color) const;

    // Match Controller (Application Layer)
    std::unique_ptr<kungfu::MatchController> m_matchController;

    // UI & Visual State
    std::shared_ptr<ISoundPlayer> m_soundPlayer;
    float m_matchFoundBannerTimer = 0.0f;
    bool m_wasMatchStarted = false;

    // UI Buttons
    std::unique_ptr<Button> m_pauseButton;
    std::unique_ptr<Button> m_sidebarRestartButton;
    std::unique_ptr<Button> m_sidebarMenuButton;
    std::unique_ptr<Button> m_rematchButton;
    std::unique_ptr<Button> m_menuButton;
    std::unique_ptr<Button> m_cancelMatchmakingButton;

    // Layout Metrics
    const float m_boardStartX = 0.0f;
    const float m_boardStartY = 100.0f;
    const float m_boardRangeX = 800.0f;
    const float m_boardRangeY = 800.0f;

    // Click / Hover animation state
    float m_totalTime = 0.0f;
    float m_lastClickTime = 0.0f;
    BoardPos m_lastClickedTile{-1, -1};
    BoardPos m_hoveredTile{-1, -1};
    bool m_isHovering = false;
    PieceAnimation m_selectedPieceAnim;
    float m_pauseTransitionProgress = 0.0f;

    // UI Views & Systems
    ParticleSystem m_particleSystem;
    SidebarView m_sidebarView;
    HeaderView m_headerView;
    FooterView m_footerView;
    BoardView m_boardView;
    GameOverlaysView m_overlaysView;
};