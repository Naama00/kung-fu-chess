// server/network/UdpServer.cpp
#include "UdpServer.hpp"
#include "UdpConnection.hpp"
#include "PlayerSession.hpp"
#include "SessionManager.hpp"
#include "Serializer.hpp"
#include "../ServerConfig.hpp"
#include <iostream>

namespace kungfu {

UdpServer::UdpServer(boost::asio::io_context& ioContext, std::uint16_t port, SessionManager& sessionManager)
    : m_socket(ioContext, udp::endpoint(udp::v4(), port)),
      m_recvBuffer(kMaxPayloadSize),
      m_sessionManager(sessionManager),
      m_pruneTimer(ioContext) {
    startReceive();
    startPruneTimer();
    std::cout << "[Server] UDP realtime server listening on port " << port << std::endl;
}

void UdpServer::startReceive() {
    m_socket.async_receive_from(
        boost::asio::buffer(m_recvBuffer), m_remoteEndpoint,
        [this](boost::system::error_code ec, std::size_t bytesRecvd) {
            if (!ec && bytesRecvd >= kHeaderSize) {
                processIncomingDatagram(bytesRecvd);
            }
            startReceive();
        });
}

void UdpServer::processIncomingDatagram(std::size_t bytesRecvd) {
    std::size_t offset = 0;
    std::uint8_t rawType = 0;
    std::uint32_t payloadSize = 0;
    bool ok = Serializer::readU8(m_recvBuffer, offset, rawType) &&
              Serializer::readU32(m_recvBuffer, offset, payloadSize);   
    if (!ok || offset + payloadSize > bytesRecvd) {
        return;
    }
    std::vector<std::uint8_t> payload;
    if (payloadSize > 0) {
        payload.assign(m_recvBuffer.data() + offset, m_recvBuffer.data() + offset + payloadSize);
    }
    auto type = static_cast<NetworkMessageType>(rawType);
    if (type == NetworkMessageType::SESSION_BIND) {
        handleSessionBind(payload);
        return;
    }
    auto session = m_sessionManager.findByUdpEndpoint(m_remoteEndpoint);
    if (!session) {
        // Not a bound endpoint and not a bind attempt - silently drop.
        // This is the expected behavior for stray, forged, or not-yet-bound
        // datagrams; logging every occurrence would be a self-inflicted DoS
        // vector against the server's own log.
        return;
    }
    session->processMessage(type, payload, TransportChannel::Realtime);
}

void UdpServer::handleSessionBind(const std::vector<std::uint8_t>& payload) {
    std::uint64_t token = 0;
    if (!Serializer::deserializeSessionBind(payload, token)) {
        return;
    }

    auto session = m_sessionManager.findByToken(token);
    if (!session) {
        std::cerr << "[UdpServer] SESSION_BIND with unknown/expired token from " << m_remoteEndpoint << std::endl;
        return;
    }

    auto connection = std::make_shared<UdpConnection>(m_socket, m_remoteEndpoint);
    session->bindRealtimeChannel(connection);
    m_sessionManager.bindUdpEndpoint(m_remoteEndpoint, session);

    session->sendPacket(NetworkMessageType::SESSION_BIND_ACK, {});
    std::cout << "[UdpServer] Bound realtime channel for '" << session->username() << "'" << std::endl;
}

void UdpServer::startPruneTimer() {
    m_pruneTimer.expires_after(ServerConfig::kSessionPruneInterval);
    m_pruneTimer.async_wait([this](const boost::system::error_code& ec) {
        if (!ec) {
            // Only forgets stale UDP endpoint<->session bindings - see
            // SessionManager::pruneStaleUdpBindings for why this never
            // triggers a full player disconnect.
            m_sessionManager.pruneStaleUdpBindings();
            startPruneTimer();
        }
    });
}

} // namespace kungfu
