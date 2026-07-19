#include "NetworkPlayer.hpp"
#include "../../server/Serializer.hpp"
#include <iostream>

namespace kungfu
{

    NetworkPlayer::NetworkPlayer(boost::asio::io_context &ioContext, const std::string &host, const std::string &port)
        : m_ioContext(ioContext),
          m_socket(ioContext),
          m_strand(boost::asio::make_strand(m_socket.get_executor())),
          m_host(host),
          m_port(port)
    {
        m_headerBuffer.resize(kHeaderSize);
    }

    NetworkPlayer::~NetworkPlayer()
    {
        boost::system::error_code ec;
        m_socket.close(ec);
    }

    void NetworkPlayer::connectAndJoin()
    {
        auto self = shared_from_this();
        boost::asio::post(m_strand, [self]()
                           { self->doConnect(); });
    }

    void NetworkPlayer::doConnect()
    {
        tcp::resolver resolver(m_ioContext);
        auto endpoints = resolver.resolve(m_host, m_port);
        auto self = shared_from_this();

        boost::asio::async_connect(m_socket, endpoints,
                                    boost::asio::bind_executor(m_strand,
                                                                [self](boost::system::error_code ec, const tcp::endpoint &)
                                                                {
                                                                    if (!ec)
                                                                    {
                                                                        std::cout << "[Client] Connected to game server successfully!" << std::endl;
                                                                        self->m_connected = true;
                                                                        self->sendJoinRequest();
                                                                        self->readHeader();
                                                                    }
                                                                    else
                                                                    {
                                                                        std::cerr << "[Client] Connection failed: " << ec.message() << std::endl;
                                                                        self->m_connected = false;
                                                                    }
                                                                }));
    }

    void NetworkPlayer::sendJoinRequest()
    {
        writePacket(NetworkMessageType::JOIN_MATCH_REQUEST, {});
    }

    void NetworkPlayer::readHeader()
    {
        auto self = shared_from_this();
        boost::asio::async_read(m_socket, boost::asio::buffer(m_headerBuffer),
                                 boost::asio::bind_executor(m_strand,
                                                             [self](boost::system::error_code ec, std::size_t)
                                                             {
                                                                 if (ec)
                                                                 {
                                                                     self->handleDisconnect();
                                                                     return;
                                                                 }

                                                                 // קריאת הכותרת חייבת להיות עקבית עם Serializer::buildFrame
                                                                 // שבו משתמש השרת (Big-Endian מפורש) - לא memcpy גולמי.
                                                                 std::size_t offset = 0;
                                                                 std::uint8_t rawType = 0;
                                                                 std::uint32_t payloadSize = 0;

                                                                 bool ok = Serializer::readU8(self->m_headerBuffer, offset, rawType) &&
                                                                           Serializer::readU32(self->m_headerBuffer, offset, payloadSize);

                                                                 if (!ok || payloadSize > kMaxPayloadSize)
                                                                 {
                                                                     std::cerr << "[Client] Invalid or oversized header from server. Disconnecting." << std::endl;
                                                                     self->handleDisconnect();
                                                                     return;
                                                                 }

                                                                 self->readPayload(static_cast<NetworkMessageType>(rawType), payloadSize);
                                                             }));
    }

    void NetworkPlayer::readPayload(NetworkMessageType type, std::uint32_t payloadSize)
    {
        auto self = shared_from_this();

        if (payloadSize == 0)
        {
            processMessage(type, {});
            readHeader();
            return;
        }

        m_payloadBuffer.resize(payloadSize);
        boost::asio::async_read(m_socket, boost::asio::buffer(m_payloadBuffer),
                                 boost::asio::bind_executor(m_strand,
                                                             [self, type](boost::system::error_code ec, std::size_t)
                                                             {
                                                                 if (!ec)
                                                                 {
                                                                     self->processMessage(type, self->m_payloadBuffer);
                                                                     self->readHeader();
                                                                 }
                                                                 else
                                                                 {
                                                                     self->handleDisconnect();
                                                                 }
                                                             }));
    }

