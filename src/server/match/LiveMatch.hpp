// server/match/LiveMatch.hpp
#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <chrono>
#include <functional>
#include <string>
#include <vector>
#include "engine/core/GameEngine.hpp"
#include "../network/NetworkMessages.hpp"
#include "../ServerConfig.hpp"

namespace kungfu {

class NetworkSession;

class LiveMatch : public std::enable_shared_from_this<LiveMatch> {
private:
    static constexpr std::chrono::milliseconds kTickInterval{50};

    std::uint64_t m_matchId;
    std::shared_ptr<GameEngine> m_engine;

    std::weak_ptr<NetworkSession> m_whiteSession;
    std::weak_ptr<NetworkSession> m_blackSession;
    std::vector<std::weak_ptr<NetworkSession>> m_spectators;

    std::string m_whiteUsername;
    std::string m_blackUsername;

    boost::asio::strand<boost::asio::any_io_executor> m_strand;

    // Tick Loop state
    boost::asio::steady_timer m_tickTimer;
    std::chrono::steady_clock::time_point m_lastTickTime;
    bool m_isRunning = false;
    bool m_hasEnded = false;

    // Reconnect state
    bool m_isWhiteDisconnected = false;
    bool m_isBlackDisconnected = false;
    int m_reconnectSecondsLeft = ServerConfig::kReconnectTimeoutSec;
    boost::asio::steady_timer m_reconnectTimer;

    std::function<void(std::uint64_t)> m_onMatchEnded;

public:
    LiveMatch(boost::asio::io_context& ioContext,
              std::uint64_t matchId,
              std::shared_ptr<GameEngine> engine);

    // Setup & Getters
    void setPlayers(std::shared_ptr<NetworkSession> white, std::shared_ptr<NetworkSession> black);
    void setOnMatchEnded(std::function<void(std::uint64_t)> callback);

    std::uint64_t matchId() const;
    std::shared_ptr<GameEngine> engine() const;

    std::shared_ptr<NetworkSession> whiteSession() const;
    std::shared_ptr<NetworkSession> blackSession() const;

    std::string whiteUsername() const;
    std::string blackUsername() const;

    bool isWhiteDisconnected() const;
    bool isBlackDisconnected() const;
    bool isWaitingForReconnection() const;

    // ------------------------------------------------------------------------
    // Match Lifecycle
    // ------------------------------------------------------------------------
    void start();
    void stop();

    // ------------------------------------------------------------------------
    // Move Handling
    // ------------------------------------------------------------------------
    void handlePlayerMove(std::shared_ptr<NetworkSession> sender, const NetworkMovePacket& packet);

    // ------------------------------------------------------------------------
    // Reconnect Logic
    // ------------------------------------------------------------------------
    void handlePlayerDisconnect(std::shared_ptr<NetworkSession> session);
    void reconnectPlayer(std::shared_ptr<NetworkSession> newSession);

    // ------------------------------------------------------------------------
    // Spectator System
    // ------------------------------------------------------------------------
    void addSpectator(std::shared_ptr<NetworkSession> spectator);
    void removeSpectator(std::shared_ptr<NetworkSession> spectator);

private:
    // Lifecycle internal helpers
    void stopInternal();
    void markEndedOnce();
    void notifyGameOver();

    // Move Handling internal helpers
    void handlePlayerMoveInternal(std::shared_ptr<NetworkSession> sender, const NetworkMovePacket& packet);

    // Tick Loop internal handlers
    void scheduleFirstTick();
    void scheduleNextTick();
    void armTimer();
    void onTick();

    // Reconnect internal helpers
    void startReconnectCountdown();
    void triggerAutoResign();

    // Spectator & Broadcast internal helpers
    void broadcastToRoom(NetworkMessageType type, const std::vector<std::uint8_t>& payload);
    void syncSpectatorState(std::shared_ptr<NetworkSession> spectator);
};

} // namespace kungfu