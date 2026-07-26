// server/network/PlayerSession.cpp
#include "server/network/PlayerSession.hpp"
#include "server/network/Serializer.hpp"
#include "server/match/MatchManager.hpp"
#include "server/match/LiveMatch.hpp"
#include "server/ServerConfig.hpp"
#include <iostream>
#include <random>

namespace kungfu {

namespace {

std::uint64_t generateSessionToken() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    return rng();
}

} // namespace

PlayerSession::PlayerSession(std::shared_ptr<IConnection> controlChannel, MatchManager& matchManager)
    : m_matchManager(matchManager),
      m_controlChannel(std::move(controlChannel)),
      m_lastActivity(std::chrono::steady_clock::now()) {}

std::uint64_t PlayerSession::sessionToken() const { return m_sessionToken; }

std::uint64_t PlayerSession::matchId() const { return m_matchId; }
void PlayerSession::setMatchId(std::uint64_t id) { m_matchId = id; }
void PlayerSession::setColor(PlayerColor col) { m_color = col; }

std::string PlayerSession::username() const { return m_username; }
int PlayerSession::rating() const { return m_rating; }
void PlayerSession::setRating(int r) { m_rating = r; }
bool PlayerSession::isAuthenticated() const { return m_isAuthenticated; }

void PlayerSession::bindRealtimeChannel(std::shared_ptr<IConnection> realtimeChannel) {
    m_realtimeChannel = std::move(realtimeChannel);
}

bool PlayerSession::hasRealtimeChannel() const { return m_realtimeChannel != nullptr; }

void PlayerSession::sendPacket(NetworkMessageType type, const std::vector<std::uint8_t>& payload) {
    auto& channel = (channelFor(type) == TransportChannel::Control) ? m_controlChannel : m_realtimeChannel;

    if (!channel) {
        if (m_controlChannel) {
            m_controlChannel->send(type, payload);
            return;
        }

        std::cerr << "[PlayerSession] Dropped message type " << static_cast<int>(type)
                  << " for '" << m_username << "': target channel not available." << std::endl;
        return;
    }

    channel->send(type, payload);
}

void PlayerSession::sendControlPacket(NetworkMessageType type, const std::vector<std::uint8_t>& payload) {
    if (!m_controlChannel) {
        std::cerr << "[PlayerSession] Dropped control message type " << static_cast<int>(type)
                  << " for '" << m_username << "': control channel not available." << std::endl;
        return;
    }
    m_controlChannel->send(type, payload);
}

// ============================================================================
// Dedicated Message Handlers
// ============================================================================

void PlayerSession::handleLoginRequest(const std::vector<std::uint8_t>& payload) {
    std::string username, password;
    if (!Serializer::deserializeAuthRequest(payload, username, password)) return;

    int rating = ServerConfig::kDefaultRating;
    bool success = m_matchManager.dbManager().authenticateUser(username, password, rating);
    std::uint64_t token = 0;

    if (success) {
        token = generateSessionToken();
        m_sessionToken = token;
        m_username = username;
        m_rating = rating;
        m_isAuthenticated = true;

        std::cout << "[Server] User authenticated over TCP: " << username << std::endl;

        auto activeMatch = m_matchManager.findActiveMatchForUser(username);
        if (activeMatch) {
            activeMatch->reconnectPlayer(shared_from_this());

            bool isWhite = (activeMatch->whiteUsername() == username);
            auto opponent = isWhite ? activeMatch->blackSession() : activeMatch->whiteSession();
            std::string opponentName = isWhite ? activeMatch->blackUsername() : activeMatch->whiteUsername();
            int opponentRating = opponent ? opponent->rating() : 0;
            auto color = isWhite ? PlayerColor::White : PlayerColor::Black;

            sendPacket(NetworkMessageType::MATCH_FOUND,
                Serializer::serializeMatchFound(activeMatch->matchId(), static_cast<std::uint8_t>(color),
                                                 opponentName, static_cast<std::uint32_t>(opponentRating)));
        }
    }

    sendPacket(NetworkMessageType::LOGIN_RESPONSE, Serializer::serializeLoginResponse(success, rating, token));
}

