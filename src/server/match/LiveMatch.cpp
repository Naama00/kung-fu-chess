// server/match/LiveMatch.cpp
#include "LiveMatch.hpp"
#include "../network/Serializer.hpp"
#include "../network/NetworkSession.hpp"
#include <utility>
#include <iostream>

namespace kungfu {

LiveMatch::LiveMatch(boost::asio::io_context& ioContext,
                     std::uint64_t matchId,
                     std::shared_ptr<GameEngine> engine)
    : m_matchId(matchId),
      m_engine(std::move(engine)),
      m_strand(boost::asio::make_strand(ioContext.get_executor())),
      m_tickTimer(ioContext),
      m_reconnectTimer(ioContext) {}

void LiveMatch::setPlayers(std::shared_ptr<NetworkSession> white, std::shared_ptr<NetworkSession> black) {
    m_whiteSession = white;
    m_blackSession = black;

    if (white) m_whiteUsername = white->username();
    if (black) m_blackUsername = black->username();
}

void LiveMatch::setOnMatchEnded(std::function<void(std::uint64_t)> callback) {
    m_onMatchEnded = std::move(callback);
}

std::uint64_t LiveMatch::matchId() const { return m_matchId; }
std::shared_ptr<GameEngine> LiveMatch::engine() const { return m_engine; }

std::shared_ptr<NetworkSession> LiveMatch::whiteSession() const { return m_whiteSession.lock(); }
std::shared_ptr<NetworkSession> LiveMatch::blackSession() const { return m_blackSession.lock(); }

std::string LiveMatch::whiteUsername() const { return m_whiteUsername; }
std::string LiveMatch::blackUsername() const { return m_blackUsername; }

bool LiveMatch::isWhiteDisconnected() const { return m_isWhiteDisconnected; }
bool LiveMatch::isBlackDisconnected() const { return m_isBlackDisconnected; }
bool LiveMatch::isWaitingForReconnection() const { return m_isWhiteDisconnected || m_isBlackDisconnected; }

void LiveMatch::start() {
    auto self = shared_from_this();
    boost::asio::post(m_strand, [self]() {
        if (self->m_isRunning) return;
        self->m_isRunning = true;
        self->m_lastTickTime = std::chrono::steady_clock::now();
        self->scheduleFirstTick();
        std::cout << "[Match " << self->m_matchId << "] Tick loop started on server." << std::endl;
    });
}

void LiveMatch::stop() {
    auto self = shared_from_this();
    boost::asio::post(m_strand, [self]() {
        self->stopInternal();
        self->markEndedOnce();
    });
}

void LiveMatch::handlePlayerDisconnect(std::shared_ptr<NetworkSession> session) {
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

void LiveMatch::reconnectPlayer(std::shared_ptr<NetworkSession> newSession) {
    auto self = shared_from_this();
    boost::asio::post(m_strand, [self, newSession]() {
        boost::system::error_code ec;
        self->m_reconnectTimer.cancel(ec); 

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

void LiveMatch::handlePlayerMove(std::shared_ptr<NetworkSession> sender, const NetworkMovePacket& packet) {
    auto self = shared_from_this();
    boost::asio::post(m_strand, [self, sender, packet]() {
        self->handlePlayerMoveInternal(sender, packet);
    });
}

void LiveMatch::handlePlayerMoveInternal(std::shared_ptr<NetworkSession> sender, const NetworkMovePacket& packet) {
    ActionRequest request = Serializer::deserializeToRequest(packet);

    std::vector<ActionRequest> requests = { request };
    std::vector<ActionResult> results = m_engine->processActionRequests(requests);

    if (results.empty()) return;

    const ActionResult& result = results.front();
    sender->sendPacket(NetworkMessageType::MOVE_RESULT, Serializer::serializeActionResult(result));

    if (result.status == ActionStatus::Accepted) {
        auto movePayload = Serializer::serializeMovePacket(packet);

        auto white = whiteSession();
        auto black = blackSession();

        if (white) white->sendPacket(NetworkMessageType::GAME_MOVE, movePayload);
        if (black) black->sendPacket(NetworkMessageType::GAME_MOVE, movePayload);
    }
}

void LiveMatch::scheduleFirstTick() {
    m_tickTimer.expires_after(kTickInterval);
    armTimer();
}

void LiveMatch::scheduleNextTick() {
    m_tickTimer.expires_at(m_tickTimer.expiry() + kTickInterval);
    armTimer();
}

void LiveMatch::armTimer() {
    auto self = shared_from_this();
    m_tickTimer.async_wait(boost::asio::bind_executor(m_strand,
        [self](const boost::system::error_code& ec) {
            if (!ec && self->m_isRunning) {
                self->onTick();
            }
        }));
}

void LiveMatch::onTick() {
    if (!m_engine || m_hasEnded) {
        return;
    }

    // 1. Calculate realistic elapsed time
    auto now = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastTickTime).count();
    m_lastTickTime = now;

    // 2. Advance game engine logic (resolves piece motion, jump, and cooldown transitions)
    m_engine->wait(static_cast<int>(elapsedMs));

    // 3. Process actions (with empty requests to trigger standard engine evaluations)
    std::vector<ActionRequest> requests;
    m_engine->processActionRequests(requests);

    // 4. Verify game over status
    if (m_engine->isGameOver()) {
        stopInternal();
        markEndedOnce();
        notifyGameOver();
        return;
    }

    // 5. Reschedule the loop for the next tick interval
    scheduleNextTick();
}

void LiveMatch::stopInternal() {
    m_isRunning = false;
    boost::system::error_code ec;
    m_tickTimer.cancel(ec);
    m_reconnectTimer.cancel(ec);
}

void LiveMatch::markEndedOnce() {
    if (m_hasEnded) return;
    m_hasEnded = true;
    if (m_onMatchEnded) {
        m_onMatchEnded(m_matchId);
    }
}

void LiveMatch::notifyGameOver() {
    auto white = whiteSession();
    auto black = blackSession();

    if (white) white->sendPacket(NetworkMessageType::GAME_OVER, {});
    if (black) black->sendPacket(NetworkMessageType::GAME_OVER, {});
}

void LiveMatch::startReconnectCountdown() {
    auto self = shared_from_this();
    m_reconnectTimer.expires_after(std::chrono::seconds(1));
    m_reconnectTimer.async_wait(boost::asio::bind_executor(m_strand,
        [self](const boost::system::error_code& ec) {
            if (ec || !self->m_isRunning || self->m_hasEnded) return;

            if (self->m_isWhiteDisconnected && self->m_isBlackDisconnected) {
                self->stopInternal();
                self->markEndedOnce();
                return;
            }

            self->m_reconnectSecondsLeft--;

            std::vector<std::uint8_t> payload = { static_cast<std::uint8_t>(self->m_reconnectSecondsLeft) };
            auto remaining = self->m_isWhiteDisconnected ? self->blackSession() : self->whiteSession();
            if (remaining) {
                remaining->sendPacket(NetworkMessageType::DISCONNECT_COUNTDOWN, payload);
            }

            if (self->m_reconnectSecondsLeft <= 0) {
                self->triggerAutoResign(); 
            } else {
                self->startReconnectCountdown();
            }
        }));
}

void LiveMatch::triggerAutoResign() {
    stopInternal();
    markEndedOnce();
    notifyGameOver();
}

} // namespace kungfu