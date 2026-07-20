// players/network/NetworkPlayer.cpp
#include "NetworkPlayer.hpp"
#include "../../server/network/Serializer.hpp" // Updated relative include path
#include "ClientAuth.hpp"                       // Updated to local inclusion (now located in players/network/)
#include <iostream>

namespace kungfu
{
    NetworkPlayer::NetworkPlayer(boost::asio::io_context &ioContext, const std::string &host, const std::string &port)
        : m_ioContext(ioContext),
          m_socket(ioContext),
          m_strand(boost::asio::make_strand(m_socket.get_executor())),
          m_host(host),
          m_port(port),
          m_recvBuffer(kMaxPayloadSize),
          m_heartbeatTimer(ioContext),
          m_retryTimer(ioContext) {}

    NetworkPlayer::~NetworkPlayer()
    {
        boost::system::error_code ec;
        m_heartbeatTimer.cancel(ec);
        m_retryTimer.cancel(ec);
        m_socket.close(ec);
    }

    void NetworkPlayer::connectAndJoin()
    {
        auto self = shared_from_this();
        boost::asio::post(m_strand, [self]() { self->doConnect(); });
    }

    void NetworkPlayer::doConnect()
    {
        udp::resolver resolver(m_ioContext);
        boost::system::error_code ec;
        auto endpoints = resolver.resolve(m_host, m_port, ec);
        
        if (ec) {
            std::cerr << "[Client] Host resolution failed: " << ec.message() << std::endl;
            m_connected = false;
            return;
        }

        m_socket.open(udp::v4(), ec);
        if (!ec) {
            m_socket.connect(*endpoints.begin(), ec);
        }

        if (!ec) {
            std::cout << "[Client] Connected to UDP game server!" << std::endl;
            m_connected = true;
            sendJoinRequest();
            startReceive();
            startHeartbeat();
            startRetryTimer(); 
        } else {
            std::cerr << "[Client] UDP Connection failed: " << ec.message() << std::endl;
            m_connected = false;
        }
    }

    void NetworkPlayer::sendJoinRequest()
    {
        if (ClientAuth::isAuthenticated) {
            auto payload = Serializer::serializeAuthRequest(ClientAuth::username, ClientAuth::password);
            writePacket(NetworkMessageType::LOGIN_REQUEST, payload);
        } else {
            writePacket(NetworkMessageType::JOIN_MATCH_REQUEST, {});
        }
    }

    void NetworkPlayer::startReceive()
    {
        auto self = shared_from_this();
        m_socket.async_receive(boost::asio::buffer(m_recvBuffer),
            boost::asio::bind_executor(m_strand,
            [self](boost::system::error_code ec, std::size_t bytesRecvd) {
                if (!ec) {
                    if (bytesRecvd >= kHeaderSize) {
                        self->processDatagram(bytesRecvd);
                    }
                    self->startReceive();
                } else {
                    self->handleDisconnect();
                }
            }));
    }

