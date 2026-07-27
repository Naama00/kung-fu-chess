// server/match/RedisSessionRegistry.hpp
#pragma once

#include <boost/asio.hpp>
#include <mutex>
#include <optional>
#include <string>
#include <cstdint>
#include <vector>

namespace kungfu {

// Global session & match registry mapping matches and users across server instances in Redis
class RedisSessionRegistry {
private:
    std::string m_host;
    std::uint16_t m_port = 6379;

    boost::asio::io_context m_ioContext;
    boost::asio::ip::tcp::socket m_socket;
    mutable std::mutex m_mutex;
    bool m_connected = false;

    bool sendCommand(const std::vector<std::string>& args, std::string& response);
    bool ensureConnected();

public:
    RedisSessionRegistry();
    ~RedisSessionRegistry();

    bool initialize(const std::string& host, std::uint16_t port);

    // Registers active match hosting mapping: match_id -> server_address, and user -> match_id
    bool registerMatch(std::uint64_t matchId, const std::string& serverAddr,
                       const std::string& whiteUsername, const std::string& blackUsername);

    // Unregisters completed match from global registry
    bool unregisterMatch(std::uint64_t matchId, const std::string& whiteUsername, const std::string& blackUsername);

    // Looks up server container address hosting a specific match ID
    std::optional<std::string> findMatchServer(std::uint64_t matchId);

    // Looks up active match ID for a specific user
    std::optional<std::uint64_t> findUserMatch(const std::string& username);
};

} // namespace kungfu