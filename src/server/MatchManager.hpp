
#pragma once

#include <boost/asio.hpp>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <vector>
#include <chrono>
#include "DatabaseManager.hpp"
#include "NetworkMessages.hpp"
#include "LiveMatch.hpp"

namespace kungfu {

class NetworkSession;
class LiveMatch;

class MatchManager {
public:
    // מבנה נתונים לייצוג שחקן בבריכת ההמתנה
    struct WaitingPlayer {
        std::shared_ptr<NetworkSession> session;
        std::chrono::steady_clock::time_point joinTime;
        int rating;
    };

private:
    std::unordered_map<std::uint64_t, std::shared_ptr<LiveMatch>> m_matches;
    std::uint64_t m_nextMatchId = 1;
    std::mutex m_mutex;

    // בריכת הממתינים החדשה
    std::vector<WaitingPlayer> m_waitingPool;

    DatabaseManager m_dbManager;
    boost::asio::io_context& m_ioContext;
    
    // טיימר אסינכרוני לביצוע סריקה מחזורית של הבריכה
    boost::asio::steady_timer m_matchmakingTimer;

public:
    explicit MatchManager(boost::asio::io_context& ioContext)
        : m_ioContext(ioContext), m_matchmakingTimer(ioContext) {
        scheduleMatchmakingTick();
    }

    DatabaseManager& dbManager() { return m_dbManager; }

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

    void removeMatch(std::uint64_t matchId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_matches.erase(matchId) > 0) {
            std::cout << "[MatchManager] Match " << matchId << " removed after completion." << std::endl;
        }
    }

        std::shared_ptr<LiveMatch> findActiveMatchForUser(const std::string& username) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& pair : m_matches) {
            auto match = pair.second;
            if (match->isWaitingForReconnection()) {
                if (match->isWhiteDisconnected() && match->whiteUsername() == username) {
                    return match;
                }
                if (match->isBlackDisconnected() && match->blackUsername() == username) {
                    return match;
                }
            }
        }
        return nullptr;
    }

private:
    void scheduleMatchmakingTick();
    void runMatchmakingCycle();
    std::shared_ptr<LiveMatch> startNewMatch(std::shared_ptr<NetworkSession> player1,
                                              std::shared_ptr<NetworkSession> player2);
};

} // namespace kungfu