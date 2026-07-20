// src/server/match/MatchManager.hpp
#pragma once

#include <boost/asio.hpp>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>
#include <chrono>
#include "../persistence/DatabaseManager.hpp"
#include "../network/NetworkMessages.hpp"

namespace kungfu {

class NetworkSession;
class LiveMatch;

class MatchManager {
public:
    struct WaitingPlayer {
        std::shared_ptr<NetworkSession> session;
        std::chrono::steady_clock::time_point joinTime;
        int rating;
    };

private:
    std::unordered_map<std::uint64_t, std::shared_ptr<LiveMatch>> m_matches;
    std::uint64_t m_nextMatchId = 1;
    std::mutex m_mutex;

    std::vector<WaitingPlayer> m_waitingPool;

    DatabaseManager m_dbManager; // Actual secured SQLite database
    boost::asio::io_context& m_ioContext;
    
    boost::asio::steady_timer m_matchmakingTimer;

public:
    explicit MatchManager(boost::asio::io_context& ioContext);

    DatabaseManager& dbManager() { return m_dbManager; }

    void registerPlayer(std::shared_ptr<NetworkSession> session);
    void unregisterPlayer(std::shared_ptr<NetworkSession> session);

    std::shared_ptr<LiveMatch> getMatch(std::uint64_t matchId);
    void removeMatch(std::uint64_t matchId);
    std::shared_ptr<LiveMatch> findActiveMatchForUser(const std::string& username);

private:
    void scheduleMatchmakingTick();
    void runMatchmakingCycle();
    std::shared_ptr<LiveMatch> startNewMatch(std::shared_ptr<NetworkSession> player1,
                                             std::shared_ptr<NetworkSession> player2);
};

} // namespace kungfu