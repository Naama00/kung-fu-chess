// players/network/NetworkPlayer.cpp
#include "NetworkPlayer.hpp"
#include "ClientConfig.hpp"
#include "ClientAuth.hpp"                       
#include "../../server/network/Serializer.hpp" 
#include <iostream>

namespace kungfu
{
    NetworkPlayer::NetworkPlayer(boost::asio::io_context &ioContext, const std::string &host, const std::string &port,
                                 bool isSpectator, std::uint64_t spectateMatchId, std::uint64_t onlineRoomCode)
        : m_ioContext(ioContext),
          m_socket(ioContext),
          m_strand(boost::asio::make_strand(m_socket.get_executor())),
          m_host(host),
          m_port(port),
          m_recvBuffer(kMaxPayloadSize),
          m_heartbeatTimer(m_strand),
          m_retryTimer(m_strand),
          m_isSpectator(isSpectator),
          m_spectateMatchId(spectateMatchId),
          m_onlineRoomCode(onlineRoomCode) 
    {
        m_matchId.store(0);
        m_assignedColor.store(PlayerColor::White);
        m_connected.store(false);
        m_matchEnded.store(false);
        m_opponentDisconnected.store(false);
        m_nextRequestId.store(1);
        m_isOpponentDisconnected.store(false);
        m_disconnectCountdown.store(ClientConfig::kDefaultDisconnectCountdownSec);
    }

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
            if (m_isSpectator) {
                sendSpectateRequest();
            } else {
                std::vector<std::uint8_t> payload;
                Serializer::writeU64(payload, m_onlineRoomCode);
                writePacket(NetworkMessageType::JOIN_MATCH_REQUEST, payload);
            }
        }
    }

    void NetworkPlayer::sendSpectateRequest()
    {
        std::vector<std::uint8_t> payload;
        Serializer::writeU64(payload, m_spectateMatchId);
        writePacket(NetworkMessageType::SPECTATE_ROOM_REQUEST, payload);
        std::cout << "[Client] Sent SPECTATE_ROOM_REQUEST for Match ID: " << m_spectateMatchId << std::endl;
    }

    void NetworkPlayer::requestActiveRooms()
    {
        if (!m_connected) return;
        writePacket(NetworkMessageType::ROOM_LIST_REQUEST, {});
    }

    /*
     * Thread-Safe State Query Methods
     * These methods bridge the network Asio strand and the main GUI/game loop thread.
     * They utilize dedicated mutexes to safely transfer updated state without blocking Asio operations.
     */

    std::vector<NetworkPlayer::ClientMatchInfo> NetworkPlayer::getActiveRooms()
    {
        std::lock_guard<std::mutex> lock(m_roomsMutex);
        m_roomsUpdated.store(false);
        return m_activeRooms;
    }

    std::string NetworkPlayer::consumePendingSync()
    {
        std::lock_guard<std::mutex> lock(m_syncMutex);
        m_hasPendingSync.store(false);
        return m_pendingSyncBoard;
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

    /*
     * Central UDP Packet Dispatcher
     * Parses the generic packet header (type + payload size) and routes the payload to the
     * appropriate state update logic. Dispatched messages update either thread-safe atomic flags,
     * thread-bridge mutexes for UI rendering, or internal pending move maps.
     */
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
                if (m_isSpectator) {
                    sendSpectateRequest();
                } else {
                    std::vector<std::uint8_t> joinPayload;
                    Serializer::writeU64(joinPayload, m_onlineRoomCode);
                    writePacket(NetworkMessageType::JOIN_MATCH_REQUEST, joinPayload);
                }
            } else {
                std::cerr << "[Client] Auth failed. Closing UDP socket." << std::endl;
                handleDisconnect();
            }
            break;
        }
        case NetworkMessageType::ROOM_STATE_SYNC:
        {
            std::size_t readOffset = 0;
            std::uint64_t matchId = 0;
            std::string boardStr;

            if (Serializer::readU64(payload, readOffset, matchId) &&
                Serializer::readString(payload, readOffset, boardStr))
            {
                m_matchId.store(matchId);
                
                std::lock_guard<std::mutex> lock(m_syncMutex);
                m_pendingSyncBoard = boardStr;
                m_hasPendingSync.store(true);
                std::cout << "[Client] Received ROOM_STATE_SYNC for Match ID: " << matchId << std::endl;
            }
            break;
        }
        case NetworkMessageType::ROOM_LIST_RESPONSE:
        {
            std::size_t readOffset = 0;
            std::uint32_t count = 0;
            if (Serializer::readU32(payload, readOffset, count)) {
                std::vector<ClientMatchInfo> rooms;
                rooms.reserve(count);
                
                bool parseOk = true;
                for (std::uint32_t i = 0; i < count; ++i) {
                    ClientMatchInfo info{};
                    parseOk &= Serializer::readU64(payload, readOffset, info.matchId);
                    parseOk &= Serializer::readString(payload, readOffset, info.whitePlayer);
                    parseOk &= Serializer::readString(payload, readOffset, info.blackPlayer);
                    
                    if (parseOk) {
                        rooms.push_back(info);
                    }
                }
                
                if (parseOk) {
                    std::lock_guard<std::mutex> lock(m_roomsMutex);
                    m_activeRooms = std::move(rooms);
                    m_roomsUpdated.store(true);
                }
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

                // Remove acknowledged move from pending retransmission queue
                m_pendingMoves.erase(result->requestId);
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

    /*
     * Action Outgoing Pipeline
     * Assigns a unique request ID, constructs the move packet, stores it inside m_pendingMoves for
     * reliable retransmission tracking, and dispatches the packet to the server via the strand.
     */
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
        m_heartbeatTimer.expires_after(ClientConfig::kHeartbeatInterval);
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
        m_retryTimer.expires_after(ClientConfig::kMoveRetryCheckInterval);
        m_retryTimer.async_wait(boost::asio::bind_executor(m_strand,
            [self](const boost::system::error_code& ec) {
                if (!ec && self->m_connected) {
                    self->checkAndRetryMoves();
                    self->startRetryTimer();
                }
            }));
    }

    /*
     * Custom Reliable UDP Retransmission Loop
     * Periodically iterates through unacknowledged move packets in m_pendingMoves.
     * If a move exceeds kMoveRetryTimeout without receiving a MOVE_RESULT ack, it is retransmitted.
     * If retries exceed kMaxMoveRetries, the connection is assumed lost and handleDisconnect() is invoked.
     */
    void NetworkPlayer::checkAndRetryMoves()
    {
        auto now = std::chrono::steady_clock::now();
        for (auto& pair : m_pendingMoves) {
            auto& pm = pair.second;
            if (now - pm.lastSent >= ClientConfig::kMoveRetryTimeout) {
                if (pm.retries >= ClientConfig::kMaxMoveRetries) {
                    std::cerr << "[Client] Move request " << pm.packet.requestId 
                              << " timed out after " << ClientConfig::kMaxMoveRetries 
                              << " retries. Simulating disconnect." << std::endl;
                    handleDisconnect();
                    return; // Return immediately to avoid iterating over cleared/invalidated map
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

        // Safely cancel active timers on disconnect
        boost::system::error_code ec;
        m_heartbeatTimer.cancel(ec);
        m_retryTimer.cancel(ec);
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