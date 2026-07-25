// players/network/NetworkPlayer.cpp
#include "NetworkPlayer.hpp"
#include "ClientConfig.hpp"
#include "ClientAuth.hpp"
#include "../../server/network/Serializer.hpp"
#include <iostream>

namespace kungfu
{
    NetworkPlayer::NetworkPlayer(boost::asio::io_context &ioContext, const std::string &host, const std::string &port,
                                 bool isSpectator, std::uint64_t spectateMatchId, std::uint64_t onlineRoomCode,
                                 bool deferJoin)
        : m_ioContext(ioContext),
          m_strand(boost::asio::make_strand(ioContext.get_executor())),
          m_controlSocket(ioContext),
          m_realtimeSocket(ioContext),
          m_realtimeRecvBuffer(kMaxPayloadSize),
          m_host(host),
          m_port(port),
          m_heartbeatTimer(m_strand),
          m_retryTimer(m_strand),
          m_isSpectator(isSpectator),
          m_spectateMatchId(spectateMatchId),
          m_onlineRoomCode(onlineRoomCode),
          m_deferJoin(deferJoin)
    {
        m_matchId.store(0);
        m_assignedColor.store(PlayerColor::White);
        m_connected.store(false);
        m_matchStarted.store(false);
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
        m_controlSocket.close(ec);
        m_realtimeSocket.close(ec);
    }

    void NetworkPlayer::connectAndJoin()
    {
        auto self = shared_from_this();
        boost::asio::post(m_strand, [self]()
                          { self->doConnect(); });
    }

    void NetworkPlayer::doConnect()
    {
        auto resolver = std::make_shared<tcp::resolver>(m_ioContext);
        auto self = shared_from_this();

        resolver->async_resolve(m_host, m_port,
                                boost::asio::bind_executor(m_strand,
                                                           [self, resolver](boost::system::error_code ec, tcp::resolver::results_type endpoints)
                                                           {
                                                               if (ec)
                                                               {
                                                                   std::cerr << "[Client] Host resolution failed: " << ec.message() << std::endl;
                                                                   self->m_connected = false;
                                                                   {
                                                                       std::lock_guard<std::mutex> lock(self->m_loginMutex);
                                                                       self->m_loginMessage = "Could not resolve server address";
                                                                   }
                                                                   self->m_loginStatus.store(LoginStatus::Failed);
                                                                   return;
                                                               }

                                                               boost::asio::async_connect(self->m_controlSocket, endpoints,
                                                                                          boost::asio::bind_executor(self->m_strand,
                                                                                                                     [self](boost::system::error_code connectEc, const tcp::endpoint &)
                                                                                                                     {
                                                                                                                         if (!connectEc)
                                                                                                                         {
                                                                                                                             std::cout << "[Client] Connected to TCP control server!" << std::endl;
                                                                                                                             self->onControlConnected();
                                                                                                                         }
                                                                                                                         else
                                                                                                                         {
                                                                                                                             std::cerr << "[Client] TCP connection failed: " << connectEc.message() << std::endl;
                                                                                                                             self->m_connected = false;
                                                                                                                             {
                                                                                                                                 std::lock_guard<std::mutex> lock(self->m_loginMutex);
                                                                                                                                 self->m_loginMessage = "Server is offline";
                                                                                                                             }
                                                                                                                             self->m_loginStatus.store(LoginStatus::Failed);
                                                                                                                         }
                                                                                                                     }));
                                                           }));
    }

    void NetworkPlayer::onControlConnected()
    {
        m_controlConnected = true;
        m_connected = true;
        startControlReceive();

        if (ClientAuth::isAuthenticated)
        {
            auto payload = Serializer::serializeAuthRequest(ClientAuth::username, ClientAuth::password);
            writePacket(NetworkMessageType::LOGIN_REQUEST, payload);
        }
        else
        {
            sendJoinRequest();
        }
    }

