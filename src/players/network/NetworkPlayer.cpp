#include "NetworkPlayer.hpp"
#include "../../server/Serializer.hpp"
#include <iostream>
#include <cstring>

namespace kungfu
{

    NetworkPlayer::NetworkPlayer(boost::asio::io_context &ioContext, const std::string &host, const std::string &port)
        : m_ioContext(ioContext),
          m_socket(ioContext),
          m_strand(boost::asio::make_strand(m_socket.get_executor())),
          m_host(host),
          m_port(port)
    {
        m_headerBuffer.resize(5); // כותרת בגודל קבוע: בית אחד לסוג, 4 בתים לאורך
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
        // שלח בקשה מסוג JOIN_MATCH_REQUEST עם גוף ריק (payloadSize = 0)
        writePacket(NetworkMessageType::JOIN_MATCH_REQUEST, {});
    }

    void NetworkPlayer::readHeader()
    {
        auto self = shared_from_this();
        boost::asio::async_read(m_socket, boost::asio::buffer(m_headerBuffer),
                                boost::asio::bind_executor(m_strand,
                                                           [self](boost::system::error_code ec, std::size_t)
                                                           {
                                                               if (!ec)
                                                               {
                                                                   NetworkMessageType type = static_cast<NetworkMessageType>(self->m_headerBuffer[0]);

                                                                   std::uint32_t payloadSize = 0;
                                                                   std::memcpy(&payloadSize, &self->m_headerBuffer[1], sizeof(payloadSize));

                                                                   self->readPayload(type, payloadSize);
                                                               }
                                                               else
                                                               {
                                                                   self->handleDisconnect();
                                                               }
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
        if (type == NetworkMessageType::MATCH_FOUND)
        {
            if (payload.size() >= 9)
            {
                std::memcpy(&m_matchId, payload.data(), 8);
                std::uint8_t colorVal = payload[8];
                m_assignedColor = static_cast<PlayerColor>(colorVal);

                std::cout << "[Client] Match found! ID: " << m_matchId
                          << " Color: " << (m_assignedColor == PlayerColor::White ? "White" : "Black")
                          << std::endl;
            }
        }
        else if (type == NetworkMessageType::GAME_MOVE)
        {
            if (payload.size() >= sizeof(NetworkMovePacket))
            {
                NetworkMovePacket packet;
                std::memcpy(&packet, payload.data(), sizeof(NetworkMovePacket));

                // תרגום חבילת המהלך שנשלחה מהשרת לבקשת מנוע מקומית
                ActionRequest request = Serializer::deserializeToRequest(packet);

                // הוספה בצורה בטוחה לתור המהלכים הממתינים
                std::lock_guard<std::mutex> lock(m_mutex);
                m_incomingActions.push_back(request);
            }
        }
    }

    void NetworkPlayer::sendMoveToServer(const PlayerAction &action)
    {
        if (!m_connected || m_matchId == 0)
            return;

        NetworkMovePacket packet;
        packet.matchId = m_matchId.load(); 
        packet.requestId = m_nextRequestId++;
        packet.playerColor = static_cast<std::uint8_t>(m_assignedColor.load());

        // מיפוי קואורדינטות: x מייצג עמודה (col), y מייצג שורה (row)
        packet.from.x = action.from.col();
        packet.from.y = action.from.row();
        packet.to.x = action.to.col();
        packet.to.y = action.to.row();

        std::vector<std::uint8_t> payload(sizeof(NetworkMovePacket));
        std::memcpy(payload.data(), &packet, sizeof(NetworkMovePacket));

        auto self = shared_from_this();
        boost::asio::post(m_strand, [self, payload]()
                          { self->writePacket(NetworkMessageType::GAME_MOVE, payload); });
    }

    void NetworkPlayer::writePacket(NetworkMessageType type, const std::vector<std::uint8_t> &payload)
    {
        std::uint32_t payloadSize = static_cast<std::uint32_t>(payload.size());

        std::vector<std::uint8_t> writeBuffer;
        writeBuffer.resize(5 + payloadSize);

        writeBuffer[0] = static_cast<std::uint8_t>(type);
        std::memcpy(&writeBuffer[1], &payloadSize, sizeof(payloadSize));

        if (payloadSize > 0)
        {
            std::memcpy(&writeBuffer[5], payload.data(), payloadSize);
        }

        auto self = shared_from_this();
        boost::asio::async_write(m_socket, boost::asio::buffer(writeBuffer),
                                 boost::asio::bind_executor(m_strand,
                                                            [self, writeBuffer](boost::system::error_code ec, std::size_t)
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

    // מימוש המתודה התקנית של ממשק IPlayer
    std::vector<ActionRequest> NetworkPlayer::decideActions(const view::GameSnapshot &snapshot)
    {
        (void)snapshot;
        std::lock_guard<std::mutex> lock(m_mutex);

        // החזרת כל המהלכים שהצטברו מהרשת וניקוי התור באותו פריים
        auto actions = m_incomingActions;
        m_incomingActions.clear();
        return actions;
    }

} // namespace kungfu