    void NetworkPlayer::processMessage(NetworkMessageType type, const std::vector<std::uint8_t> &payload)
    {
        switch (type)
        {
        case NetworkMessageType::MATCH_FOUND:
        {
            std::size_t offset = 0;
            std::uint64_t matchId = 0;
            std::uint8_t colorVal = 0;

            if (Serializer::readU64(payload, offset, matchId) &&
                Serializer::readU8(payload, offset, colorVal))
            {
                // כתיבה דרך .store() ולא memcpy ישיר לתוך std::atomic:
                // memcpy עוקף את מנגנון האטומיות ואינו מבטיח סמנטיקת
                // memory-ordering תקינה, גם אם בפועל עובד על רוב הפלטפורמות.
                m_matchId.store(matchId);
                m_assignedColor.store(static_cast<PlayerColor>(colorVal));

                std::cout << "[Client] Match found! ID: " << matchId
                           << " Color: " << (m_assignedColor.load() == PlayerColor::White ? "White" : "Black")
                           << std::endl;
            }
            else
            {
                std::cerr << "[Client] Malformed MATCH_FOUND payload. Ignoring." << std::endl;
            }
            break;
        }

        case NetworkMessageType::GAME_MOVE:
        {
            auto packet = Serializer::deserializeMovePacket(payload);
            if (!packet.has_value())
            {
                std::cerr << "[Client] Malformed GAME_MOVE payload. Ignoring." << std::endl;
                break;
            }

            ActionRequest request = Serializer::deserializeToRequest(*packet);

            std::lock_guard<std::mutex> lock(m_mutex);
            m_incomingActions.push_back(request);
            break;
        }

        case NetworkMessageType::MOVE_RESULT:
        {
            auto result = Serializer::deserializeToResult(payload);
            if (!result.has_value())
            {
                std::cerr << "[Client] Malformed MOVE_RESULT payload. Ignoring." << std::endl;
                break;
            }

            std::lock_guard<std::mutex> lock(m_mutex);
            m_incomingResults.push_back(*result);
            break;
        }

        case NetworkMessageType::GAME_OVER:
            std::cout << "[Client] Game over." << std::endl;
            m_matchEnded.store(true);
            break;

        case NetworkMessageType::OPPONENT_DISCONNECTED:
            std::cout << "[Client] Opponent disconnected." << std::endl;
            m_opponentDisconnected.store(true);
            break;

        case NetworkMessageType::HEARTBEAT:
        case NetworkMessageType::JOIN_MATCH_REQUEST:
            // הודעות שהלקוח לא אמור לקבל מהשרת - מתעלמים בשקט.
            break;
        }
    }

    void NetworkPlayer::sendMoveToServer(const PlayerAction &action)
    {
        if (!m_connected || m_matchId.load() == 0)
            return;

        NetworkMovePacket packet{};
        packet.matchId = m_matchId.load();
        packet.requestId = m_nextRequestId.fetch_add(1);
        packet.playerColor = static_cast<std::uint8_t>(m_assignedColor.load());

        // מיפוי קואורדינטות: x מייצג עמודה (col), y מייצג שורה (row)
        packet.from.x = action.from.col();
        packet.from.y = action.from.row();
        packet.to.x = action.to.col();
        packet.to.y = action.to.row();

        // סריאליזציה מפורשת (33 בתים קבועים) - לא memcpy של ה-struct,
        // כדי להתאים בדיוק לפרוטוקול שהשרת מצפה לו ולהימנע מתלות ב-padding.
        auto payload = Serializer::serializeMovePacket(packet);

        auto self = shared_from_this();
        boost::asio::post(m_strand, [self, payload]()
                           { self->writePacket(NetworkMessageType::GAME_MOVE, payload); });
    }

    void NetworkPlayer::writePacket(NetworkMessageType type, const std::vector<std::uint8_t> &payload)
    {
        // Serializer::buildFrame בונה כותרת + payload בפורמט זהה לזה שהשרת
        // קורא (Big-Endian מפורש), כך שאין סיכוי לאי-התאמת endianness.
        auto frame = std::make_shared<std::vector<std::uint8_t>>(Serializer::buildFrame(type, payload));

        auto self = shared_from_this();
        boost::asio::async_write(m_socket, boost::asio::buffer(*frame),
                                  boost::asio::bind_executor(m_strand,
                                                              [self, frame](boost::system::error_code ec, std::size_t)
                                                              {
                                                                  if (ec)
                                                                  {
                                                                      std::cerr << "[Client] Write error: " << ec.message() << std::endl;
                                                                  }
                                                              }));
    }

    void NetworkPlayer::handleDisconnect()
    {
        std::cout << "[Client] Disconnected from server." << std::endl;
        m_connected = false;
        m_matchId = 0;
    }

    std::vector<ActionRequest> NetworkPlayer::decideActions(const view::GameSnapshot &snapshot)
    {
        (void)snapshot;
        std::lock_guard<std::mutex> lock(m_mutex);

        auto actions = std::move(m_incomingActions);
        m_incomingActions.clear();
        return actions;
    }

} // namespace kungfu