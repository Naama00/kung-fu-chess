// server/network/NetworkServer.hpp
#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include "NetworkMessages.hpp"

namespace kungfu {

using boost::asio::ip::udp;
class MatchManager;
class NetworkSession;

// UDP Server responsible for receiving raw datagrams and managing sessions.
class NetworkServer {
private:
    udp::socket m_socket;
    udp::endpoint m_remoteEndpoint;
    std::vector<std::uint8_t> m_recvBuffer;
    
    std::map<udp::endpoint, std::shared_ptr<NetworkSession>> m_sessions;
    std::mutex m_sessionsMutex;

    MatchManager& m_matchManager;
    boost::asio::steady_timer m_pruneTimer;

public:
    NetworkServer(boost::asio::io_context& ioContext, std::uint16_t port, MatchManager& matchManager);
    ~NetworkServer() = default;

private:
    void startReceive();
    void processIncomingDatagram(std::size_t bytesRecvd);
    void startPruneTimer();
    void pruneStaleSessions();
};

} // namespace kungfu