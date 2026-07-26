// players/network/LobbyService.hpp
#pragma once

#include "players/network/NetworkPlayer.hpp"
#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace kungfu {

// Application Service encapsulating background lobby connections,
// worker thread lifecycles, and periodic room-list polling.
// Headless with zero UI framework dependencies.
class LobbyService {
private:
    boost::asio::io_context m_ioContext;
    std::shared_ptr<NetworkPlayer> m_lobbyPlayer;
    std::thread m_netThread;

    float m_queryTimer = 0.0f;
    const float m_queryInterval = 1.5f;

    std::string m_host;
    std::string m_port;

public:
    LobbyService(const std::string& host = ClientConfig::kDefaultHost, 
                 const std::string& port = ClientConfig::kDefaultPort);
    ~LobbyService();

    void start();
    void stop();

    // Advances query timers and requests room updates when active
    void update(float deltaTime);

    // Fetches the latest snapshot of active match rooms
    std::vector<NetworkPlayer::ClientMatchInfo> getActiveRooms();

    bool isConnected() const;
};

} // namespace kungfu