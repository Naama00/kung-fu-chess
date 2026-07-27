// server/match/LiveMatch.cpp
#include "server/match/LiveMatch.hpp"
#include "server/network/Serializer.hpp"
#include "server/network/PlayerSession.hpp"
#include "server/ServerConfig.hpp"
#include "engine/io/BoardPrinter.hpp"
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

void LiveMatch::setPlayers(std::shared_ptr<PlayerSession> white, std::shared_ptr<PlayerSession> black) {
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

std::shared_ptr<PlayerSession> LiveMatch::whiteSession() const { return m_whiteSession.lock(); }
std::shared_ptr<PlayerSession> LiveMatch::blackSession() const { return m_blackSession.lock(); }

std::string LiveMatch::whiteUsername() const { return m_whiteUsername; }
std::string LiveMatch::blackUsername() const { return m_blackUsername; }

bool LiveMatch::isWhiteDisconnected() const { return m_isWhiteDisconnected.load(); }
bool LiveMatch::isBlackDisconnected() const { return m_isBlackDisconnected.load(); }
bool LiveMatch::isWaitingForReconnection() const {
    return m_isWhiteDisconnected.load() || m_isBlackDisconnected.load();
}
bool LiveMatch::hasEnded() const { return m_hasEnded; }

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
    broadcastToRoom(NetworkMessageType::GAME_OVER, {});
}

void LiveMatch::handlePlayerMove(std::shared_ptr<PlayerSession> sender, const NetworkMovePacket& packet) {
    auto self = shared_from_this();
    boost::asio::post(m_strand, [self, sender, packet]() {
        self->handlePlayerMoveInternal(sender, packet);
    });
}

void LiveMatch::handlePlayerMoveInternal(std::shared_ptr<PlayerSession> sender, const NetworkMovePacket& packet) {
    if (isWaitingForReconnection()) {
        std::cout << "[Match " << m_matchId << "] Move rejected: match is waiting for opponent reconnection." << std::endl;
        return;
    }

    auto white = whiteSession();
    auto black = blackSession();
    
    bool isWhiteMove = (packet.playerColor == static_cast<std::uint8_t>(PlayerColor::White));
    bool isBlackMove = (packet.playerColor == static_cast<std::uint8_t>(PlayerColor::Black));

    if ((isWhiteMove && sender != white) || (isBlackMove && sender != black)) {
        std::cerr << "[Match " << m_matchId << "] Security warning: Unauthorized move attempt from session: " 
                  << (sender ? sender->username() : "Unknown") << " trying to move color: " 
                  << (isWhiteMove ? "White" : "Black") << std::endl;
        return;
    }

    ActionRequest request = Serializer::deserializeToRequest(packet);

    std::vector<ActionRequest> requests = { request };
    std::vector<ActionResult> results = m_engine->processActionRequests(requests);

    if (results.empty()) return;

    const ActionResult& result = results.front();
    sender->sendPacket(NetworkMessageType::MOVE_RESULT, Serializer::serializeActionResult(result));

    if (result.status == ActionStatus::Accepted) {
        auto movePayload = Serializer::serializeMovePacket(packet);
        broadcastToRoom(NetworkMessageType::GAME_MOVE, movePayload);
    }
}

void LiveMatch::scheduleFirstTick() {
    m_tickTimer.expires_after(ServerConfig::kLiveMatchTickInterval);
    armTimer();
}

void LiveMatch::scheduleNextTick() {
    m_tickTimer.expires_at(m_tickTimer.expiry() + ServerConfig::kLiveMatchTickInterval);
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

    auto now = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastTickTime).count();
    m_lastTickTime = now;

    m_engine->wait(static_cast<int>(elapsedMs));

    std::vector<ActionRequest> requests;
    m_engine->processActionRequests(requests);

    if (m_engine->isGameOver()) {
        stopInternal();
        markEndedOnce();
        notifyGameOver();
        return;
    }

    scheduleNextTick();
}

