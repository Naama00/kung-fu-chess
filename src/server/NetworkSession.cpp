// server/NetworkSession.cpp
#include "NetworkSession.hpp"
#include "Serializer.hpp"
#include "MatchManager.hpp"
#include <iostream>

namespace kungfu {

NetworkSession::NetworkSession(udp::socket& serverSocket, udp::endpoint remoteEndpoint, MatchManager& matchManager)
    : m_serverSocket(serverSocket),
      m_remoteEndpoint(remoteEndpoint),
      m_strand(boost::asio::make_strand(m_serverSocket.get_executor())),
      m_matchManager(matchManager),
      m_lastActivity(std::chrono::steady_clock::now()) {}

std::uint64_t NetworkSession::matchId() const { return m_matchId; }
void NetworkSession::setMatchId(std::uint64_t id) { m_matchId = id; }
void NetworkSession::setColor(PlayerColor col) { m_color = col; }
std::string NetworkSession::username() const { return m_username; }
int NetworkSession::rating() const { return m_rating; }
void NetworkSession::setRating(int r) { m_rating = r; }

void NetworkSession::start() {
    updateActivity();
}

// שליחת חבילה אסינכרונית דרך ה-Socket המשותף אל כתובת היעד של הלקוח
void NetworkSession::sendPacket(NetworkMessageType type, const std::vector<std::uint8_t>& payload) {
    auto frame = std::make_shared<std::vector<std::uint8_t>>(Serializer::buildFrame(type, payload));
    auto self = shared_from_this();
    boost::asio::post(m_strand, [self, frame]() {
        self->m_serverSocket.async_send_to(boost::asio::buffer(*frame), self->m_remoteEndpoint,
            boost::asio::bind_executor(self->m_strand,
            [self, frame](boost::system::error_code ec, std::size_t) {
                if (ec) {
                    std::cerr << "[Session] UDP Send error: " << ec.message() << std::endl;
                }
            }));
    });
}

void NetworkSession::processMessage(NetworkMessageType type, const std::vector<std::uint8_t>& payload) {
    updateActivity();

    if (type == NetworkMessageType::LOGIN_REQUEST) {
        std::string username, password;
        if (Serializer::deserializeAuthRequest(payload, username, password)) {
            int rating = 1200;
            bool success = m_matchManager.dbManager().authenticateUser(username, password, rating);

            std::vector<std::uint8_t> response;
            Serializer::writeU8(response, success ? 1 : 0);
            if (success) {
                Serializer::writeU32(response, static_cast<std::uint32_t>(rating));
                m_username = username;
                m_rating = rating;
                m_isAuthenticated = true;

                std::cout << "[Server] User connected via UDP: " << username << std::endl;

                auto activeMatch = m_matchManager.findActiveMatchForUser(username);
                if (activeMatch) {
                    activeMatch->reconnectPlayer(shared_from_this());

                    std::vector<std::uint8_t> matchFoundPayload;
                    Serializer::writeU64(matchFoundPayload, activeMatch->matchId());
                    Serializer::writeU8(matchFoundPayload, static_cast<std::uint8_t>(m_color));
                    sendPacket(NetworkMessageType::MATCH_FOUND, matchFoundPayload);
                }
            }
            sendPacket(NetworkMessageType::LOGIN_RESPONSE, response);
        }
    } else if (type == NetworkMessageType::REGISTER_REQUEST) {
        std::string username, password;
        if (Serializer::deserializeAuthRequest(payload, username, password)) {
            bool success = m_matchManager.dbManager().registerUser(username, password);
            std::vector<std::uint8_t> response;
            Serializer::writeU8(response, success ? 1 : 0);
            sendPacket(NetworkMessageType::REGISTER_RESPONSE, response);
        }
    } else if (type == NetworkMessageType::JOIN_MATCH_REQUEST) {
        if (!m_isAuthenticated) {
            std::cerr << "[Server] Blocked unauthenticated UDP player from joining match." << std::endl;
            return;
        }
        m_matchManager.registerPlayer(shared_from_this());
    } else if (type == NetworkMessageType::GAME_MOVE) {
        if (m_matchId == 0) return;

        auto packet = Serializer::deserializeMovePacket(payload);
        if (!packet.has_value()) {
            std::cerr << "[Session] Received malformed GAME_MOVE UDP payload." << std::endl;
            return;
        }

        auto match = m_matchManager.getMatch(m_matchId);
        if (match) {
            match->handlePlayerMove(shared_from_this(), *packet);
        }
    }
}

void NetworkSession::handleDisconnect() {
    m_matchManager.unregisterPlayer(shared_from_this());
}

} // namespace kungfu