    void NetworkPlayer::processDatagram(std::size_t bytesRecvd)
    {
        std::size_t offset = 0;
        std::uint8_t rawType = 0;
        std::uint32_t payloadSize = 0;

        bool ok = Serializer::readU8(m_recvBuffer, offset, rawType) &&
                  Serializer::readU32(m_recvBuffer, offset, payloadSize);

        if (!ok || offset + payloadSize > bytesRecvd) {
            return;
        }

        std::vector<std::uint8_t> payload(
            m_recvBuffer.begin() + offset, 
            m_recvBuffer.begin() + offset + payloadSize
        );

        NetworkMessageType type = static_cast<NetworkMessageType>(rawType);

        switch (type)
        {
        case NetworkMessageType::LOGIN_RESPONSE:
        {
            if (!payload.empty() && payload[0] == 1) {
                std::cout << "[Client] UDP silent authentication succeeded!" << std::endl;
                writePacket(NetworkMessageType::JOIN_MATCH_REQUEST, {});
            } else {
                std::cerr << "[Client] Auth failed. Closing UDP socket." << std::endl;
                handleDisconnect();
            }
            break;
        }
        case NetworkMessageType::DISCONNECT_COUNTDOWN:
        {
            if (!payload.empty()) {
                m_disconnectCountdown.store(static_cast<int>(payload[0]));
                m_isOpponentDisconnected.store(true);
            }
            break;
        }
        case NetworkMessageType::MATCH_FOUND:
        {
            std::size_t readOffset = 0;
            std::uint64_t matchId = 0;
            std::uint8_t colorVal = 0;

            if (Serializer::readU64(payload, readOffset, matchId) &&
                Serializer::readU8(payload, readOffset, colorVal))
            {
                m_matchId.store(matchId);
                m_assignedColor.store(static_cast<PlayerColor>(colorVal));
                std::cout << "[Client] UDP Match started! ID: " << matchId << std::endl;
            }
            break;
        }
        case NetworkMessageType::GAME_MOVE:
        {
            m_isOpponentDisconnected.store(false);
            auto packet = Serializer::deserializeMovePacket(payload);
            if (packet.has_value()) {
                ActionRequest request = Serializer::deserializeToRequest(*packet);
                std::lock_guard<std::mutex> lock(m_mutex);
                m_incomingActions.push_back(request);
            }
            break;
        }
        case NetworkMessageType::MOVE_RESULT:
        {
            auto result = Serializer::deserializeToResult(payload);
            if (result.has_value()) {
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_incomingResults.push_back(*result);
                }

                // Remove the confirmed request from pending queue
                std::uint64_t reqId = result->requestId;
                auto self = shared_from_this();
                boost::asio::post(m_strand, [self, reqId]() {
                    self->m_pendingMoves.erase(reqId);
                });
            }
            break;
        }
        case NetworkMessageType::GAME_OVER:
            m_matchEnded.store(true);
            m_isOpponentDisconnected.store(false);
            break;

        case NetworkMessageType::OPPONENT_DISCONNECTED:
            m_opponentDisconnected.store(true);
            m_isOpponentDisconnected.store(false);
            break;

        case NetworkMessageType::MATCH_TIMEOUT:
            m_opponentDisconnected.store(true); 
            break;

        case NetworkMessageType::HEARTBEAT:
        case NetworkMessageType::JOIN_MATCH_REQUEST:
        case NetworkMessageType::REGISTER_RESPONSE:
            break;
        }
    }

    void NetworkPlayer::sendMoveToServer(const PlayerAction &action)
    {
        if (!m_connected || m_matchId.load() == 0) return;

        NetworkMovePacket packet{};
        packet.matchId = m_matchId.load();
        packet.requestId = m_nextRequestId.fetch_add(1);
        packet.playerColor = static_cast<std::uint8_t>(m_assignedColor.load());
        packet.from.x = action.from.col();
        packet.from.y = action.from.row();
        packet.to.x = action.to.col();
        packet.to.y = action.to.row();

        auto self = shared_from_this();
        boost::asio::post(m_strand, [self, packet]() {
            // Save inside the tracking map for reliable execution check
            PendingMove pm{packet, std::chrono::steady_clock::now(), 0};
            self->m_pendingMoves[packet.requestId] = pm;

            auto payload = Serializer::serializeMovePacket(packet);
            self->writePacket(NetworkMessageType::GAME_MOVE, payload);
        });
    }

    void NetworkPlayer::writePacket(NetworkMessageType type, const std::vector<std::uint8_t> &payload)
    {
        auto frame = std::make_shared<std::vector<std::uint8_t>>(Serializer::buildFrame(type, payload));
        auto self = shared_from_this();
        m_socket.async_send(boost::asio::buffer(*frame),
            boost::asio::bind_executor(m_strand,
            [self, frame](boost::system::error_code ec, std::size_t) {
                if (ec) {
                    std::cerr << "[Client] UDP Write error: " << ec.message() << std::endl;
                }
            }));
    }

    void NetworkPlayer::startHeartbeat()
    {
        auto self = shared_from_this();
        m_heartbeatTimer.expires_after(std::chrono::seconds(5));
        m_heartbeatTimer.async_wait(boost::asio::bind_executor(m_strand,
            [self](const boost::system::error_code& ec) {
                if (!ec && self->m_connected) {
                    self->writePacket(NetworkMessageType::HEARTBEAT, {});
                    self->startHeartbeat();
                }
            }));
    }

    void NetworkPlayer::startRetryTimer()
    {
        auto self = shared_from_this();
        m_retryTimer.expires_after(std::chrono::milliseconds(100));
        m_retryTimer.async_wait(boost::asio::bind_executor(m_strand,
            [self](const boost::system::error_code& ec) {
                if (!ec && self->m_connected) {
                    self->checkAndRetryMoves();
                    self->startRetryTimer();
                }
            }));
    }

    void NetworkPlayer::checkAndRetryMoves()
    {
        auto now = std::chrono::steady_clock::now();
        for (auto& pair : m_pendingMoves) {
            auto& pm = pair.second;
            // Retry if no server confirmation was received within 200ms
            if (now - pm.lastSent >= std::chrono::milliseconds(200)) {
                if (pm.retries >= 5) {
                    std::cerr << "[Client] Move request " << pm.packet.requestId 
                              << " timed out after 5 retries. Simulating disconnect." << std::endl;
                    handleDisconnect();
                    break;
                }
                pm.retries++;
                pm.lastSent = now;
                std::cout << "[Client] Re-sending lost UDP move request: " << pm.packet.requestId 
                          << " (Attempt " << pm.retries << ")" << std::endl;

                auto payload = Serializer::serializeMovePacket(pm.packet);
                writePacket(NetworkMessageType::GAME_MOVE, payload);
            }
        }
    }

    void NetworkPlayer::handleDisconnect()
    {
        m_connected = false;
        m_matchId = 0;
        m_isOpponentDisconnected.store(false);
        m_pendingMoves.clear();
    }

    std::vector<ActionRequest> NetworkPlayer::decideActions(const view::GameSnapshot &snapshot)
    {
        (void)snapshot;
        std::lock_guard<std::mutex> lock(m_mutex);
        auto actions = std::move(m_incomingActions);
        m_incomingActions.clear();
        return actions;
    }

    std::vector<ActionResult> NetworkPlayer::pollResults()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto results = std::move(m_incomingResults);
        m_incomingResults.clear();
        return results;
    }
} // namespace kungfu