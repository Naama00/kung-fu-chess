// players/network/NetworkPlayer.hpp
#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <mutex>
#include <map>
#include <string>
#include <atomic>
#include <cstdint>
#include <chrono>
#include "players/IPlayer.hpp"
#include "../../server/NetworkMessages.hpp"
#include "../../engine/actions/ActionRequest.hpp"
#include "../../engine/actions/ActionResult.hpp"

namespace kungfu
{
    using boost::asio::ip::udp;

    class NetworkPlayer : public IPlayer, public std::enable_shared_from_this<NetworkPlayer>
    {
    private:
        boost::asio::io_context &m_ioContext;
        udp::socket m_socket;
        boost::asio::strand<boost::asio::any_io_executor> m_strand;

        std::string m_host;
        std::string m_port;

        std::atomic<std::uint64_t> m_matchId{0};
        std::atomic<PlayerColor> m_assignedColor{PlayerColor::White};
        std::atomic<bool> m_connected{false};

        std::atomic<bool> m_matchEnded{false};
        std::atomic<bool> m_opponentDisconnected{false};

        // תורים המשותפים עם ה-UI thread (מוגנים ב-Mutex)
        std::vector<ActionRequest> m_incomingActions;
        std::vector<ActionResult> m_incomingResults;
        std::mutex m_mutex;

        std::vector<std::uint8_t> m_recvBuffer;

        std::atomic<std::uint64_t> m_nextRequestId{1};
        std::atomic<bool> m_isOpponentDisconnected{false};
        std::atomic<int> m_disconnectCountdown{20};

        // טיימרים
        boost::asio::steady_timer m_heartbeatTimer;
        boost::asio::steady_timer m_retryTimer; // טיימר לבדיקת איבודי חבילות

        // מבנה נתונים למהלכים הממתינים לאישור (מנוהל רק על ה-Strand - אין צורך בנעילה)
        struct PendingMove {
            NetworkMovePacket packet;
            std::chrono::steady_clock::time_point lastSent;
            int retries = 0;
        };
        std::map<std::uint64_t, PendingMove> m_pendingMoves;

    public:
        NetworkPlayer(boost::asio::io_context &ioContext, const std::string &host, const std::string &port);
        ~NetworkPlayer() override;

        void connectAndJoin();
        std::vector<ActionRequest> decideActions(const view::GameSnapshot &snapshot) override;
        std::vector<ActionResult> pollResults();
        void sendMoveToServer(const PlayerAction &action);

        bool isConnected() const { return m_connected; }
        std::uint64_t matchId() const { return m_matchId; }
        PlayerColor assignedColor() const { return m_assignedColor; }
        bool matchEnded() const { return m_matchEnded; }
        bool opponentDisconnected() const { return m_opponentDisconnected; }
        bool isOpponentDisconnectedWithCountdown() const { return m_isOpponentDisconnected.load(); }
        int opponentDisconnectCountdown() const { return m_disconnectCountdown.load(); }

    private:
        void doConnect();
        void sendJoinRequest();
        void startReceive();
        void processDatagram(std::size_t bytesRecvd);
        void writePacket(NetworkMessageType type, const std::vector<std::uint8_t> &payload);
        void handleDisconnect();
        
        void startHeartbeat();
        
        // מנגנון אבטחת הגעה (Application-level ACKs)
        void startRetryTimer();
        void checkAndRetryMoves();
    };
} // namespace kungfu