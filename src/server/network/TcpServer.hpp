// server/network/TcpServer.hpp
#pragma once

#include <boost/asio.hpp>
#include <cstdint>

namespace kungfu {

using boost::asio::ip::tcp;
class MatchManager;
class SessionManager;

// TCP server: the reliable control-plane entry point.
//
// Handles the accept loop and wires every new connection to a fresh
// PlayerSession. Authentication, lobby/matchmaking, room listing,
// spectating, and match-lifecycle notifications all flow through here -
// see NetworkMessages.hpp for the full TCP/UDP split and the SESSION_BIND
// handshake used to hand off to UdpServer for gameplay.
class TcpServer {
private:
    tcp::acceptor m_acceptor;
    MatchManager& m_matchManager;
    SessionManager& m_sessionManager;

public:
    TcpServer(boost::asio::io_context& ioContext, std::uint16_t port, MatchManager& matchManager, SessionManager& sessionManager);

private:
    void startAccept();
};

} // namespace kungfu
