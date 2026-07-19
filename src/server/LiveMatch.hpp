#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <chrono>
#include <iostream>
#include <vector>
#include <cstring>
#include "../engine/core/GameEngine.hpp"
#include "NetworkMessages.hpp"

namespace kungfu {

// הצהרה קודמת של NetworkSession למניעת תלויות מעגליות
class NetworkSession;

class LiveMatch : public std::enable_shared_from_this<LiveMatch> {
private:
    std::uint64_t m_matchId;
    std::shared_ptr<GameEngine> m_engine;
    
    // שימוש ב-weak_ptr מונע מעגלי זיכרון (Session -> Match -> Session)
    std::weak_ptr<NetworkSession> m_whiteSession;
    std::weak_ptr<NetworkSession> m_blackSession;

    boost::asio::steady_timer m_tickTimer;
    std::chrono::steady_clock::time_point m_lastTickTime;
    bool m_isRunning = false;

public:
    LiveMatch(boost::asio::io_context& ioContext, 
              std::uint64_t matchId, 
              std::shared_ptr<GameEngine> engine)
        : m_matchId(matchId), 
          m_engine(std::move(engine)), 
          m_tickTimer(ioContext) {}

    void setPlayers(std::shared_ptr<NetworkSession> white, std::shared_ptr<NetworkSession> black) {
        m_whiteSession = white;
        m_blackSession = black;
    }

    std::uint64_t matchId() const { return m_matchId; }
    std::shared_ptr<GameEngine> engine() const { return m_engine; }

    void start() {
        if (m_isRunning) return;
        m_isRunning = true;
        m_lastTickTime = std::chrono::steady_clock::now();
        scheduleNextTick();
        std::cout << "[Match " << m_matchId << "] Tick loop started on server." << std::endl;
    }

    void stop() {
        m_isRunning = false;
        boost::system::error_code ec;
        m_tickTimer.cancel(ec);
    }

    // פונקציית עיבוד מהלכים שתמומש לאחר הגדרת NetworkSession
    void handlePlayerMove(std::shared_ptr<NetworkSession> sender, const NetworkMovePacket& packet);

private:
    void scheduleNextTick() {
        if (!m_isRunning) return;

        auto self = shared_from_this();
        m_tickTimer.expires_after(std::chrono::milliseconds(50)); // 20 Ticks בשנייה
        m_tickTimer.async_wait([self](const boost::system::error_code& ec) {
            if (!ec && self->m_isRunning) {
                self->onTick();
            }
        });
    }

    void onTick() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastTickTime);
        m_lastTickTime = now;

        // עדכון הפיזיקה וצינון הכלים במנוע
        m_engine->wait(static_cast<int>(elapsed.count()));

        if (m_engine->isGameOver()) {
            std::cout << "[Match " << m_matchId << "] Game over on server." << std::endl;
            stop();
            return;
        }

        scheduleNextTick();
    }
};

} // namespace kungfu