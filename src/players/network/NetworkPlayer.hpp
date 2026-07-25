// players/network/NetworkPlayer.hpp
#pragma once

#include <boost/asio.hpp>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "players/IPlayer.hpp"
#include "../../server/network/NetworkMessages.hpp"
#include "../../engine/actions/ActionRequest.hpp"
#include "../../engine/actions/ActionResult.hpp"

namespace kungfu
{
    using boost::asio::ip::tcp;
    using boost::asio::ip::udp;

    // ============================================================================
    // NetworkPlayer talks to the server over two independent channels:
    //   - m_controlSocket  (TCP): login, lobby, matchmaking, match-lifecycle.
    //   - m_realtimeSocket (UDP): moves, move results, heartbeat.
    // ============================================================================
    class NetworkPlayer : public IPlayer, public std::enable_shared_from_this<NetworkPlayer>
    {
    public:
        enum class LoginStatus { Pending, Success, Failed };

        // Struct to hold serialized details of live matches running on the server
        struct ClientMatchInfo {
            std::uint64_t matchId;
            std::string whitePlayer;
            std::string blackPlayer;
        };

    private:
        boost::asio::io_context &m_ioContext;
        boost::asio::strand<boost::asio::any_io_executor> m_strand;

        // ---- Control channel (TCP) ----
        tcp::socket m_controlSocket;
        std::array<std::uint8_t, kHeaderSize> m_controlHeaderBuffer{};
        std::vector<std::uint8_t> m_controlPayloadBuffer;
        std::deque<std::vector<std::uint8_t>> m_controlWriteQueue;
        std::atomic<bool> m_controlConnected{false};

        // ---- Realtime channel (UDP) ----
        udp::socket m_realtimeSocket;
        std::vector<std::uint8_t> m_realtimeRecvBuffer;
        std::atomic<bool> m_realtimeBound{false};

        std::uint64_t m_sessionToken = 0;

        std::string m_host;
        std::string m_port;

        std::atomic<std::uint64_t> m_matchId{0};
        std::atomic<PlayerColor> m_assignedColor{PlayerColor::White};
        std::atomic<bool> m_connected{false};
        std::atomic<bool> m_matchStarted{false};

        // Opponent info stored thread-safely
        std::string m_opponentUsername = "Waiting...";
        std::uint32_t m_opponentRating = 1200;
        std::mutex m_opponentInfoMutex;

        std::atomic<bool> m_matchEnded{false};
        std::atomic<bool> m_opponentDisconnected{false};

        // Shared action/result queues accessed by the UI thread (protected by m_mutex)
        std::vector<ActionRequest> m_incomingActions;
        std::vector<ActionResult> m_incomingResults;
        std::mutex m_mutex;

        std::atomic<std::uint64_t> m_nextRequestId{1};
        std::atomic<bool> m_isOpponentDisconnected{false};
        std::atomic<int> m_disconnectCountdown{20};

        // Network timers - both run on the realtime channel
        boost::asio::steady_timer m_heartbeatTimer;
        boost::asio::steady_timer m_retryTimer;

        // Moves awaiting server confirmation (managed strictly on the Strand thread context)
        struct PendingMove {
            NetworkMovePacket packet;
            std::chrono::steady_clock::time_point lastSent;
            int retries = 0;
        };
        std::map<std::uint64_t, PendingMove> m_pendingMoves;

        bool m_isSpectator = false;
        std::uint64_t m_spectateMatchId = 0;
        std::uint64_t m_onlineRoomCode = 0;

        bool m_deferJoin = false;
        std::atomic<LoginStatus> m_loginStatus{LoginStatus::Pending};
        std::string m_loginMessage;
        std::mutex m_loginMutex;

        std::atomic<bool> m_hasPendingSync{false};
        std::string m_pendingSyncBoard;
        std::mutex m_syncMutex;

        std::vector<ClientMatchInfo> m_activeRooms;
        std::atomic<bool> m_roomsUpdated{false};
        std::mutex m_roomsMutex;

    public:
        NetworkPlayer(boost::asio::io_context &ioContext, const std::string &host, const std::string &port,
                      bool isSpectator = false, std::uint64_t spectateMatchId = 0, std::uint64_t onlineRoomCode = 0,
                      bool deferJoin = false);
        ~NetworkPlayer() override;

        void connectAndJoin();
        std::vector<ActionRequest> decideActions(const view::GameSnapshot &snapshot) override;
        std::vector<ActionResult> pollResults();
        void sendMoveToServer(const PlayerAction &action);

        bool isConnected() const { return m_connected; }
        bool hasMatchStarted() const { return m_matchStarted.load(); }
        std::uint64_t matchId() const { return m_matchId; }
        PlayerColor assignedColor() const { return m_assignedColor; }
        
        std::string opponentUsername() {
            std::lock_guard<std::mutex> lock(m_opponentInfoMutex);
            return m_opponentUsername;
        }
        
        std::uint32_t opponentRating() {
            std::lock_guard<std::mutex> lock(m_opponentInfoMutex);
            return m_opponentRating;
        }

        std::uint64_t onlineRoomCode() const { return m_onlineRoomCode; }

        bool matchEnded() const { return m_matchEnded; }
        bool opponentDisconnected() const { return m_opponentDisconnected; }
        bool isOpponentDisconnectedWithCountdown() const { return m_isOpponentDisconnected.load(); }
        int opponentDisconnectCountdown() const { return m_disconnectCountdown.load(); }

        bool isSpectator() const { return m_isSpectator; }
        bool hasPendingSync() const { return m_hasPendingSync.load(); }
        std::string consumePendingSync();

        std::vector<ClientMatchInfo> getActiveRooms();
        void requestActiveRooms();

        LoginStatus loginStatus() const { return m_loginStatus.load(); }
        std::string loginMessage() {
            std::lock_guard<std::mutex> lock(m_loginMutex);
            return m_loginMessage;
        }

        // Call this once the caller knows what mode to join in (after LoginScreen hands connection to StartScreen)
        void beginPlay(bool isSpectator, std::uint64_t spectateMatchId, std::uint64_t onlineRoomCode);

        // Publicly accessible for clean connection termination
        void handleDisconnect();

        // Resets in-match state without tearing down active socket connections
        void resetMatchState();
        
    private:
        // ---- Connection handshake ----
        void doConnect();
        void onControlConnected();
        void connectRealtimeChannel();
        void sendSessionBind();
        void sendJoinRequest();
        void sendSpectateRequest();

        // ---- Control channel (TCP): stream framing ----
        void startControlReceive();
        void readControlHeader();
        void readControlPayload(NetworkMessageType type, std::uint32_t payloadSize);
        void writeControlPacket(std::vector<std::uint8_t> frame);
        void writeControlNext();

        // ---- Realtime channel (UDP): datagram framing ----
        void startRealtimeReceive();
        void writeRealtimePacket(std::vector<std::uint8_t> frame);

        void writePacket(NetworkMessageType type, const std::vector<std::uint8_t> &payload);
        void handleMessage(NetworkMessageType type, const std::vector<std::uint8_t> &payload, TransportChannel fromChannel);

        void startHeartbeat();
        void startRetryTimer();
        void checkAndRetryMoves();
    };
} // namespace kungfu