// server/match/MatchManager.hpp
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

struct MatchInfo {
    std::uint64_t matchId;
    std::string whitePlayer;
    std::string blackPlayer;
};

class MatchManager {
public:
    struct WaitingPlayer {
        std::shared_ptr<NetworkSession> session;
        std::chrono::steady_clock::time_point joinTime;
        int rating;
        std::uint64_t roomCode = 0;
    };

private:
    std::unordered_map<std::uint64_t, std::shared_ptr<LiveMatch>> m_matches;
    std::uint64_t m_nextMatchId = 1;
    std::mutex m_mutex;

    std::vector<WaitingPlayer> m_waitingPool;

    DatabaseManager m_dbManager; 
    boost::asio::io_context& m_ioContext;
    
    boost::asio::steady_timer m_matchmakingTimer;

public:
    explicit MatchManager(boost::asio::io_context& ioContext);

    DatabaseManager& dbManager() { return m_dbManager; }

    void registerPlayer(std::shared_ptr<NetworkSession> session, std::uint64_t roomCode = 0);
    void unregisterPlayer(std::shared_ptr<NetworkSession> session);

    std::shared_ptr<LiveMatch> getMatch(std::uint64_t matchId);
    void removeMatch(std::uint64_t matchId);
    std::shared_ptr<LiveMatch> findActiveMatchForUser(const std::string& username);
    std::vector<MatchInfo> getActiveMatchesList();

private:
    void scheduleMatchmakingTick();
    void runMatchmakingCycle();

    // Helper functions for clean matchmaking flow
    void removeTimedOutPlayers(std::chrono::steady_clock::time_point now);
    bool canPairPlayers(const WaitingPlayer& p1, const WaitingPlayer& p2, int waitDurationSec) const;
    bool isPrivateRoomMatch(const WaitingPlayer& p1, const WaitingPlayer& p2) const;
    bool isRatedMatch(const WaitingPlayer& p1, const WaitingPlayer& p2, int waitDurationSec) const;

    std::shared_ptr<LiveMatch> startNewMatch(std::shared_ptr<NetworkSession> player1,
                                             std::shared_ptr<NetworkSession> player2);
};

} // namespace kungfu