    void NetworkPlayer::connectRealtimeChannel()
    {
        auto resolver = std::make_shared<udp::resolver>(m_ioContext);
        auto self = shared_from_this();

        resolver->async_resolve(m_host, m_port,
                                boost::asio::bind_executor(m_strand,
                                                           [self, resolver](boost::system::error_code ec, udp::resolver::results_type endpoints)
                                                           {
                                                               if (ec)
                                                               {
                                                                   std::cerr << "[Client] Realtime endpoint resolution failed: " << ec.message() << std::endl;
                                                                   self->handleDisconnect();
                                                                   return;
                                                               }

                                                               self->m_realtimeSocket.open(udp::v4(), ec);
                                                               if (!ec)
                                                               {
                                                                   self->m_realtimeSocket.connect(*endpoints.begin(), ec);
                                                               }

                                                               if (!ec)
                                                               {
                                                                   std::cout << "[Client] Connected to UDP realtime server, binding session..." << std::endl;
                                                                   self->startRealtimeReceive();
                                                                   self->sendSessionBind();
                                                               }
                                                               else
                                                               {
                                                                   std::cerr << "[Client] UDP connection failed: " << ec.message() << std::endl;
                                                                   self->handleDisconnect();
                                                               }
                                                           }));
    }

    void NetworkPlayer::sendSessionBind()
    {
        writePacket(NetworkMessageType::SESSION_BIND, Serializer::serializeSessionBind(m_sessionToken));
    }

    void NetworkPlayer::sendJoinRequest()
    {
        if (m_isSpectator)
        {
            sendSpectateRequest();
            return;
        }

        if (m_matchStarted.load() && m_matchId.load() != 0)
        {
            std::cout << "[Client] Already in match " << m_matchId.load()
                      << ", skipping JOIN_MATCH request." << std::endl;
            return;
        }

        std::vector<std::uint8_t> payload;
        Serializer::writeU64(payload, m_onlineRoomCode);
        writePacket(NetworkMessageType::JOIN_MATCH_REQUEST, payload);
    }

    void NetworkPlayer::sendSpectateRequest()
    {
        std::vector<std::uint8_t> payload;
        Serializer::writeU64(payload, m_spectateMatchId);
        writePacket(NetworkMessageType::SPECTATE_ROOM_REQUEST, payload);
        std::cout << "[Client] Sent SPECTATE_ROOM_REQUEST for Match ID: " << m_spectateMatchId << std::endl;
    }

    void NetworkPlayer::beginPlay(bool isSpectator, std::uint64_t spectateMatchId, std::uint64_t onlineRoomCode)
    {
        auto self = shared_from_this();
        boost::asio::post(m_strand, [self, isSpectator, spectateMatchId, onlineRoomCode]()
                          {
            self->resetMatchState();
            self->m_isSpectator = isSpectator;
            self->m_spectateMatchId = spectateMatchId;
            self->m_onlineRoomCode = onlineRoomCode;
            self->m_deferJoin = false; // Ensure deferJoin is cleared when joining a game
            if (!self->m_connected.load()) {
                self->doConnect();
            } else {
                self->sendJoinRequest();
            } });
    }

    void NetworkPlayer::requestActiveRooms()
    {
        if (!m_connected)
            return;
        auto self = shared_from_this();
        boost::asio::post(m_strand, [self]()
                          { self->writePacket(NetworkMessageType::ROOM_LIST_REQUEST, {}); });
    }

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

    void NetworkPlayer::startControlReceive()
    {
        readControlHeader();
    }

    void NetworkPlayer::readControlHeader()
    {
        auto self = shared_from_this();
        boost::asio::async_read(m_controlSocket, boost::asio::buffer(m_controlHeaderBuffer),
                                boost::asio::bind_executor(m_strand, [self](boost::system::error_code ec, std::size_t)
                                                           {
                if (ec) {
                    self->handleDisconnect();
                    return;
                }

                std::size_t offset = 0;
                std::uint8_t rawType = 0;
                std::uint32_t payloadSize = 0;
                std::vector<std::uint8_t> header(self->m_controlHeaderBuffer.begin(), self->m_controlHeaderBuffer.end());

                bool ok = Serializer::readU8(header, offset, rawType) &&
                          Serializer::readU32(header, offset, payloadSize);

                if (!ok || payloadSize > kMaxPayloadSize) {
                    std::cerr << "[Client] Malformed or oversized control frame header." << std::endl;
                    self->handleDisconnect();
                    return;
                }

                self->readControlPayload(static_cast<NetworkMessageType>(rawType), payloadSize); }));
    }

