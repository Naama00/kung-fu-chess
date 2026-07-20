// server/LiveMatch.hpp
#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <chrono>
#include <functional>
#include <iostream>
#include <vector>
#include <cstring>
#include <string>
#include "../engine/core/GameEngine.hpp"
#include "NetworkMessages.hpp"
#include "NetworkSession.hpp"

namespace kungfu {

class NetworkSession;

class LiveMatch : public std::enable_shared_from_this<LiveMatch> {
private:
    // להוציא hard coded!!!
    static constexpr std::chrono::milliseconds kTickInterval{50}; // 20 טיקים בשנייה

    std::uint64_t m_matchId;
    std::shared_ptr<GameEngine> m_engine;

    // שימוש ב-weak_ptr למניעת מעגלי זיכרון (Session -> Match -> Session)
    std::weak_ptr<NetworkSession> m_whiteSession;
    std::weak_ptr<NetworkSession> m_blackSession;

    // שמירת שמות המשתמש למקרה שהסשן נהרס פיזית בעת ניתוק
    std::string m_whiteUsername;
    std::string m_blackUsername;

    // Strand ייעודי למשחק הספציפי הזה להגנה מפני מרוץ תהליכים
    boost::asio::strand<boost::asio::any_io_executor> m_strand;

    boost::asio::steady_timer m_tickTimer;
    std::chrono::steady_clock::time_point m_lastTickTime;
    bool m_isRunning = false;
    bool m_hasEnded = false;

    // משתני ניהול ניתוק וספירה לאחור (Auto-Resign)
    bool m_isWhiteDisconnected = false;
    bool m_isBlackDisconnected = false;
    int m_reconnectSecondsLeft = 20;
    boost::asio::steady_timer m_reconnectTimer; // טיימר ניתוק ייעודי

    std::function<void(std::uint64_t)> m_onMatchEnded;

public:
    LiveMatch(boost::asio::io_context& ioContext,
              std::uint64_t matchId,
              std::shared_ptr<GameEngine> engine)
        : m_matchId(matchId),
          m_engine(std::move(engine)),
          m_strand(boost::asio::make_strand(ioContext.get_executor())),
          m_tickTimer(ioContext),
          m_reconnectTimer(ioContext) {}

    void setPlayers(std::shared_ptr<NetworkSession> white, std::shared_ptr<NetworkSession> black);

    void setOnMatchEnded(std::function<void(std::uint64_t)> callback) {
        m_onMatchEnded = std::move(callback);
    }

    std::uint64_t matchId() const { return m_matchId; }
    std::shared_ptr<GameEngine> engine() const { return m_engine; }

    std::shared_ptr<NetworkSession> whiteSession() const { return m_whiteSession.lock(); }
    std::shared_ptr<NetworkSession> blackSession() const { return m_blackSession.lock(); }

    std::string whiteUsername() const { return m_whiteUsername; }
    std::string blackUsername() const { return m_blackUsername; }

    bool isWhiteDisconnected() const { return m_isWhiteDisconnected; }
    bool isBlackDisconnected() const { return m_isBlackDisconnected; }
    bool isWaitingForReconnection() const { return m_isWhiteDisconnected || m_isBlackDisconnected; }

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

    // עצירה שקטה של המשחק
    void stop() {
        auto self = shared_from_this();
        boost::asio::post(m_strand, [self]() {
            self->stopInternal();
            self->markEndedOnce();
        });
    }

    // קבלת אירוע ניתוק פיזי של שחקן
    void handlePlayerDisconnect(std::shared_ptr<NetworkSession> session) {
        auto self = shared_from_this();
        boost::asio::post(m_strand, [self, session]() {
            if (self->m_hasEnded) return;

            auto white = self->whiteSession();
            auto black = self->blackSession();

            if (white && session == white) {
                self->m_isWhiteDisconnected = true;
                std::cout << "[Match " << self->m_matchId << "] Player White disconnected. 20s countdown initiated." << std::endl;
            } else if (black && session == black) {
                self->m_isBlackDisconnected = true;
                std::cout << "[Match " << self->m_matchId << "] Player Black disconnected. 20s countdown initiated." << std::endl;
            }

            self->m_reconnectSecondsLeft = 20;
            self->startReconnectCountdown();
        });
    }

    // טיפול בחיבור מחדש מוצלח של שחקן
    void reconnectPlayer(std::shared_ptr<NetworkSession> newSession) {
        auto self = shared_from_this();
        boost::asio::post(m_strand, [self, newSession]() {
            boost::system::error_code ec;
            self->m_reconnectTimer.cancel(ec); // עצירת טיימר הניתוק

            if (self->m_isWhiteDisconnected && newSession->username() == self->m_whiteUsername) {
                self->m_whiteSession = newSession;
                self->m_isWhiteDisconnected = false;
                newSession->setMatchId(self->m_matchId);
                newSession->setColor(PlayerColor::White);
                std::cout << "[Match " << self->m_matchId << "] White reconnected successfully!" << std::endl;
            } else if (self->m_isBlackDisconnected && newSession->username() == self->m_blackUsername) {
                self->m_blackSession = newSession;
                self->m_isBlackDisconnected = false;
                newSession->setMatchId(self->m_matchId);
                newSession->setColor(PlayerColor::Black);
                std::cout << "[Match " << self->m_matchId << "] Black reconnected successfully!" << std::endl;
            }
        });
    }

    void handlePlayerMove(std::shared_ptr<NetworkSession> sender, const NetworkMovePacket& packet);

private:
    void scheduleFirstTick() {
        m_tickTimer.expires_after(kTickInterval);
        armTimer();
    }

    void scheduleNextTick() {
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

    void onTick();
    
    void stopInternal() {
        m_isRunning = false;
        boost::system::error_code ec;
        m_tickTimer.cancel(ec);
        m_reconnectTimer.cancel(ec);
    }

    void markEndedOnce() {
        if (m_hasEnded) return;
        m_hasEnded = true;
        if (m_onMatchEnded) {
            m_onMatchEnded(m_matchId);
        }
    }

    void notifyGameOver();

    // הרצת טיימר הספירה לאחור של הניתוק (טיק של שנייה אחת)
    void startReconnectCountdown() {
        auto self = shared_from_this();
        m_reconnectTimer.expires_after(std::chrono::seconds(1));
        m_reconnectTimer.async_wait(boost::asio::bind_executor(m_strand,
            [self](const boost::system::error_code& ec) {
                if (ec || !self->m_isRunning || self->m_hasEnded) return;

                // אם שני הצדדים התנתקו במקביל, נבטל את המשחק
                if (self->m_isWhiteDisconnected && self->m_isBlackDisconnected) {
                    self->stopInternal();
                    self->markEndedOnce();
                    return;
                }

                self->m_reconnectSecondsLeft--;

                // שליחת ה-Countdown המעודכן לשחקן המחובר
                std::vector<std::uint8_t> payload = { static_cast<std::uint8_t>(self->m_reconnectSecondsLeft) };
                auto remaining = self->m_isWhiteDisconnected ? self->blackSession() : self->whiteSession();
                if (remaining) {
                    remaining->sendPacket(NetworkMessageType::DISCONNECT_COUNTDOWN, payload);
                }

                if (self->m_reconnectSecondsLeft <= 0) {
                    self->triggerAutoResign(); // ביצוע הפסד טכני
                } else {
                    self->startReconnectCountdown();
                }
            }));
    }

    void triggerAutoResign();
    void handlePlayerMoveInternal(std::shared_ptr<NetworkSession> sender, const NetworkMovePacket& packet);
};

} // namespace kungfu