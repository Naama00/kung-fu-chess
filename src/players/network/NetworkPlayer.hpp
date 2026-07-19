#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <mutex>
#include <string>
#include <atomic>
#include "players/IPlayer.hpp"
#include "../../server/NetworkMessages.hpp"

namespace kungfu
{

    using boost::asio::ip::tcp;

    class NetworkPlayer : public IPlayer, public std::enable_shared_from_this<NetworkPlayer>
    {
    private:
        boost::asio::io_context &m_ioContext;
        tcp::socket m_socket;
        boost::asio::strand<boost::asio::any_io_executor> m_strand;

        std::string m_host;
        std::string m_port;

        std::atomic<std::uint64_t> m_matchId{0};
        std::atomic<PlayerColor> m_assignedColor{PlayerColor::White};
        std::atomic<bool> m_connected{false};

        // תור מוגן חוטים לאחסון מהלכים שהגיעו מהשרת
        std::vector<ActionRequest> m_incomingActions;
        std::mutex m_mutex;

        // מאגרי זיכרון לקריאת המידע מהרשת
        std::vector<std::uint8_t> m_headerBuffer;
        std::vector<std::uint8_t> m_payloadBuffer;

        std::uint64_t m_nextRequestId = 1;

    public:
        NetworkPlayer(boost::asio::io_context &ioContext, const std::string &host, const std::string &port);
        ~NetworkPlayer() override;

        // התחברות לשרת ושליחת בקשת הצטרפות למשחק (Matchmaking)
        void connectAndJoin();

        // מימוש מתודת הממשק התקני של IPlayer
        std::vector<ActionRequest> decideActions(const view::GameSnapshot &snapshot) override;

        // שליחת מהלך שבוצע מקומית אל השרת
        void sendMoveToServer(const PlayerAction &action);

        bool isConnected() const { return m_connected; }
        std::uint64_t matchId() const { return m_matchId; }
        PlayerColor assignedColor() const { return m_assignedColor; }

    private:
        void doConnect();
        void sendJoinRequest();
        void readHeader();
        void readPayload(NetworkMessageType type, std::uint32_t payloadSize);
        void processMessage(NetworkMessageType type, const std::vector<std::uint8_t> &payload);
        void writePacket(NetworkMessageType type, const std::vector<std::uint8_t> &payload);
        void handleDisconnect();
    };

} // namespace kungfu