    void NetworkPlayer::readControlPayload(NetworkMessageType type, std::uint32_t payloadSize)
    {
        m_controlPayloadBuffer.assign(payloadSize, 0);

        if (payloadSize == 0)
        {
            handleMessage(type, m_controlPayloadBuffer, TransportChannel::Control);
            readControlHeader();
            return;
        }

        auto self = shared_from_this();
        boost::asio::async_read(m_controlSocket, boost::asio::buffer(m_controlPayloadBuffer),
                                boost::asio::bind_executor(m_strand, [self, type](boost::system::error_code ec, std::size_t)
                                                           {
                if (ec) {
                    self->handleDisconnect();
                    return;
                }
                self->handleMessage(type, self->m_controlPayloadBuffer, TransportChannel::Control);
                self->readControlHeader(); }));
    }

    void NetworkPlayer::writeControlPacket(std::vector<std::uint8_t> frame)
    {
        bool writeInProgress = !m_controlWriteQueue.empty();
        m_controlWriteQueue.push_back(std::move(frame));
        if (!writeInProgress)
        {
            writeControlNext();
        }
    }

    void NetworkPlayer::writeControlNext()
    {
        auto self = shared_from_this();
        boost::asio::async_write(m_controlSocket, boost::asio::buffer(m_controlWriteQueue.front()),
                                 boost::asio::bind_executor(m_strand, [self](boost::system::error_code ec, std::size_t)
                                                            {
                if (ec) {
                    std::cerr << "[Client] TCP write error: " << ec.message() << std::endl;
                    self->handleDisconnect();
                    return;
                }
                self->m_controlWriteQueue.pop_front();
                if (!self->m_controlWriteQueue.empty()) {
                    self->writeControlNext();
                } }));
    }

    void NetworkPlayer::startRealtimeReceive()
    {
        auto self = shared_from_this();
        m_realtimeSocket.async_receive(boost::asio::buffer(m_realtimeRecvBuffer),
                                       boost::asio::bind_executor(m_strand,
                                                                  [self](boost::system::error_code ec, std::size_t bytesRecvd)
                                                                  {
                                                                      if (!ec)
                                                                      {
                                                                          if (bytesRecvd >= kHeaderSize)
                                                                          {
                                                                              std::size_t offset = 0;
                                                                              std::uint8_t rawType = 0;
                                                                              std::uint32_t payloadSize = 0;

                                                                              bool ok = Serializer::readU8(self->m_realtimeRecvBuffer, offset, rawType) &&
                                                                                        Serializer::readU32(self->m_realtimeRecvBuffer, offset, payloadSize);

                                                                              if (ok && offset + payloadSize <= bytesRecvd)
                                                                              {
                                                                                  std::vector<std::uint8_t> payload(
                                                                                      self->m_realtimeRecvBuffer.begin() + offset,
                                                                                      self->m_realtimeRecvBuffer.begin() + offset + payloadSize);
                                                                                  self->handleMessage(static_cast<NetworkMessageType>(rawType), payload, TransportChannel::Realtime);
                                                                              }
                                                                          }
                                                                          self->startRealtimeReceive();
                                                                      }
                                                                      else
                                                                      {
                                                                          self->handleDisconnect();
                                                                      }
                                                                  }));
    }

    void NetworkPlayer::writeRealtimePacket(std::vector<std::uint8_t> frame)
    {
        auto framePtr = std::make_shared<std::vector<std::uint8_t>>(std::move(frame));
        auto self = shared_from_this();
        m_realtimeSocket.async_send(boost::asio::buffer(*framePtr),
                                    boost::asio::bind_executor(m_strand,
                                                               [self, framePtr](boost::system::error_code ec, std::size_t)
                                                               {
                                                                   if (ec)
                                                                   {
                                                                       std::cerr << "[Client] UDP write error: " << ec.message() << std::endl;
                                                                   }
                                                               }));
    }

    void NetworkPlayer::writePacket(NetworkMessageType type, const std::vector<std::uint8_t> &payload)
    {
        auto frame = Serializer::buildFrame(type, payload);
        if (channelFor(type) == TransportChannel::Control)
        {
            writeControlPacket(std::move(frame));
        }
        else
        {
            writeRealtimePacket(std::move(frame));
        }
    }

