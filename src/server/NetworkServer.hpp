// server/NetworkServer.hpp
#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <iostream>
#include <chrono>
#include "NetworkMessages.hpp"
#include "Serializer.hpp"
#include "MatchManager.hpp"
#include "NetworkSession.hpp"

namespace kungfu {

using boost::asio::ip::udp;

class NetworkServer {
private:
    udp::socket m_socket;
    udp::endpoint m_remoteEndpoint;
    std::vector<std::uint8_t> m_recvBuffer;
    
    // מיפוי כתובות לקוח לסשן פעיל
    std::map<udp::endpoint, std::shared_ptr<NetworkSession>> m_sessions;
    std::mutex m_sessionsMutex;

    MatchManager& m_matchManager;
    boost::asio::steady_timer m_pruneTimer;

public:
    NetworkServer(boost::asio::io_context& ioContext, std::uint16_t port, MatchManager& matchManager)
        : m_socket(ioContext, udp::endpoint(udp::v4(), port)),
          m_recvBuffer(kMaxPayloadSize),
          m_matchManager(matchManager),
          m_pruneTimer(ioContext) {
        startReceive();
        startPruneTimer();
        std::cout << "[Server] UDP Server listening on port " << port << std::endl;
    }

private:
    void startReceive() {
        m_socket.async_receive_from(
            boost::asio::buffer(m_recvBuffer), m_remoteEndpoint,
            [this](boost::system::error_code ec, std::size_t bytesRecvd) {
                if (!ec && bytesRecvd >= kHeaderSize) {
                    processIncomingDatagram(bytesRecvd);
                }
                startReceive();
            });
    }

    void processIncomingDatagram(std::size_t bytesRecvd) {
        std::size_t offset = 0;
        std::uint8_t rawType = 0;
        std::uint32_t payloadSize = 0;

        bool ok = Serializer::readU8(m_recvBuffer, offset, rawType) &&
                  Serializer::readU32(m_recvBuffer, offset, payloadSize);

        if (!ok || offset + payloadSize > bytesRecvd) {
            return; // חבילת UDP לא תקינה או קצרה מדי
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
                // רק בקשות כניסה מורשות ליצור סשן חדש
                NetworkMessageType type = static_cast<NetworkMessageType>(rawType);
                if (type == NetworkMessageType::LOGIN_REQUEST || type == NetworkMessageType::REGISTER_REQUEST) {
                    session = std::make_shared<NetworkSession>(m_socket, m_remoteEndpoint, m_matchManager);
                    m_sessions[m_remoteEndpoint] = session;
                    session->start();
                }
            }
        }

        if (session) {
            session->processMessage(static_cast<NetworkMessageType>(rawType), payload);
        }
    }

    // הפעלת טיימר שמנקה סשנים מנותקים כל 5 שניות
    void startPruneTimer() {
        m_pruneTimer.expires_after(std::chrono::seconds(5));
        m_pruneTimer.async_wait([this](const boost::system::error_code& ec) {
            if (!ec) {
                pruneStaleSessions();
                startPruneTimer();
            }
        });
    }

    void pruneStaleSessions() {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        
        for (auto it = m_sessions.begin(); it != m_sessions.end(); ) {
            // אם לא התקבל סיגנל מהלקוח מעל 25 שניות - נחשיב אותו כמנותק
            if (now - it->second->lastActivity() > std::chrono::seconds(25)) {
                std::cout << "[Server] Pruning inactive UDP session: " << it->second->username() << std::endl;
                it->second->handleDisconnect();
                it = m_sessions.erase(it);
            } else {
                ++it;
            }
        }
    }
};

} // namespace kungfu