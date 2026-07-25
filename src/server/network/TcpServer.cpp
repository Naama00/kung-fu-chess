// server/network/TcpServer.cpp
#include "TcpServer.hpp"
#include "TcpConnection.hpp"
#include "PlayerSession.hpp"
#include "SessionManager.hpp"
#include "../match/MatchManager.hpp"
#include <iostream>

namespace kungfu {

TcpServer::TcpServer(boost::asio::io_context& ioContext, std::uint16_t port, MatchManager& matchManager, SessionManager& sessionManager)
    : m_acceptor(ioContext, tcp::endpoint(tcp::v4(), port)),
      m_matchManager(matchManager),
      m_sessionManager(sessionManager) {
    startAccept();
    std::cout << "[Server] TCP control server listening on port " << port << std::endl;
}

void TcpServer::startAccept() {
    m_acceptor.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                auto connection = std::make_shared<TcpConnection>(std::move(socket));
                auto session = std::make_shared<PlayerSession>(connection, m_matchManager);

                // Tracks whether this session's token has already been
                // registered with SessionManager, so registration happens
                // exactly once (right after a successful LOGIN_REQUEST) and
                // can be cleanly reversed on disconnect.
                auto tokenRegistered = std::make_shared<bool>(false);

                connection->start(
                    // onMessage
                    [this, session, tokenRegistered](NetworkMessageType type, const std::vector<std::uint8_t>& payload) {
                        session->processMessage(type, payload, TransportChannel::Control);

                        if (!*tokenRegistered && session->sessionToken() != 0) {
                            m_sessionManager.registerToken(session->sessionToken(), session);
                            *tokenRegistered = true;
                        }
                    },
                    // onDisconnect
                    [this, session, tokenRegistered]() {
                        if (*tokenRegistered) {
                            m_sessionManager.unregisterToken(session->sessionToken());
                        }
                        m_sessionManager.unbindSession(session);
                        session->handleDisconnect();
                    }
                );
            }
            startAccept();
        });
}

} // namespace kungfu