void LiveMatch::notifyOpponentDisconnectCountdown(int secondsLeft) {
    const bool bothDisconnected = m_isWhiteDisconnected.load() && m_isBlackDisconnected.load();
    if (!bothDisconnected) {
        std::vector<std::uint8_t> payload = { static_cast<std::uint8_t>(secondsLeft) };
        auto remaining = m_isWhiteDisconnected.load() ? blackSession() : whiteSession();
        if (remaining) {
            remaining->sendPacket(NetworkMessageType::DISCONNECT_COUNTDOWN, payload);
        }
    }
}

void LiveMatch::handlePlayerDisconnect(std::shared_ptr<PlayerSession> session) {
    if (m_hasEnded) return;

    auto white = whiteSession();
    auto black = blackSession();
    bool markedDisconnected = false;

    if (white && session == white) {
        m_isWhiteDisconnected.store(true);
        markedDisconnected = true;
        std::cout << "[Match " << m_matchId << "] Player White disconnected. "
                  << ServerConfig::kReconnectTimeoutSec << "s countdown initiated." << std::endl;
    } else if (black && session == black) {
        m_isBlackDisconnected.store(true);
        markedDisconnected = true;
        std::cout << "[Match " << m_matchId << "] Player Black disconnected. "
                  << ServerConfig::kReconnectTimeoutSec << "s countdown initiated." << std::endl;
    }

    if (!markedDisconnected) return;

    auto self = shared_from_this();
    boost::asio::post(m_strand, [self]() {
        if (self->m_hasEnded) return;
        self->m_reconnectSecondsLeft = ServerConfig::kReconnectTimeoutSec;
        self->notifyOpponentDisconnectCountdown(self->m_reconnectSecondsLeft);
        self->startReconnectCountdown();
    });
}

void LiveMatch::reconnectPlayer(std::shared_ptr<PlayerSession> newSession) {
    if (m_hasEnded || !newSession) return;

    const auto& username = newSession->username();
    PlayerColor color = PlayerColor::White;
    const char* colorName = nullptr;

    if (m_isWhiteDisconnected.load() && username == m_whiteUsername) {
        color = PlayerColor::White;
        colorName = "White";
    } else if (m_isBlackDisconnected.load() && username == m_blackUsername) {
        color = PlayerColor::Black;
        colorName = "Black";
    } else {
        return;
    }

    newSession->setMatchId(m_matchId);
    newSession->setColor(color);

    auto self = shared_from_this();
    boost::asio::post(m_strand, [self, newSession, color, colorName]() {
        boost::system::error_code ec;
        self->m_reconnectTimer.cancel(ec);

        const auto& reconnectUsername = newSession->username();
        if (self->m_isWhiteDisconnected.load() && reconnectUsername == self->m_whiteUsername) {
            self->m_whiteSession = newSession;
            self->m_isWhiteDisconnected.store(false);
        } else if (self->m_isBlackDisconnected.load() && reconnectUsername == self->m_blackUsername) {
            self->m_blackSession = newSession;
            self->m_isBlackDisconnected.store(false);
        } else {
            return;
        }

        std::cout << "[Match " << self->m_matchId << "] " << colorName << " reconnected successfully!" << std::endl;
        self->syncSpectatorState(newSession);

        const bool remainingDisconnected = self->m_isWhiteDisconnected.load() || self->m_isBlackDisconnected.load();

        auto otherSession = (color == PlayerColor::White) ? self->blackSession() : self->whiteSession();
        if (otherSession) {
            std::vector<std::uint8_t> clearPayload = { 0 };
            otherSession->sendPacket(NetworkMessageType::DISCONNECT_COUNTDOWN, clearPayload);
        }

        if (remainingDisconnected) {
            self->startReconnectCountdown();
        }
    });
}

