// server/network/SessionManager.hpp
#pragma once

#include <boost/asio.hpp>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace kungfu {

using boost::asio::ip::udp;
class PlayerSession;

// Central registry correlating the two identities a connected client can
// have: a TCP session token (issued at login) and, once bound, a UDP
// endpoint used for realtime gameplay traffic.
//
// This is the only place that knows how to go from a raw transport-level
// identity to a PlayerSession. TcpServer and UdpServer both depend on it,
// but never on each other - which is what lets the two transports evolve
// independently.
class SessionManager {
private:
    struct UdpBinding {
        std::shared_ptr<PlayerSession> session;
        std::chrono::steady_clock::time_point lastSeen;
    };

    mutable std::mutex m_mutex;
    std::unordered_map<std::uint64_t, std::shared_ptr<PlayerSession>> m_byToken;
    std::map<udp::endpoint, UdpBinding> m_byUdpEndpoint;

public:
    // ---- Token registry, populated after a successful TCP login ----
    void registerToken(std::uint64_t token, std::shared_ptr<PlayerSession> session);
    void unregisterToken(std::uint64_t token);
    std::shared_ptr<PlayerSession> findByToken(std::uint64_t token) const;

    // ---- UDP endpoint registry, populated after SESSION_BIND ----
    void bindUdpEndpoint(const udp::endpoint& endpoint, std::shared_ptr<PlayerSession> session);
    void unbindSession(std::shared_ptr<PlayerSession> session);

    // Looks up the session bound to 'endpoint' and, if found, refreshes its
    // liveness timestamp (this is called once per incoming datagram, which
    // is exactly the signal that binding is still alive).
    std::shared_ptr<PlayerSession> findByUdpEndpoint(const udp::endpoint& endpoint);

    // Removes UDP bindings that have been silent for longer than
    // ServerConfig::kSessionTimeout. This only forgets the UDP endpoint -
    // it never touches the underlying PlayerSession or its TCP channel; see
    // PlayerSession::handleDisconnect for the actual disconnect trigger.
    void pruneStaleUdpBindings();
};

} // namespace kungfu
