#pragma once

#include <boost/asio.hpp>
#include <unordered_map>
#include <unordered_set>
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

// ============================================================================
// MatchManager
//
// אחראי על שני דברים:
//  1. Matchmaking - תור המתנה של שחקנים שממתינים ליריב.
//  2. מחזור החיים של משחקים פעילים (יצירה, טעינה, וניקוי כשמסתיים).
//
// ה-mutex כאן קיים בכוונה, גם שכרגע main.cpp מריץ io_context.run() מ-thread
// יחיד: המבנה כאן מיועד לתמוך במספר רב של משחקים במקביל, כולל אפשרות
// עתידית להריץ io_context.run() ממספר threads (thread pool) כדי לנצל
// ליבות מרובות - במקרה כזה registerPlayer/unregisterPlayer עלולים להיקרא
// במקביל מ-threads שונים, ואילו הגישה למצב המשחק (LiveMatch) עצמו כבר
// מוגנת בנפרד ע"י ה-strand הפרטי של כל LiveMatch.
// ============================================================================
class MatchManager {
private:
    std::unordered_map<std::uint64_t, std::shared_ptr<LiveMatch>> m_matches;
    std::uint64_t m_nextMatchId = 1;
    std::mutex m_mutex;

    // queue שומר על סדר ההגעה (FIFO); set מאפשר בדיקת "כבר בתור?" ב-O(1)
    std::queue<std::shared_ptr<NetworkSession>> m_waitingQueue;
    std::unordered_set<std::shared_ptr<NetworkSession>> m_waitingSet;

    boost::asio::io_context& m_ioContext;

public:
    explicit MatchManager(boost::asio::io_context& ioContext)
        : m_ioContext(ioContext) {}

    // הצהרות בלבד - המימוש המלא מועבר לתחתית NetworkServer.hpp, שם
    // NetworkSession מוכר במלואו.
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

    // נקרא ע"י LiveMatch (דרך callback) כשמשחק מסתיים, כדי לפנות אותו
    // מהמפה. מונע memory leak לוגי של משחקים שהסתיימו "בטבעיות".
    void removeMatch(std::uint64_t matchId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_matches.erase(matchId) > 0) {
            std::cout << "[MatchManager] Match " << matchId << " removed after completion." << std::endl;
        }
    }

private:
    std::shared_ptr<LiveMatch> startNewMatch(std::shared_ptr<NetworkSession> player1,
                                              std::shared_ptr<NetworkSession> player2);
};

} // namespace kungfu