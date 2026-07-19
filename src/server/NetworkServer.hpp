#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <iostream>
#include <cstring>
#include "NetworkMessages.hpp"
#include "Serializer.hpp"
#include "MatchManager.hpp"
#include "LiveMatch.hpp"
#include "engine/io/BoardParser.hpp"
#include "engine/rules/RuleEngine.hpp"
#include "engine/common/GameConfig.hpp"

namespace kungfu {

using boost::asio::ip::tcp;

class NetworkSession : public std::enable_shared_from_this<NetworkSession> {
private:
    tcp::socket m_socket;
    boost::asio::strand<boost::asio::any_io_executor> m_strand;
    MatchManager& m_matchManager;
    std::uint64_t m_matchId = 0;
    PlayerColor m_color = PlayerColor::White;

    std::vector<std::uint8_t> m_headerBuffer;
    std::vector<std::uint8_t> m_payloadBuffer;

public:
    NetworkSession(tcp::socket socket, MatchManager& matchManager)
        : m_socket(std::move(socket)), 
          m_strand(boost::asio::make_strand(m_socket.get_executor())),
          m_matchManager(matchManager) {
        m_headerBuffer.resize(5); // כותרת בגודל קבוע: בית אחד לסוג, 4 בתים לאורך
    }

    tcp::socket& socket() { return m_socket; }
    std::uint64_t matchId() const { return m_matchId; }
    void setMatchId(std::uint64_t id) { m_matchId = id; }
    void setColor(PlayerColor col) { m_color = col; }

    void start() {
        auto self = shared_from_this();
        boost::asio::post(m_strand, [self]() {
            self->readHeader();
        });
    }

    // שליחת הודעה בצורה בטוחה מסוג קול קריאה (Thread-safe)
    void sendPacket(NetworkMessageType type, const std::vector<std::uint8_t>& payload) {
        auto self = shared_from_this();
        boost::asio::post(m_strand, [self, type, payload]() {
            self->writePacket(type, payload);
        });
    }

private:
    void readHeader() {
        auto self = shared_from_this();
        boost::asio::async_read(m_socket, boost::asio::buffer(m_headerBuffer),
            boost::asio::bind_executor(m_strand,
            [self](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    NetworkMessageType type = static_cast<NetworkMessageType>(self->m_headerBuffer[0]);
                    
                    std::uint32_t payloadSize = 0;
                    std::memcpy(&payloadSize, &self->m_headerBuffer[1], sizeof(payloadSize));

                    self->readPayload(type, payloadSize);
                } else {
                    self->handleDisconnect();
                }
            }));
    }

    void readPayload(NetworkMessageType type, std::uint32_t payloadSize) {
        auto self = shared_from_this();
        
        if (payloadSize == 0) {
            processMessage(type, {});
            readHeader();
            return;
        }

        m_payloadBuffer.resize(payloadSize);
        boost::asio::async_read(m_socket, boost::asio::buffer(m_payloadBuffer),
            boost::asio::bind_executor(m_strand,
            [self, type](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    self->processMessage(type, self->m_payloadBuffer);
                    self->readHeader(); // קריאת ההודעה הבאה בתור
                } else {
                    self->handleDisconnect();
                }
            }));
    }

    void processMessage(NetworkMessageType type, const std::vector<std::uint8_t>& payload) {
        if (type == NetworkMessageType::JOIN_MATCH_REQUEST) {
            m_matchManager.registerPlayer(shared_from_this());
        } 
        else if (type == NetworkMessageType::GAME_MOVE) {
            if (m_matchId == 0) return;

            NetworkMovePacket packet;
            if (payload.size() >= sizeof(NetworkMovePacket)) {
                std::memcpy(&packet, payload.data(), sizeof(NetworkMovePacket));
                
                auto match = m_matchManager.getMatch(m_matchId);
                if (match) {
                    match->handlePlayerMove(shared_from_this(), packet);
                }
            }
        }
    }

    void writePacket(NetworkMessageType type, const std::vector<std::uint8_t>& payload) {
        std::uint32_t payloadSize = static_cast<std::uint32_t>(payload.size());
        
        std::vector<std::uint8_t> writeBuffer;
        writeBuffer.resize(5 + payloadSize);
        
        writeBuffer[0] = static_cast<std::uint8_t>(type);
        std::memcpy(&writeBuffer[1], &payloadSize, sizeof(payloadSize));
        
        if (payloadSize > 0) {
            std::memcpy(&writeBuffer[5], payload.data(), payloadSize);
        }

        auto self = shared_from_this();
        boost::asio::async_write(m_socket, boost::asio::buffer(writeBuffer),
            boost::asio::bind_executor(m_strand,
            [self, writeBuffer](boost::system::error_code ec, std::size_t) {
                if (ec) {
                    std::cerr << "[Session] Write error: " << ec.message() << std::endl;
                }
            }));
    }

    void handleDisconnect() {
        m_matchManager.unregisterPlayer(shared_from_this());
    }
};

