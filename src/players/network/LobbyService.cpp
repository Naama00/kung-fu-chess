// players/network/LobbyService.cpp
#include "players/network/LobbyService.hpp"
#include <iostream>

namespace kungfu {

LobbyService::LobbyService(const std::string& host, const std::string& port)
    : m_host(host), m_port(port) {}

LobbyService::~LobbyService() {
    stop();
}

void LobbyService::start() {
    if (m_lobbyPlayer) return;

    m_lobbyPlayer = std::make_shared<NetworkPlayer>(m_ioContext, m_host, m_port, true, 0);
    m_lobbyPlayer->connectAndJoin();

    m_netThread = std::thread([this]() {
        boost::asio::io_context::work work(m_ioContext);
        m_ioContext.run();
    });
}

void LobbyService::stop() {
    if (m_lobbyPlayer) {
        m_ioContext.stop();
        if (m_netThread.joinable()) {
            m_netThread.join();
        }
        m_lobbyPlayer.reset();
    }
}

void LobbyService::update(float deltaTime) {
    if (!m_lobbyPlayer) {
        start();
    }

    m_queryTimer -= deltaTime;
    if (m_queryTimer <= 0.0f) {
        m_queryTimer = m_queryInterval;
        if (m_lobbyPlayer && m_lobbyPlayer->isConnected()) {
            m_lobbyPlayer->requestActiveRooms();
        }
    }
}

std::vector<NetworkPlayer::ClientMatchInfo> LobbyService::getActiveRooms() {
    if (m_lobbyPlayer && m_lobbyPlayer->isConnected()) {
        return m_lobbyPlayer->getActiveRooms();
    }
    return {};
}

bool LobbyService::isConnected() const {
    return m_lobbyPlayer && m_lobbyPlayer->isConnected();
}

} // namespace kungfu