// server/network/NetworkServer.cpp
#include "NetworkServer.hpp"
#include "../match/MatchManager.hpp"
#include "NetworkSession.hpp"
#include "Serializer.hpp"
#include <iostream>

namespace kungfu {

NetworkServer::NetworkServer(boost::asio::io_context& ioContext, std::uint16_t port, MatchManager& matchManager)
    : m_socket(ioContext, udp::endpoint(udp::v4(), port)),
      m_recvBuffer(kMaxPayloadSize),
      m_matchManager(matchManager),
      m_pruneTimer(ioContext) {
    startReceive();
    startPruneTimer();
    std::cout << "[Server] UDP Server listening on port " << port << std::endl;
}

void NetworkServer::startReceive() {
    m_socket.async_receive_from(
        boost::asio::buffer(m_recvBuffer), m_remoteEndpoint,
        [this](boost::system::error_code ec, std::size_t bytesRecvd) {
            if (!ec && bytesRecvd >= kHeaderSize) {
                processIncomingDatagram(bytesRecvd);
            }
            startReceive();
        });
}

void NetworkServer::processIncomingDatagram(std::size_t bytesRecvd) {
    std::size_t offset = 0;
    std::uint8_t rawType = 0;
    std::uint32_t payloadSize = 0;

    bool ok = Serializer::readU8(m_recvBuffer, offset, rawType) &&
              Serializer::readU32(m_recvBuffer, offset, payloadSize);

    if (!ok || offset + payloadSize > bytesRecvd) {
        return; 
    }

    std::vector<std::uint8_t> payload(
        m_recvBuffer.begin() + offset, 
        m_recvBuffer.begin() + offset + payloadSize
    );

    std::shared_ptr<NetworkSession> session;
    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        auto it = m_sessions.find(m_remoteEndpoint);
        if (it != m_sessions.end()) {
            session = it->second;
        } else {
            // Create a session for a new remote endpoint.
            // Note: This is a simple approach; in a real-world scenario,
            // you might want to implement additional checks (e.g., authentication) before creating a session.
            session = std::make_shared<NetworkSession>(m_socket, m_remoteEndpoint, m_matchManager);
            m_sessions[m_remoteEndpoint] = session;
            session->start();
        }
    }

    if (session) {
        session->processMessage(static_cast<NetworkMessageType>(rawType), payload);
    }
}

void NetworkServer::startPruneTimer() {
    m_pruneTimer.expires_after(kungfu::ServerConfig::kSessionPruneInterval);
    m_pruneTimer.async_wait([this](const boost::system::error_code& ec) {
        if (!ec) {
            pruneStaleSessions();
            startPruneTimer();
        }
    });
}

void NetworkServer::pruneStaleSessions() {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ) {
        if (now - it->second->lastActivity() > kungfu::ServerConfig::kSessionTimeout) {
            std::cout << "[Server] Pruning inactive UDP session: " << it->second->username() << std::endl;
            it->second->handleDisconnect();
            it = m_sessions.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace kungfu