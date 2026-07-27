// server/persistence/RedisUserRepository.hpp
#pragma once

#include "server/persistence/IUserRepository.hpp"
#include <boost/asio.hpp>
#include <mutex>
#include <string>

namespace kungfu {

// Redis-backed NoSQL user repository implementing IUserRepository via native RESP protocol
class RedisUserRepository : public IUserRepository {
private:
    std::string m_host;
    std::uint16_t m_port = 6379;
    
    boost::asio::io_context m_ioContext;
    boost::asio::ip::tcp::socket m_socket;
    mutable std::mutex m_mutex;
    bool m_connected = false;

    // Internal helper methods for RESP protocol communication
    bool sendCommand(const std::vector<std::string>& args, std::string& response);
    bool ensureConnected();

public:
    RedisUserRepository();
    ~RedisUserRepository() override;

    RedisUserRepository(const RedisUserRepository&) = delete;
    RedisUserRepository& operator=(const RedisUserRepository&) = delete;

    // Connection initialization ("host:port" format)
    bool initialize(const std::string& connectionString) override;

    // User persistence operations
    bool createUser(const UserRecord& user) override;
    std::optional<UserRecord> findByUsername(const std::string& username) override;
    bool updateRating(const std::string& username, int newRating) override;
};

} // namespace kungfu