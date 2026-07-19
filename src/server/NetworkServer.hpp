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
        m_headerBuffer.resize(kHeaderSize);
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

    // בונה את ה-frame השלם (כותרת + payload) פעם אחת ומעביר אותו הלאה
    // כ-shared_ptr, כדי לא להעתיק את הנתונים פעם נוספת בתוך ה-strand
    // (בגרסה הקודמת הפרמטר הועתק פעם אחת בעת ה-capture, ואז שוב בתוך
    // writePacket - שתי העתקות מיותרות לכל שליחה).
    void sendPacket(NetworkMessageType type, const std::vector<std::uint8_t>& payload) {
        auto frame = std::make_shared<std::vector<std::uint8_t>>(Serializer::buildFrame(type, payload));
        auto self = shared_from_this();
        boost::asio::post(m_strand, [self, frame]() {
            self->writeFrame(frame);
        });
    }

private:
    void readHeader() {
        auto self = shared_from_this();
        boost::asio::async_read(m_socket, boost::asio::buffer(m_headerBuffer),
            boost::asio::bind_executor(m_strand,
            [self](boost::system::error_code ec, std::size_t) {
                if (ec) {
                    self->handleDisconnect();
                    return;
                }

                std::size_t offset = 0;
                std::uint8_t rawType = 0;
                std::uint32_t payloadSize = 0;

                bool ok = Serializer::readU8(self->m_headerBuffer, offset, rawType) &&
                          Serializer::readU32(self->m_headerBuffer, offset, payloadSize);

                if (!ok || payloadSize > kMaxPayloadSize) {
                    // כותרת פגומה, או שחקן/תוקף שמנסה לגרום להקצאת זיכרון
                    // ענקית - מנתקים את החיבור במקום לנסות להמשיך.
                    std::cerr << "[Session] Invalid or oversized payload announced ("
                              << payloadSize << " bytes). Disconnecting." << std::endl;
                    self->handleDisconnect();
                    return;
                }

                self->readPayload(static_cast<NetworkMessageType>(rawType), payloadSize);
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

            auto packet = Serializer::deserializeMovePacket(payload);
            if (!packet.has_value()) {
                std::cerr << "[Session] Received malformed GAME_MOVE payload. Ignoring." << std::endl;
                return;
            }

            auto match = m_matchManager.getMatch(m_matchId);
            if (match) {
                match->handlePlayerMove(shared_from_this(), *packet);
            }
        }
    }

    void writeFrame(const std::shared_ptr<std::vector<std::uint8_t>>& frame) {
        auto self = shared_from_this();
        boost::asio::async_write(m_socket, boost::asio::buffer(*frame),
            boost::asio::bind_executor(m_strand,
            [self, frame](boost::system::error_code ec, std::size_t) {
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

inline void LiveMatch::handlePlayerMoveInternal(std::shared_ptr<NetworkSession> sender,
                                                 const NetworkMovePacket& packet) {
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

        // שליחת עדכון התנועה לכל השחקנים המחוברים למשחק (גם ליוזם וגם ליריב)
        if (white) {
            white->sendPacket(NetworkMessageType::GAME_MOVE, movePayload);
        }
        if (black) {
            black->sendPacket(NetworkMessageType::GAME_MOVE, movePayload);
        }
    }
}

inline void LiveMatch::notifyGameOver() {
    auto white = whiteSession();
    auto black = blackSession();

    if (white) {
        white->sendPacket(NetworkMessageType::GAME_OVER, {});
    }
    if (black) {
        black->sendPacket(NetworkMessageType::GAME_OVER, {});
    }
}

inline void MatchManager::registerPlayer(std::shared_ptr<NetworkSession> session) {
    std::shared_ptr<NetworkSession> player1 = nullptr;
    std::shared_ptr<NetworkSession> player2 = nullptr;
    bool shouldStart = false;

    // צמצום הנעילה אך ורק לניהול התור באמצעות בלוק סגור {}
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_waitingSet.count(session) > 0) {
            return;
        }

        m_waitingQueue.push(session);
        m_waitingSet.insert(session);
        std::cout << "[Lobby] Player queued. Queue size: " << m_waitingQueue.size() << std::endl;

        if (m_waitingQueue.size() >= 2) {
            player1 = m_waitingQueue.front();
            m_waitingQueue.pop();
            m_waitingSet.erase(player1);

            player2 = m_waitingQueue.front();
            m_waitingQueue.pop();
            m_waitingSet.erase(player2);

            shouldStart = true;
        }
    } // המנעול (lock) משתחרר כאן באופן אוטומטי!

    // כעת אנו יוצרים את המשחק מחוץ לנעילה הגלובלית - בטוח לחלוטין וללא deadlock
    if (shouldStart && player1 && player2) {
        startNewMatch(player1, player2);
    }
}

inline void MatchManager::unregisterPlayer(std::shared_ptr<NetworkSession> session) {
    std::shared_ptr<LiveMatch> matchToClose;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // הסרה מתור ההמתנה, אם השחקן עדיין שם.
        if (m_waitingSet.erase(session) > 0) {
            std::queue<std::shared_ptr<NetworkSession>> filtered;
            while (!m_waitingQueue.empty()) {
                auto player = m_waitingQueue.front();
                m_waitingQueue.pop();
                if (player != session) {
                    filtered.push(player);
                }
            }
            m_waitingQueue = std::move(filtered);
        }

        std::uint64_t matchId = session->matchId();
        if (matchId != 0) {
            auto it = m_matches.find(matchId);
            if (it != m_matches.end()) {
                matchToClose = it->second;
            }
        }
    } // משחררים את המנעול לפני קריאה למשחק, כדי למנוע deadlock:
      // match->stop() יגרום בסופו של דבר לקריאה חוזרת ל-removeMatch,
      // שגם היא נועלת את אותו m_mutex.

    if (matchToClose) {
        auto opponent = (matchToClose->whiteSession() == session)
            ? matchToClose->blackSession()
            : matchToClose->whiteSession();

        if (opponent) {
            opponent->sendPacket(NetworkMessageType::OPPONENT_DISCONNECTED, {});
        }

        matchToClose->stop(); // יפעיל את ה-callback שמסיר את המשחק מהמפה
        std::cout << "[Lobby] Match " << matchToClose->matchId()
                  << " closed due to player disconnect." << std::endl;
    }
}

inline std::shared_ptr<LiveMatch> MatchManager::startNewMatch(std::shared_ptr<NetworkSession> player1,
                                                                std::shared_ptr<NetworkSession> player2) {
    std::uint64_t id = m_nextMatchId++;

    static const std::string kStartBoard =
        "bR bN bB bQ bK bB bN bR\n"
        "bP bP bP bP bP bP bP bP\n"
        ". . . . . . . .\n"
        ". . . . . . . .\n"
        ". . . . . . . .\n"
        ". . . . . . . .\n"
        "wP wP wP wP wP wP wP wP\n"
        "wR wN wB wQ wK wB wN wR\n";

    auto board = BoardParser::parse(kStartBoard);
    auto ruleEngine = std::make_shared<RuleEngine>(board);

    GameConfig config;
    config.allowSimultaneousMovement = true;
    config.enablePremoves = true;
    config.allowJumping = true;

    auto engine = std::make_shared<GameEngine>(board, ruleEngine, config);
    auto match = std::make_shared<LiveMatch>(m_ioContext, id, engine);

    // ברגע שהמשחק יסתיים (בכל דרך), נסיר אותו אוטומטית מהמפה. שומר על
    // MatchManager כ"מקור האמת" היחיד ל-cleanup, בלי תלות בכך שמישהו
    // יזכור לקרוא לו באופן ידני מכל נקודת יציאה אפשרית.
    match->setOnMatchEnded([this](std::uint64_t finishedMatchId) {
        removeMatch(finishedMatchId);
    });

    player1->setMatchId(id);
    player1->setColor(PlayerColor::White);

    player2->setMatchId(id);
    player2->setColor(PlayerColor::Black);

    match->setPlayers(player1, player2);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_matches[id] = match;
    }

    auto buildMatchFoundPayload = [id](PlayerColor color) {
        std::vector<std::uint8_t> payload;
        Serializer::writeU64(payload, id);
        Serializer::writeU8(payload, static_cast<std::uint8_t>(color));
        return payload;
    };

    player1->sendPacket(NetworkMessageType::MATCH_FOUND, buildMatchFoundPayload(PlayerColor::White));
    player2->sendPacket(NetworkMessageType::MATCH_FOUND, buildMatchFoundPayload(PlayerColor::Black));

    match->start();

    std::cout << "[MatchManager] Created match " << id << " between two players." << std::endl;

    return match;
}

} // namespace kungfu