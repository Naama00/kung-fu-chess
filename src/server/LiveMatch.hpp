#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <chrono>
#include <functional>
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
    static constexpr std::chrono::milliseconds kTickInterval{50}; // 20 טיקים בשנייה

    std::uint64_t m_matchId;
    std::shared_ptr<GameEngine> m_engine;

    // שימוש ב-weak_ptr מונע מעגלי זיכרון (Session -> Match -> Session)
    std::weak_ptr<NetworkSession> m_whiteSession;
    std::weak_ptr<NetworkSession> m_blackSession;

    // Strand ייעודי למשחק הזה בלבד. מבטיח שה-tick וטיפול במהלכים של
    // המשחק הספציפי הזה אף פעם לא רצים בו-זמנית, גם אם בעתיד ה-io_context
    // ירוץ ממספר threads (כלומר: תמיכה בהרצה מקבילה אמיתית של הרבה
    // משחקים בו-זמנית, כל אחד עם ה-strand שלו, בלי צורך במנעול גלובלי).
    boost::asio::strand<boost::asio::any_io_executor> m_strand;

    boost::asio::steady_timer m_tickTimer;
    std::chrono::steady_clock::time_point m_lastTickTime;
    bool m_isRunning = false;
    bool m_hasEnded = false;

    // נקרא פעם אחת כשהמשחק מסתיים (מכל סיבה) כדי שה-MatchManager יוכל
    // להסיר אותו מהמפה שלו. נשמר כ-std::function כדי ש-LiveMatch לא יצטרך
    // לדעת דבר על MatchManager (הפרדת אחריות).
    std::function<void(std::uint64_t)> m_onMatchEnded;

public:
    LiveMatch(boost::asio::io_context& ioContext,
              std::uint64_t matchId,
              std::shared_ptr<GameEngine> engine)
        : m_matchId(matchId),
          m_engine(std::move(engine)),
          m_strand(boost::asio::make_strand(ioContext.get_executor())),
          m_tickTimer(ioContext) {}

    void setPlayers(std::shared_ptr<NetworkSession> white, std::shared_ptr<NetworkSession> black) {
        m_whiteSession = white;
        m_blackSession = black;
    }

    void setOnMatchEnded(std::function<void(std::uint64_t)> callback) {
        m_onMatchEnded = std::move(callback);
    }

    std::uint64_t matchId() const { return m_matchId; }
    std::shared_ptr<GameEngine> engine() const { return m_engine; }

    std::shared_ptr<NetworkSession> whiteSession() const { return m_whiteSession.lock(); }
    std::shared_ptr<NetworkSession> blackSession() const { return m_blackSession.lock(); }

    void start() {
        auto self = shared_from_this();
        boost::asio::post(m_strand, [self]() {
            if (self->m_isRunning) return;
            self->m_isRunning = true;
            self->m_lastTickTime = std::chrono::steady_clock::now();
            self->scheduleFirstTick();
            std::cout << "[Match " << self->m_matchId << "] Tick loop started on server." << std::endl;
        });
    }

    // עצירה "שקטה" - לשימוש כשמישהו חיצוני (למשל ניתוק שחקן) רוצה לבטל
    // את המשחק בלי לשדר הודעת GAME_OVER (במקרה הזה משודרת הודעה אחרת).
    void stop() {
        auto self = shared_from_this();
        boost::asio::post(m_strand, [self]() {
            self->stopInternal();
            self->markEndedOnce();
        });
    }

    // מטופל מחוץ ל-strand (מגיע מ-NetworkSession) ומועבר פנימה אליו כדי
    // להבטיח סדר מהלכים עקבי גם אם קריאות מגיעות מ-threads שונים.
    void handlePlayerMove(std::shared_ptr<NetworkSession> sender, const NetworkMovePacket& packet) {
        auto self = shared_from_this();
        boost::asio::post(m_strand, [self, sender, packet]() {
            self->handlePlayerMoveInternal(sender, packet);
        });
    }

private:
    void scheduleFirstTick() {
        m_tickTimer.expires_after(kTickInterval);
        armTimer();
    }

    void scheduleNextTick() {
        // מתזמן ביחס לזמן התפוגה הקודם (ולא ביחס ל"עכשיו") כדי למנוע
        // סטייה מצטברת (drift) הנגרמת מזמן העיבוד של כל טיק.
        m_tickTimer.expires_at(m_tickTimer.expiry() + kTickInterval);
        armTimer();
    }

    void armTimer() {
        auto self = shared_from_this();
        m_tickTimer.async_wait(boost::asio::bind_executor(m_strand,
            [self](const boost::system::error_code& ec) {
                if (!ec && self->m_isRunning) {
                    self->onTick();
                }
            }));
    }

    void onTick() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastTickTime);
        m_lastTickTime = now;

        // עדכון הפיזיקה וצינון הכלים במנוע
        m_engine->wait(static_cast<int>(elapsed.count()));

        if (m_engine->isGameOver()) {
            std::cout << "[Match " << m_matchId << "] Game over on server." << std::endl;
            stopInternal();
            notifyGameOver();
            markEndedOnce();
            return;
        }

        scheduleNextTick();
    }

    void stopInternal() {
        m_isRunning = false;
        boost::system::error_code ec;
        m_tickTimer.cancel(ec);
    }

    // מבטיח שהקריאה ל-callback הסיום תתבצע פעם אחת בדיוק, לא משנה אם
    // המשחק הסתיים "בטבעיות" (onTick) או עקב ניתוק (stop() חיצוני).
    void markEndedOnce() {
        if (m_hasEnded) return;
        m_hasEnded = true;
        if (m_onMatchEnded) {
            m_onMatchEnded(m_matchId);
        }
    }

    // הודעה לשני השחקנים שהמשחק הסתיים בדרך "רגילה" (לא ניתוק).
    // מוגדר inline בתחתית NetworkServer.hpp, שם NetworkSession כבר מוכר
    // במלואו (כולל sendPacket).
    void notifyGameOver();

    // מימוש בפועל של טיפול במהלך שחקן - מוגדר inline בתחתית NetworkServer.hpp
    void handlePlayerMoveInternal(std::shared_ptr<NetworkSession> sender, const NetworkMovePacket& packet);
};

} // namespace kungfu