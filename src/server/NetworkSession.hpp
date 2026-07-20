// server/NetworkSession.hpp
#pragma once

#include <boost/asio.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include "NetworkMessages.hpp"
#include "engine/common/Enums.hpp"

namespace kungfu {

using boost::asio::ip::udp;

class MatchManager;

class NetworkSession : public std::enable_shared_from_this<NetworkSession> {
private:
    udp::socket& m_serverSocket; // הפניה ל-Socket המרכזי של השרת
    udp::endpoint m_remoteEndpoint; // נקודת הקצה של הלקוח
    boost::asio::strand<boost::asio::any_io_executor> m_strand;
    MatchManager& m_matchManager;
    
    std::uint64_t m_matchId = 0;
    PlayerColor m_color = PlayerColor::White;

    std::string m_username;
    int m_rating = 1200;
    bool m_isAuthenticated = false;

    // מעקב פעילות לצורך זיהוי ניתוקים
    std::chrono::steady_clock::time_point m_lastActivity;

public:
    NetworkSession(udp::socket& serverSocket, udp::endpoint remoteEndpoint, MatchManager& matchManager);

    udp::endpoint remoteEndpoint() const { return m_remoteEndpoint; }
    std::uint64_t matchId() const;
    void setMatchId(std::uint64_t id);
    void setColor(PlayerColor col);

    std::string username() const;
    int rating() const;
    void setRating(int r);

    void start();
    void sendPacket(NetworkMessageType type, const std::vector<std::uint8_t>& payload);

    // עיבוד הודעה שהתקבלה מהשרת המרכזי
    void processMessage(NetworkMessageType type, const std::vector<std::uint8_t>& payload);
    void handleDisconnect();

    std::chrono::steady_clock::time_point lastActivity() const { return m_lastActivity; }
    void updateActivity() { m_lastActivity = std::chrono::steady_clock::now(); }
};

} // namespace kungfu