    void NetworkPlayer::handleMessage(NetworkMessageType type, const std::vector<std::uint8_t> &payload, TransportChannel fromChannel)
    {
        bool isControlMove = (fromChannel == TransportChannel::Control) && (type == NetworkMessageType::GAME_MOVE);
        if (channelFor(type) != fromChannel && !isControlMove) {
            std::cerr << "[Client] Rejected message type " << static_cast<int>(type)
                      << ": arrived on the wrong transport." << std::endl;
            return;
        }

        switch (type)
        {
        case NetworkMessageType::LOGIN_RESPONSE:
        {
            bool success = false;
            int rating = 0;
            std::uint64_t token = 0;

            if (Serializer::deserializeLoginResponse(payload, success, rating, token) && success)
            {
                std::cout << "[Client] TCP authentication succeeded!" << std::endl;
                ClientAuth::rating = rating;
                m_sessionToken = token;
                connectRealtimeChannel();
            }
            else
            {
                {
                    std::lock_guard<std::mutex> lock(m_loginMutex);
                    m_loginMessage = "Login failed. Invalid password.";
                }
                m_loginStatus.store(LoginStatus::Failed);
                std::cerr << "[Client] Auth failed. Closing connection." << std::endl;
                handleDisconnect();
            }
            break;
        }
        case NetworkMessageType::SESSION_BIND_ACK:
        {
            m_realtimeBound.store(true);
            std::cout << "[Client] Realtime channel bound." << std::endl;
            startHeartbeat();
            startRetryTimer();
            if (m_deferJoin)
            {
                {
                    std::lock_guard<std::mutex> lock(m_loginMutex);
                    m_loginMessage = "Success! Access granted.";
                }
                m_loginStatus.store(LoginStatus::Success);
            }
            else
            {
                sendJoinRequest();
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
                m_matchStarted.store(true);

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
            if (Serializer::readU32(payload, readOffset, count))
            {
                std::vector<ClientMatchInfo> rooms;
                rooms.reserve(count);

                bool parseOk = true;
                for (std::uint32_t i = 0; i < count; ++i)
                {
                    ClientMatchInfo info{};
                    parseOk &= Serializer::readU64(payload, readOffset, info.matchId);
                    parseOk &= Serializer::readString(payload, readOffset, info.whitePlayer);
                    parseOk &= Serializer::readString(payload, readOffset, info.blackPlayer);

                    if (parseOk)
                    {
                        rooms.push_back(info);
                    }
                }

                if (parseOk)
                {
                    std::lock_guard<std::mutex> lock(m_roomsMutex);
                    m_activeRooms = std::move(rooms);
                    m_roomsUpdated.store(true);
                }
            }
            break;
        }
        case NetworkMessageType::DISCONNECT_COUNTDOWN:
        {
            if (!payload.empty())
            {
                int seconds = static_cast<int>(payload[0]);
                if (seconds == 0)
                {
                    // 0 indicates the opponent reconnected; hide overlay immediately
                    m_isOpponentDisconnected.store(false);
                }
                else
                {
                    m_disconnectCountdown.store(seconds);
                    m_isOpponentDisconnected.store(true);
                }
            }
            break;
        }
        case NetworkMessageType::MATCH_FOUND:
        {
            std::size_t readOffset = 0;
            std::uint64_t matchId = 0;
            std::uint8_t colorVal = 0;
            std::string oppUser;
            std::uint32_t oppElo = 1200;

            if (Serializer::readU64(payload, readOffset, matchId) &&
                Serializer::readU8(payload, readOffset, colorVal))
            {
                m_matchId.store(matchId);
                m_assignedColor.store(static_cast<PlayerColor>(colorVal));

                if (Serializer::readString(payload, readOffset, oppUser) &&
                    Serializer::readU32(payload, readOffset, oppElo))
                {
                    std::lock_guard<std::mutex> lock(m_opponentInfoMutex);
                    m_opponentUsername = oppUser;
                    m_opponentRating = oppElo;
                }

                m_matchStarted.store(true);
                std::cout << "[Client] Match started! ID: " << matchId
                          << " vs Opponent: " << oppUser << " (Elo: " << oppElo << ")" << std::endl;
            }
            break;
        }
        case NetworkMessageType::MOVE_RESULT:
        {
            auto resultOpt = Serializer::deserializeToResult(payload);
            if (resultOpt.has_value())
            {
                m_pendingMoves.erase(resultOpt->requestId); // Clear pending retry upon server response
                std::lock_guard<std::mutex> lock(m_mutex);
                m_incomingResults.push_back(*resultOpt);
            }
            break;
        }
        case NetworkMessageType::GAME_MOVE:
        {
            m_isOpponentDisconnected.store(false);
            auto packet = Serializer::deserializeMovePacket(payload);
            if (packet.has_value())
            {
                m_pendingMoves.erase(packet->requestId); // Clear pending retry upon confirmed move broadcast
                ActionRequest request = Serializer::deserializeToRequest(*packet);
                std::lock_guard<std::mutex> lock(m_mutex);
                m_incomingActions.push_back(request);
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
        case NetworkMessageType::SESSION_BIND:
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
        packet.from.x = action.from.col();
        packet.from.y = action.from.row();
        packet.to.x = action.to.col();
        packet.to.y = action.to.row();

        auto self = shared_from_this();
        boost::asio::post(m_strand, [self, packet]()
                          {
            PendingMove pm{packet, std::chrono::steady_clock::now(), 0};
            self->m_pendingMoves[packet.requestId] = pm;

            auto payload = Serializer::serializeMovePacket(packet);
            self->writePacket(NetworkMessageType::GAME_MOVE, payload); });
    }

    void NetworkPlayer::startHeartbeat()
    {
        auto self = shared_from_this();
        m_heartbeatTimer.expires_after(ClientConfig::kHeartbeatInterval);
        m_heartbeatTimer.async_wait(boost::asio::bind_executor(m_strand,
                                                               [self](const boost::system::error_code &ec)
                                                               {
                                                                   if (!ec && self->m_realtimeBound)
                                                                   {
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
                                                           [self](const boost::system::error_code &ec)
                                                           {
                                                               if (!ec && self->m_realtimeBound)
                                                               {
                                                                   self->checkAndRetryMoves();
                                                                   self->startRetryTimer();
                                                               }
                                                           }));
    }

    void NetworkPlayer::checkAndRetryMoves()
    {
        auto now = std::chrono::steady_clock::now();
        for (auto it = m_pendingMoves.begin(); it != m_pendingMoves.end(); )
        {
            auto &pm = it->second;
            if (now - pm.lastSent >= ClientConfig::kMoveRetryTimeout)
            {
                if (pm.retries >= ClientConfig::kMaxMoveRetries)
                {
                    std::cerr << "[Client] Move request " << pm.packet.requestId
                              << " max retries reached. Clearing unconfirmed pending move." << std::endl;
                    it = m_pendingMoves.erase(it);
                    continue;
                }
                pm.retries++;
                pm.lastSent = now;
                std::cout << "[Client] Re-sending lost UDP move request: " << pm.packet.requestId
                          << " (Attempt " << pm.retries << ")" << std::endl;

                auto payload = Serializer::serializeMovePacket(pm.packet);
                writePacket(NetworkMessageType::GAME_MOVE, payload);
            }
            ++it;
        }
    }

    void NetworkPlayer::handleDisconnect()
    {
        m_connected = false;
        m_controlConnected = false;
        m_realtimeBound = false;
        m_matchId = 0;
        m_matchStarted.store(false);
        m_isOpponentDisconnected.store(false);
        m_pendingMoves.clear();

        boost::system::error_code ec;
        m_controlSocket.close(ec);
        m_realtimeSocket.close(ec);
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

    void NetworkPlayer::resetMatchState()
    {
        m_matchId.store(0);
        m_matchStarted.store(false);
        m_matchEnded.store(false);
        m_opponentDisconnected.store(false);
        m_isOpponentDisconnected.store(false);

        auto self = shared_from_this();
        boost::asio::post(m_strand, [self]() {
            self->m_pendingMoves.clear();
        });

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_incomingActions.clear();
            m_incomingResults.clear();
        }
        {
            std::lock_guard<std::mutex> lock(m_opponentInfoMutex);
            m_opponentUsername = "Waiting...";
            m_opponentRating = 1200;
        }
    }

} // namespace kungfu