void PlayerSession::handleRegisterRequest(const std::vector<std::uint8_t>& payload) {
    std::string username, password;
    if (Serializer::deserializeAuthRequest(payload, username, password)) {
        bool success = m_matchManager.dbManager().registerUser(username, password);
        std::vector<std::uint8_t> response;
        Serializer::writeU8(response, success ? 1 : 0);
        sendPacket(NetworkMessageType::REGISTER_RESPONSE, response);
    }
}

void PlayerSession::handleJoinMatchRequest(const std::vector<std::uint8_t>& payload) {
    if (!m_isAuthenticated) {
        std::cerr << "[Server] Blocked unauthenticated player from joining match." << std::endl;
        return;
    }
    std::uint64_t roomCode = 0;
    if (payload.size() >= 8) {
        std::size_t offset = 0;
        Serializer::readU64(payload, offset, roomCode);
    }
    m_matchManager.registerPlayer(shared_from_this(), roomCode);
}

void PlayerSession::handleGameMoveMsg(const std::vector<std::uint8_t>& payload) {
    if (m_matchId == 0) return;

    auto packet = Serializer::deserializeMovePacket(payload);
    if (!packet.has_value()) {
        std::cerr << "[PlayerSession] Received malformed GAME_MOVE payload." << std::endl;
        return;
    }

    auto match = m_matchManager.getMatch(m_matchId);
    if (match) {
        match->handlePlayerMove(shared_from_this(), *packet);
    }
}

void PlayerSession::handleSpectateRoomRequest(const std::vector<std::uint8_t>& payload) {
    std::size_t offset = 0;
    std::uint64_t targetMatchId = 0;
    if (Serializer::readU64(payload, offset, targetMatchId)) {
        auto match = m_matchManager.getMatch(targetMatchId);
        if (match) {
            match->addSpectator(shared_from_this());
        } else {
            std::cerr << "[Server] Spectate request for non-existent match: " << targetMatchId << std::endl;
        }
    }
}

void PlayerSession::handleRoomListRequest() {
    auto matches = m_matchManager.getActiveMatchesList();

    std::vector<std::uint8_t> response;
    Serializer::writeU32(response, static_cast<std::uint32_t>(matches.size()));

    for (const auto& match : matches) {
        Serializer::writeU64(response, match.matchId);
        Serializer::writeString(response, match.whitePlayer);
        Serializer::writeString(response, match.blackPlayer);
    }

    sendPacket(NetworkMessageType::ROOM_LIST_RESPONSE, response);
}

void PlayerSession::processMessage(NetworkMessageType type, const std::vector<std::uint8_t>& payload, TransportChannel fromChannel) {
    updateActivity();

    bool isControlMove = (fromChannel == TransportChannel::Control) && (type == NetworkMessageType::GAME_MOVE);
    if (channelFor(type) != fromChannel && !isControlMove) {
        std::cerr << "[PlayerSession] Rejected message type " << static_cast<int>(type)
                  << " from '" << m_username << "': arrived on the wrong transport." << std::endl;
        return;
    }

    switch (type) {
        case NetworkMessageType::LOGIN_REQUEST:         handleLoginRequest(payload); break;
        case NetworkMessageType::REGISTER_REQUEST:      handleRegisterRequest(payload); break;
        case NetworkMessageType::JOIN_MATCH_REQUEST:    handleJoinMatchRequest(payload); break;
        case NetworkMessageType::GAME_MOVE:             handleGameMoveMsg(payload); break;
        case NetworkMessageType::SPECTATE_ROOM_REQUEST: handleSpectateRoomRequest(payload); break;
        case NetworkMessageType::ROOM_LIST_REQUEST:     handleRoomListRequest(); break;
        case NetworkMessageType::HEARTBEAT:             break;
        default:
            std::cerr << "[PlayerSession] Received unhandled message type: "
                      << static_cast<int>(type) << std::endl;
            break;
    }
}

void PlayerSession::handleDisconnect() {
    m_matchManager.unregisterPlayer(shared_from_this());
    if (m_matchId != 0) {
        auto match = m_matchManager.getMatch(m_matchId);
        if (match) {
            match->removeSpectator(shared_from_this());
        }
    }
}

std::chrono::steady_clock::time_point PlayerSession::lastActivity() const { return m_lastActivity; }
void PlayerSession::updateActivity() { m_lastActivity = std::chrono::steady_clock::now(); }

} // namespace kungfu