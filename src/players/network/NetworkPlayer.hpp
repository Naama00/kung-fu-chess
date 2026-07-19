// players/network/NetworkPlayer.hpp
#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <mutex>
#include <string>
#include <atomic>
#include <cstdint>
#include "players/IPlayer.hpp"
#include "../../server/NetworkMessages.hpp"
#include "../../engine/actions/ActionRequest.hpp"
#include "../../engine/actions/ActionResult.hpp"

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

        // דגלים שמעדכנים את הצד הצרכני (loop המשחק) על אירועים מהשרת
        // שאינם מהלכים/תוצאות - בלי לחייב אותו לפרסר את זרם ההודעות בעצמו.
        std::atomic<bool> m_matchEnded{false};
        std::atomic<bool> m_opponentDisconnected{false};

        // גישה לתורים האלה תמיד מוגנת ב-m_mutex, כולל לוגיקת ה-networking
        // (שרצה על m_strand) וגם הצרכן (שרץ בד"כ ב-thread של לולאת המשחק).
        std::vector<ActionRequest> m_incomingActions;
        std::vector<ActionResult> m_incomingResults;
        std::mutex m_mutex;

        std::vector<std::uint8_t> m_headerBuffer;
        std::vector<std::uint8_t> m_payloadBuffer;

        // אטומי כי sendMoveToServer עשוי להיקרא מ-thread שאינו m_strand
        // (בד"כ מ-thread לולאת המשחק).
        std::atomic<std::uint64_t> m_nextRequestId{1};

    public:
        NetworkPlayer(boost::asio::io_context &ioContext, const std::string &host, const std::string &port);
        ~NetworkPlayer() override;

        void connectAndJoin();

        std::vector<ActionRequest> decideActions(const view::GameSnapshot &snapshot) override;

        // שליפת תוצאות האימות שהצטברו וניקוי התור המקומי באותו פריים
        std::vector<ActionResult> pollResults()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto results = std::move(m_incomingResults);
            m_incomingResults.clear();
            return results;
        }

        void sendMoveToServer(const PlayerAction &action);

        bool isConnected() const { return m_connected; }
        std::uint64_t matchId() const { return m_matchId; }
        PlayerColor assignedColor() const { return m_assignedColor; }

        // true פעם אחת אחרי שהמשחק הסתיים "בטבעיות" (GAME_OVER מהשרת)
        bool matchEnded() const { return m_matchEnded; }

        // true אם היריב התנתק לפני שהמשחק הסתיים (OPPONENT_DISCONNECTED)
        bool opponentDisconnected() const { return m_opponentDisconnected; }

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