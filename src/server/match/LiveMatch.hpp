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

namespace kungfu {

class NetworkSession;

class LiveMatch : public std::enable_shared_from_this<LiveMatch> {
private:
    static constexpr std::chrono::milliseconds kTickInterval{50}; // 20 ticks per second

    std::uint64_t m_matchId;
    std::shared_ptr<GameEngine> m_engine;

    std::weak_ptr<NetworkSession> m_whiteSession;
    std::weak_ptr<NetworkSession> m_blackSession;

    std::string m_whiteUsername;
    std::string m_blackUsername;

    boost::asio::strand<boost::asio::any_io_executor> m_strand;

    boost::asio::steady_timer m_tickTimer;
    std::chrono::steady_clock::time_point m_lastTickTime;
    bool m_isRunning = false;
    bool m_hasEnded = false;

    bool m_isWhiteDisconnected = false;
    bool m_isBlackDisconnected = false;
    int m_reconnectSecondsLeft = 20;
    boost::asio::steady_timer m_reconnectTimer; 

    std::function<void(std::uint64_t)> m_onMatchEnded;

public:
    LiveMatch(boost::asio::io_context& ioContext,
              std::uint64_t matchId,
              std::shared_ptr<GameEngine> engine);

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

    void start();
    void stop();
    void handlePlayerDisconnect(std::shared_ptr<NetworkSession> session);
    void reconnectPlayer(std::shared_ptr<NetworkSession> newSession);
    void handlePlayerMove(std::shared_ptr<NetworkSession> sender, const NetworkMovePacket& packet);

private:
    void scheduleFirstTick();
    void scheduleNextTick();
    void armTimer();
    void onTick();
    void stopInternal();
    void markEndedOnce();
    void notifyGameOver();
    void startReconnectCountdown();
    void triggerAutoResign();
    void handlePlayerMoveInternal(std::shared_ptr<NetworkSession> sender, const NetworkMovePacket& packet);
};

} // namespace kungfu