void LiveMatch::startReconnectCountdown() {
    auto self = shared_from_this();
    m_reconnectTimer.expires_after(ServerConfig::kReconnectTickInterval);
    m_reconnectTimer.async_wait(boost::asio::bind_executor(m_strand,
        [self](const boost::system::error_code& ec) {
            if (ec || !self->m_isRunning || self->m_hasEnded) return;

            self->m_reconnectSecondsLeft--;
            self->notifyOpponentDisconnectCountdown(self->m_reconnectSecondsLeft);

            if (self->m_reconnectSecondsLeft <= 0) {
                if (self->m_isWhiteDisconnected.load() && self->m_isBlackDisconnected.load()) {
                    self->stopInternal();
                    self->markEndedOnce();
                } else {
                    self->triggerAutoResign();
                }
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

void LiveMatch::addSpectator(std::shared_ptr<PlayerSession> spectator) {
    auto self = shared_from_this();
    boost::asio::post(m_strand, [self, spectator]() {
        if (self->m_hasEnded) return;

        self->m_spectators.push_back(spectator);
        spectator->setMatchId(self->m_matchId);

        std::cout << "[Room " << self->m_matchId << "] Spectator " 
                  << spectator->username() << " joined the room." << std::endl;

        self->syncSpectatorState(spectator);
    });
}

void LiveMatch::removeSpectator(std::shared_ptr<PlayerSession> spectator) {
    if (!spectator) return;
    auto self = shared_from_this();
    boost::asio::post(m_strand, [self, spectator]() {
        auto it = std::remove_if(self->m_spectators.begin(), self->m_spectators.end(),
            [spectator](const std::weak_ptr<PlayerSession>& weakSpec) {
                auto spec = weakSpec.lock();
                return !spec || spec == spectator;
            });
        
        if (it != self->m_spectators.end()) {
            self->m_spectators.erase(it, self->m_spectators.end());
            std::cout << "[Room " << self->m_matchId << "] Spectator " 
                      << spectator->username() << " left the room." << std::endl;
        }
    });
}

void LiveMatch::broadcastToRoom(NetworkMessageType type, const std::vector<std::uint8_t>& payload) {
    auto white = whiteSession();
    auto black = blackSession();
    if (white) white->sendPacket(type, payload);
    if (black) black->sendPacket(type, payload);

    std::vector<std::shared_ptr<PlayerSession>> activeSpectators;
    {
        auto specIt = m_spectators.begin();
        while (specIt != m_spectators.end()) {
            if (auto spec = specIt->lock()) {
                activeSpectators.push_back(spec);
                ++specIt;
            } else {
                specIt = m_spectators.erase(specIt);
            }
        }
    }

    for (const auto& spectator : activeSpectators) {
        if (spectator) {
            spectator->sendPacket(type, payload);
        }
    }
}

void LiveMatch::syncSpectatorState(std::shared_ptr<PlayerSession> spectator) {
    auto rawBoard = std::dynamic_pointer_cast<const Board>(m_engine->getBoard());
    if (!rawBoard) return;
    
    std::string boardLayout = BoardPrinter::print(*rawBoard);

    std::vector<std::uint8_t> payload;
    Serializer::writeU64(payload, m_matchId);
    Serializer::writeString(payload, boardLayout);

    spectator->sendPacket(NetworkMessageType::ROOM_STATE_SYNC, payload);
}

MatchStateSnapshot LiveMatch::exportState() const {
    MatchStateSnapshot snap;
    if (m_engine) {
        snap = m_engine->exportState();
    }
    snap.matchId = m_matchId;
    snap.whiteUsername = m_whiteUsername;
    snap.blackUsername = m_blackUsername;
    snap.isWhiteDisconnected = m_isWhiteDisconnected.load();
    snap.isBlackDisconnected = m_isBlackDisconnected.load();
    snap.reconnectSecondsLeft = m_reconnectSecondsLeft;
    return snap;
}

void LiveMatch::restoreState(const MatchStateSnapshot& snapshot) {
    m_matchId = snapshot.matchId;
    m_whiteUsername = snapshot.whiteUsername;
    m_blackUsername = snapshot.blackUsername;
    m_isWhiteDisconnected.store(snapshot.isWhiteDisconnected);
    m_isBlackDisconnected.store(snapshot.isBlackDisconnected);
    m_reconnectSecondsLeft = snapshot.reconnectSecondsLeft;

    if (m_engine) {
        m_engine->restoreState(snapshot);
    }
}

} // namespace kungfu