class NetworkServer {
private:
    tcp::acceptor m_acceptor;
    MatchManager& m_matchManager;

public:
    NetworkServer(boost::asio::io_context& ioContext, std::uint16_t port, MatchManager& matchManager)
        : m_acceptor(ioContext, tcp::endpoint(tcp::v4(), port)), m_matchManager(matchManager) {
        startAccept();
    }

private:
    void startAccept() {
        m_acceptor.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::cout << "[Server] Accepted new connection." << std::endl;
                    auto session = std::make_shared<NetworkSession>(std::move(socket), m_matchManager);
                    session->start();
                }
                startAccept();
            });
    }
};

// =================================────────────────────────────────============
// מימושי Inline לפתרון תלויות מעגליות (Circular Dependencies)
// =================================────────────────────────────────============

inline void LiveMatch::handlePlayerMove(std::shared_ptr<NetworkSession> sender, const NetworkMovePacket& packet) {
    ActionRequest request = Serializer::deserializeToRequest(packet);
    
    std::vector<ActionRequest> requests = { request };
    std::vector<ActionResult> results = m_engine->processActionRequests(requests);

    if (!results.empty()) {
        ActionResult result = results.front();

        std::vector<std::uint8_t> resultPayload(sizeof(ActionResult));
        std::memcpy(resultPayload.data(), &result, sizeof(ActionResult));
        sender->sendPacket(NetworkMessageType::MOVE_RESULT, resultPayload);

        if (result.status == ActionStatus::Accepted) {
            std::vector<std::uint8_t> movePayload(sizeof(NetworkMovePacket));
            std::memcpy(movePayload.data(), &packet, sizeof(NetworkMovePacket));

            auto white = m_whiteSession.lock();
            auto black = m_blackSession.lock();

            if (sender == white && black) {
                black->sendPacket(NetworkMessageType::GAME_MOVE, movePayload);
            } else if (sender == black && white) {
                white->sendPacket(NetworkMessageType::GAME_MOVE, movePayload);
            }
        }
    }
}

inline void MatchManager::registerPlayer(std::shared_ptr<NetworkSession> session) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_waitingPlayers.empty() && m_waitingPlayers.front() == session) {
        return;
    }

    m_waitingPlayers.push(session);
    std::cout << "[Lobby] Player queued. Queue size: " << m_waitingPlayers.size() << std::endl;

    if (m_waitingPlayers.size() >= 2) {
        auto player1 = m_waitingPlayers.front();
        m_waitingPlayers.pop();
        auto player2 = m_waitingPlayers.front();
        m_waitingPlayers.pop();

        startNewMatch(player1, player2);
    }
}

inline void MatchManager::unregisterPlayer(std::shared_ptr<NetworkSession> session) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::queue<std::shared_ptr<NetworkSession>> tempQueue;
    while (!m_waitingPlayers.empty()) {
        auto player = m_waitingPlayers.front();
        m_waitingPlayers.pop();
        if (player != session) {
            tempQueue.push(player);
        }
    }
    m_waitingPlayers = std::move(tempQueue);
    
    std::uint64_t matchId = session->matchId();
    if (matchId != 0) {
        auto it = m_matches.find(matchId);
        if (it != m_matches.end()) {
            it->second->stop();
            m_matches.erase(it);
            std::cout << "[Lobby] Match " << matchId << " cleaned up due to disconnect." << std::endl;
        }
    }
}

inline void MatchManager::startNewMatch(std::shared_ptr<NetworkSession> player1, std::shared_ptr<NetworkSession> player2) {
    std::uint64_t id = m_nextMatchId++;

    std::string startBoard =
        "bR bN bB bQ bK bB bN bR\n"
        "bP bP bP bP bP bP bP bP\n"
        ". . . . . . . .\n"
        ". . . . . . . .\n"
        ". . . . . . . .\n"
        ". . . . . . . .\n"
        "wP wP wP wP wP wP wP wP\n"
        "wR wN wB wQ wK wB wN wR\n";

    auto board = BoardParser::parse(startBoard);
    auto ruleEngine = std::make_shared<RuleEngine>(board);
    
    GameConfig config;
    config.allowSimultaneousMovement = true;
    config.enablePremoves = true;
    config.allowJumping = true;

    auto engine = std::make_shared<GameEngine>(board, ruleEngine, config);
    auto match = std::make_shared<LiveMatch>(m_ioContext, id, engine);

    player1->setMatchId(id);
    player1->setColor(PlayerColor::White);

    player2->setMatchId(id);
    player2->setColor(PlayerColor::Black);

    match->setPlayers(player1, player2);
    m_matches[id] = match;

    std::vector<std::uint8_t> payloadWhite(9);
    std::uint8_t colorWhite = static_cast<std::uint8_t>(PlayerColor::White);
    std::memcpy(payloadWhite.data(), &id, 8);
    std::memcpy(payloadWhite.data() + 8, &colorWhite, 1);

    std::vector<std::uint8_t> payloadBlack(9);
    std::uint8_t colorBlack = static_cast<std::uint8_t>(PlayerColor::Black);
    std::memcpy(payloadBlack.data(), &id, 8);
    std::memcpy(payloadBlack.data() + 8, &colorBlack, 1);

    player1->sendPacket(NetworkMessageType::MATCH_FOUND, payloadWhite);
    player2->sendPacket(NetworkMessageType::MATCH_FOUND, payloadBlack);

    match->start();

    std::cout << "[MatchManager] Created match " << id << " between two players." << std::endl;
}

} // namespace kungfu