// server/network/SessionManager.cpp
#include "SessionManager.hpp"
#include "PlayerSession.hpp"
#include "../ServerConfig.hpp"
#include <iostream>

namespace kungfu {

void SessionManager::registerToken(std::uint64_t token, std::shared_ptr<PlayerSession> session) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_byToken[token] = std::move(session);
}

void SessionManager::unregisterToken(std::uint64_t token) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_byToken.erase(token);
}

std::shared_ptr<PlayerSession> SessionManager::findByToken(std::uint64_t token) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_byToken.find(token);
    return it != m_byToken.end() ? it->second : nullptr;
}

void SessionManager::bindUdpEndpoint(const udp::endpoint& endpoint, std::shared_ptr<PlayerSession> session) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_byUdpEndpoint[endpoint] = UdpBinding{std::move(session), std::chrono::steady_clock::now()};
}

void SessionManager::unbindSession(std::shared_ptr<PlayerSession> session) {
    if (!session) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_byUdpEndpoint.begin(); it != m_byUdpEndpoint.end(); ) {
        if (it->second.session == session) {
            it = m_byUdpEndpoint.erase(it);
        } else {
            ++it;
        }
    }
}

std::shared_ptr<PlayerSession> SessionManager::findByUdpEndpoint(const udp::endpoint& endpoint) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_byUdpEndpoint.find(endpoint);
    if (it == m_byUdpEndpoint.end()) return nullptr;

    it->second.lastSeen = std::chrono::steady_clock::now();
    return it->second.session;
}

void SessionManager::pruneStaleUdpBindings() {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto now = std::chrono::steady_clock::now();

    for (auto it = m_byUdpEndpoint.begin(); it != m_byUdpEndpoint.end(); ) {
        if (now - it->second.lastSeen > ServerConfig::kSessionTimeout) {
            std::cout << "[SessionManager] Pruning stale UDP binding for endpoint " << it->first << std::endl;
            it = m_byUdpEndpoint.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace kungfu
