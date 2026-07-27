// server/match/DistributedMatchmaker.hpp
#pragma once

#include <boost/asio.hpp>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>

namespace kungfu {

struct MatchedPair {
    std::string player1;
    std::string player2;
    std::uint64_t roomCode = 0;
};

// Distributed matchmaking engine utilizing central Redis pool across all server nodes
class DistributedMatchmaker {
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
    DistributedMatchmaker();
    ~DistributedMatchmaker();

    bool initialize(const std::string& host, std::uint16_t port);

    // Adds a waiting player request into central Redis pool
    bool addWaitingPlayer(const std::string& username, int rating, std::uint64_t roomCode, const std::string& serverAddr);

    // Removes a waiting player request from central Redis pool
    bool removeWaitingPlayer(const std::string& username);

    // Scans central Redis pool and claims compatible matched player pairs atomically
    std::vector<MatchedPair> pollMatchedPairs();
};

} // namespace kungfu