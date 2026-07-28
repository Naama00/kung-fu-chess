// engine/core/MatchController.hpp
#pragma once

#include "engine/actions/ActionRequest.hpp"
#include "engine/actions/ActionResult.hpp"
#include "engine/actions/PlayerAction.hpp"
#include "engine/common/Enums.hpp"
#include "engine/common/GameConfig.hpp"
#include "engine/common/Position.hpp"
#include "engine/core/GameEngine.hpp"
#include "engine/core/PremoveQueue.hpp"
#include "engine/events/GameEvents.hpp"
#include "engine/rules/PromotionRules.hpp"
#include "engine/rules/RuleEngine.hpp"
#include "engine/snapshot/GameSnapshot.hpp"
#include "engine/events/EventBus.hpp"
#include "players/IPlayer.hpp"
#include "players/ai/IAIDecisionStrategy.hpp"
#include "players/human/HumanPlayer.hpp"
#include "players/network/NetworkPlayer.hpp"
#include "players/network/NetworkSession.hpp"

#include <boost/asio.hpp>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace kungfu {

enum class AiDifficulty { Easy, Medium, Hard };

class MatchController {
public:
    MatchController(const GameConfig& config,
                    bool isSimultaneous,
                    bool isAiOpponent,
                    AiDifficulty aiDifficulty,
                    bool isNetworkMode,
                    std::string host = ClientConfig::kDefaultHost, 
                    std::string port = ClientConfig::kDefaultPort, 
                    bool isSpectator = false,
                    std::uint64_t spectateMatchId = 0,
                    std::uint64_t onlineRoomCode = 0,
                    std::shared_ptr<NetworkSession> authSession = nullptr);

    ~MatchController();

    void update(float deltaTime);
    bool handleTileClick(int row, int col);
    void handleJump(const Position& pos);
    void togglePause();
    void resetMatch();
    void cancelMatchmaking();

    view::GameSnapshot getSnapshot() const;
    const PremoveQueue& getPremoveQueue() const;
    int getCurrentTimeMs() const;
    bool isPaused() const;
    bool isGameOver() const;
    bool isNetworkMode() const;
    bool isAiOpponent() const;
    bool isSpectator() const;
    bool hasMatchStarted() const;
    bool isOpponentDisconnectedWithCountdown() const;
    int opponentDisconnectCountdown() const;
    bool matchEnded() const;
    bool opponentDisconnected() const;
    float searchTimer() const;
    std::uint64_t onlineRoomCode() const;
    bool hasActiveMotion() const;
    std::shared_ptr<EventBus> getEventBus() const;

    std::string getPlayerName(PlayerColor color) const;
    const std::vector<std::string>& getHistory(PlayerColor color) const;
    std::optional<PlayerColor> determineWinnerColor() const;
    std::optional<Position> selectedPosition() const;
    void clearSelection();

    std::shared_ptr<NetworkPlayer> networkPlayer() const { return m_networkPlayer; }

private:
    void initializeMatch();
    void processAiTurn(float deltaTime);
    void applySyncedBoard(const std::string& boardText);
    std::unique_ptr<IAIDecisionStrategy> createAiStrategy() const;
    void addHistoryLog(PlayerColor color, const std::string& logText);
    std::string getMoveNotationString(PieceType type, const Position& from, const Position& to) const;
    PieceType getPieceTypeAt(const Position& pos) const;

    // Config & Engine
    GameConfig m_config;
    bool m_isSimultaneous = true;
    bool m_isAiOpponent = false;
    AiDifficulty m_aiDifficulty = AiDifficulty::Medium;
    bool m_isNetworkMode = false;
    bool m_isSpectator = false;
    std::string m_host;
    std::string m_port;
    std::uint64_t m_spectateMatchId = 0;
    std::uint64_t m_onlineRoomCode = 0;

    std::shared_ptr<GameEngine> m_gameEngine;
    std::shared_ptr<HumanPlayer> m_humanPlayer;
    std::shared_ptr<IPlayer> m_aiPlayer;
    std::shared_ptr<EventBus> m_eventBus;

    // Network State
    std::shared_ptr<NetworkPlayer> m_networkPlayer;
    std::shared_ptr<NetworkSession> m_authSession;
    boost::asio::io_context m_ioContext;
    std::thread m_networkThread;
    bool m_wasMatchStarted = false;
    float m_searchTimer = 0.0f;

    // AI State
    bool m_aiThinking = false;
    bool m_aiActionPending = false;
    float m_aiDecisionTimer = 1.0f;
    std::future<std::vector<ActionRequest>> m_aiFuture;

    // Match State
    bool m_isPaused = false;
    std::vector<std::string> m_whiteHistory;
    std::vector<std::string> m_blackHistory;
};

} // namespace kungfu