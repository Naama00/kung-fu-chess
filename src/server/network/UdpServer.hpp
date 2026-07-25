// server/network/UdpServer.hpp
#pragma once

#include <boost/asio.hpp>
#include <cstdint>
#include <vector>
#include "NetworkMessages.hpp"

namespace kungfu {

using boost::asio::ip::udp;
class SessionManager;

// UDP server: the low-latency realtime-plane entry point.
//
// Unlike TCP, a UDP endpoint carries no identity of its own, so every
// incoming datagram is either a SESSION_BIND (attaching this endpoint to an
// already-authenticated PlayerSession - see NetworkMessages.hpp for the
// handshake) or is routed to a session that was already bound. Unbound,
// non-SESSION_BIND datagrams are dropped - this closes the "anyone can
// create a session by sending a packet" gap that existed when this server
// also handled login directly.
class UdpServer {
private:
    udp::socket m_socket;
    udp::endpoint m_remoteEndpoint;
    std::vector<std::uint8_t> m_recvBuffer;

    SessionManager& m_sessionManager;
    boost::asio::steady_timer m_pruneTimer;

public:
    UdpServer(boost::asio::io_context& ioContext, std::uint16_t port, SessionManager& sessionManager);

private:
    void startReceive();
    void processIncomingDatagram(std::size_t bytesRecvd);
    void handleSessionBind(const std::vector<std::uint8_t>& payload);

    void startPruneTimer();
};

} // namespace kungfu
