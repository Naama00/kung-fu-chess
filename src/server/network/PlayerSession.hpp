// server/network/PlayerSession.hpp
#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "server/network/IConnection.hpp"
#include "server/network/NetworkMessages.hpp"
#include "server/ServerConfig.hpp"
#include "engine/common/Enums.hpp"

namespace kungfu
{
    class MatchManager;

    // PlayerSession is the single logical identity of a connected player or
    // spectator. It is transport-agnostic: it never touches a boost::asio
    // socket directly, only the IConnection interface. It holds up to two
    // channels:
    //   - m_controlChannel  (TCP) - set for the entire lifetime of the session,
    //     from the moment the TCP connection is accepted.
    //   - m_realtimeChannel (UDP) - set once the client completes the
    //     SESSION_BIND handshake described in NetworkMessages.hpp.
    //
    // A session is created when a client's TCP connection is accepted and lives
    // for as long as that connection stays open; see handleDisconnect().
    class PlayerSession : public std::enable_shared_from_this<PlayerSession>
    {
    private:
        MatchManager &m_matchManager;

        std::shared_ptr<IConnection> m_controlChannel;
        std::shared_ptr<IConnection> m_realtimeChannel;

        std::uint64_t m_sessionToken = 0;

        std::uint64_t m_matchId = 0;
        PlayerColor m_color = PlayerColor::White;

        std::string m_username;
        int m_rating = ServerConfig::kDefaultRating;
        bool m_isAuthenticated = false;

        std::chrono::steady_clock::time_point m_lastActivity;

    public:
        PlayerSession(std::shared_ptr<IConnection> controlChannel, MatchManager &matchManager);

        // ---- Identity & match state ----
        std::uint64_t sessionToken() const;

        std::uint64_t matchId() const;
        void setMatchId(std::uint64_t id);
        void setColor(PlayerColor col);

        std::string username() const;
        int rating() const;
        void setRating(int r);
        bool isAuthenticated() const;

        // ---- Channel management ----
        void bindRealtimeChannel(std::shared_ptr<IConnection> realtimeChannel);
        bool hasRealtimeChannel() const;

        // ---- Sending ----
        void sendPacket(NetworkMessageType type, const std::vector<std::uint8_t> &payload);
        void sendControlPacket(NetworkMessageType type, const std::vector<std::uint8_t> &payload);

        // ---- Incoming message handling ----
        void processMessage(NetworkMessageType type, const std::vector<std::uint8_t> &payload, TransportChannel fromChannel);
        void handleDisconnect();

        std::chrono::steady_clock::time_point lastActivity() const;
        void updateActivity();

    private:
        // ---- Dedicated Message Handlers ----
        void handleLoginRequest(const std::vector<std::uint8_t>& payload);
        void handleRegisterRequest(const std::vector<std::uint8_t>& payload);
        void handleJoinMatchRequest(const std::vector<std::uint8_t>& payload);
        void handleGameMoveMsg(const std::vector<std::uint8_t>& payload);
        void handleSpectateRoomRequest(const std::vector<std::uint8_t>& payload);
        void handleRoomListRequest();
    };

} // namespace kungfu