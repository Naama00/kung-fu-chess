// server/match/LiveMatch.cpp
#include "LiveMatch.hpp"
#include "../network/Serializer.hpp"
#include "../network/NetworkSession.hpp"
#include "../ServerConfig.hpp"
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

// ============================================================================
// Match Lifecycle
// ============================================================================

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

// ============================================================================
// Move Handling
// ============================================================================

void LiveMatch::handlePlayerMove(std::shared_ptr<NetworkSession> sender, const NetworkMovePacket& packet) {
    auto self = shared_from_this();
    boost::asio::post(m_strand, [self, sender, packet]() {
        self->handlePlayerMoveInternal(sender, packet);
    });
}

void LiveMatch::handlePlayerMoveInternal(std::shared_ptr<NetworkSession> sender, const NetworkMovePacket& packet) {
    // 1. Retrieve active session handles
    auto white = whiteSession();
    auto black = blackSession();
    
    // 2. Validate move color tags
    bool isWhiteMove = (packet.playerColor == static_cast<std::uint8_t>(PlayerColor::White));
    bool isBlackMove = (packet.playerColor == static_cast<std::uint8_t>(PlayerColor::Black));

    // 3. Security check: verify sender ownership of the color
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

        // Broadcast validated move to active players and room spectators
        broadcastToRoom(NetworkMessageType::GAME_MOVE, movePayload);
    }
}

// ============================================================================
// Game Tick Loop
// ============================================================================

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

    // 1. Calculate elapsed time since last tick
    auto now = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastTickTime).count();
    m_lastTickTime = now;

    // 2. Advance game engine state logic
    m_engine->wait(static_cast<int>(elapsedMs));

    // 3. Process empty requests to trigger engine evaluations
    std::vector<ActionRequest> requests;
    m_engine->processActionRequests(requests);

    // 4. Check for game termination condition
    if (m_engine->isGameOver()) {
        stopInternal();
        markEndedOnce();
        notifyGameOver();
        return;
    }

    // 5. Schedule next tick iteration
    scheduleNextTick();
}

// ============================================================================
// Reconnect Logic
// ============================================================================

void LiveMatch::handlePlayerDisconnect(std::shared_ptr<NetworkSession> session) {
    auto self = shared_from_this();
    boost::asio::post(m_strand, [self, session]() {
        if (self->m_hasEnded) return;

        auto white = self->whiteSession();
        auto black = self->blackSession();

        if (white && session == white) {
            self->m_isWhiteDisconnected = true;
            std::cout << "[Match " << self->m_matchId << "] Player White disconnected. "
                      << ServerConfig::kReconnectTimeoutSec << "s countdown initiated." << std::endl;
        } else if (black && session == black) {
            self->m_isBlackDisconnected = true;
            std::cout << "[Match " << self->m_matchId << "] Player Black disconnected. "
                      << ServerConfig::kReconnectTimeoutSec << "s countdown initiated." << std::endl;
        }

        self->m_reconnectSecondsLeft = ServerConfig::kReconnectTimeoutSec;
        self->startReconnectCountdown();
    });
}

void LiveMatch::reconnectPlayer(std::shared_ptr<NetworkSession> newSession) {
    auto self = shared_from_this();
    boost::asio::post(m_strand, [self, newSession]() {
        boost::system::error_code ec;
        self->m_reconnectTimer.cancel(ec);

        const auto& username = newSession->username();

        auto handleReconnect = [&](std::shared_ptr<NetworkSession>& session, bool& isDisconnected, 
                                   PlayerColor color, const char* colorName) {
            session = newSession;
            isDisconnected = false;
            newSession->setMatchId(self->m_matchId);
            newSession->setColor(color);
            std::cout << "[Match " << self->m_matchId << "] " << colorName << " reconnected successfully!" << std::endl;
        };

        if (self->m_isWhiteDisconnected && username == self->m_whiteUsername) {
            handleReconnect(self->m_whiteSession, self->m_isWhiteDisconnected, PlayerColor::White, "White");
        } else if (self->m_isBlackDisconnected && username == self->m_blackUsername) {
            handleReconnect(self->m_blackSession, self->m_isBlackDisconnected, PlayerColor::Black, "Black");
        }
    });
}

void LiveMatch::startReconnectCountdown() {
    auto self = shared_from_this();
    m_reconnectTimer.expires_after(ServerConfig::kReconnectTickInterval);
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

// ============================================================================
// Spectator System & Broadcasts
// ============================================================================

void LiveMatch::addSpectator(std::shared_ptr<NetworkSession> spectator) {
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

void LiveMatch::removeSpectator(std::shared_ptr<NetworkSession> spectator) {
    auto self = shared_from_this();
    boost::asio::post(m_strand, [self, spectator]() {
        auto it = std::remove_if(self->m_spectators.begin(), self->m_spectators.end(),
            [spectator](const std::weak_ptr<NetworkSession>& weakSpec) {
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
    // 1. Send to active players
    auto white = whiteSession();
    auto black = blackSession();
    if (white) white->sendPacket(type, payload);
    if (black) black->sendPacket(type, payload);

    // 2. Broadcast to spectators with automatic prune of expired sessions
    auto it = m_spectators.begin();
    while (it != m_spectators.end()) {
        if (auto spec = it->lock()) {
            spec->sendPacket(type, payload);
            ++it;
        } else {
            it = m_spectators.erase(it);
        }
    }
}

// Sends the current board status to a viewer who has just joined the room
void LiveMatch::syncSpectatorState(std::shared_ptr<NetworkSession> spectator) {
    auto rawBoard = std::dynamic_pointer_cast<const Board>(m_engine->getBoard());
    if (!rawBoard) return;
    
    std::string boardLayout = BoardPrinter::print(*rawBoard);

    // Build the state synchronization packet using Serializer helper
    std::vector<std::uint8_t> payload;
    Serializer::writeU64(payload, m_matchId);
    Serializer::writeString(payload, boardLayout);

    spectator->sendPacket(NetworkMessageType::ROOM_STATE_SYNC, payload);
}

} // namespace kungfu