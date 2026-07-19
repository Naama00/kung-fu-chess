#pragma once

#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>
#include <queue>
#include "../engine/core/GameEngine.hpp"
#include "NetworkMessages.hpp"

namespace kungfu {

// הצהרות קודמות בלבד - מונע תלות מעגלית
class NetworkSession;
class LiveMatch;

class MatchManager {
private:
    std::unordered_map<std::uint64_t, std::shared_ptr<LiveMatch>> m_matches;
    std::uint64_t m_nextMatchId = 1;
    std::mutex m_mutex;

    std::queue<std::shared_ptr<NetworkSession>> m_waitingPlayers;
    boost::asio::io_context& m_ioContext;

public:
    explicit MatchManager(boost::asio::io_context& ioContext) 
        : m_ioContext(ioContext) {}

    // הצהרות בלבד - המימוש המלא מועבר לתחתית NetworkServer.hpp
    void registerPlayer(std::shared_ptr<NetworkSession> session);
    void unregisterPlayer(std::shared_ptr<NetworkSession> session);

    std::shared_ptr<LiveMatch> getMatch(std::uint64_t matchId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_matches.find(matchId);
        if (it != m_matches.end()) {
            return it->second;
        }
        return nullptr;
    }

private:
    void startNewMatch(std::shared_ptr<NetworkSession> player1, std::shared_ptr<NetworkSession> player2);
};

} // namespace kungfu