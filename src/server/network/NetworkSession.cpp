// src/server/network/NetworkSession.cpp
#include "NetworkSession.hpp"
#include "Serializer.hpp"
#include "../match/MatchManager.hpp"
#include "../match/LiveMatch.hpp"
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

    switch (type) {
        case NetworkMessageType::LOGIN_REQUEST: {
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
            break;
        }

        case NetworkMessageType::REGISTER_REQUEST: {
            std::string username, password;
            if (Serializer::deserializeAuthRequest(payload, username, password)) {
                bool success = m_matchManager.dbManager().registerUser(username, password);
                std::vector<std::uint8_t> response;
                Serializer::writeU8(response, success ? 1 : 0);
                sendPacket(NetworkMessageType::REGISTER_RESPONSE, response);
            }
            break;
        }

        case NetworkMessageType::JOIN_MATCH_REQUEST: {
            if (!m_isAuthenticated) {
                std::cerr << "[Server] Blocked unauthenticated UDP player from joining match." << std::endl;
                break;
            }
            // Retrieving a designated room code if one exists
            std::uint64_t roomCode = 0;
            if (payload.size() >= 8) {
                std::size_t offset = 0;
                Serializer::readU64(payload, offset, roomCode);
            }
            m_matchManager.registerPlayer(shared_from_this(), roomCode);
            break;
        }

        case NetworkMessageType::GAME_MOVE: {
            if (m_matchId == 0) break;

            auto packet = Serializer::deserializeMovePacket(payload);
            if (!packet.has_value()) {
                std::cerr << "[Session] Received malformed GAME_MOVE UDP payload." << std::endl;
                break;
            }

            auto match = m_matchManager.getMatch(m_matchId);
            if (match) {
                match->handlePlayerMove(shared_from_this(), *packet);
            }
            break;
        }

        case NetworkMessageType::SPECTATE_ROOM_REQUEST: {
            std::size_t offset = 0;
            std::uint64_t targetMatchId = 0;
            if (Serializer::readU64(payload, offset, targetMatchId)) {
                auto match = m_matchManager.getMatch(targetMatchId);
                if (match) {
                    match->addSpectator(shared_from_this());
                } else {
                    std::cerr << "[Server] User requested non-existent Match ID to spectate: " << targetMatchId << std::endl;
                }
            }
            break;
        }

        case NetworkMessageType::ROOM_LIST_REQUEST: {
            auto matches = m_matchManager.getActiveMatchesList();
            
            std::vector<std::uint8_t> response;
            Serializer::writeU32(response, static_cast<std::uint32_t>(matches.size()));
            
            for (const auto& match : matches) {
                Serializer::writeU64(response, match.matchId);
                
                Serializer::writeU32(response, static_cast<std::uint32_t>(match.whitePlayer.size()));
                for (char c : match.whitePlayer) {
                    response.push_back(static_cast<std::uint8_t>(c));
                }
                
                Serializer::writeU32(response, static_cast<std::uint32_t>(match.blackPlayer.size()));
                for (char c : match.blackPlayer) {
                    response.push_back(static_cast<std::uint8_t>(c));
                }
            }
            
            sendPacket(NetworkMessageType::ROOM_LIST_RESPONSE, response);
            break;
        }

        default:
            std::cerr << "[Session] Received unhandled network message type: "
                      << static_cast<int>(type) << std::endl;
            break;
    }
}

void NetworkSession::handleDisconnect() {
    m_matchManager.unregisterPlayer(shared_from_this());
    // If spectating a match, remove from spectator list
    if (m_matchId != 0) {
        auto match = m_matchManager.getMatch(m_matchId);
        if (match) {
            match->removeSpectator(shared_from_this());
        }
    }
}

std::chrono::steady_clock::time_point NetworkSession::lastActivity() const { return m_lastActivity; }
void NetworkSession::updateActivity() { m_lastActivity = std::chrono::steady_clock::now(); }

} // namespace kungfu