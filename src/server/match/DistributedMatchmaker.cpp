// server/match/DistributedMatchmaker.cpp
#include "server/match/DistributedMatchmaker.hpp"
#include <iostream>
#include <sstream>

namespace kungfu {

DistributedMatchmaker::DistributedMatchmaker()
    : m_socket(m_ioContext) {}

DistributedMatchmaker::~DistributedMatchmaker() {
    boost::system::error_code ec;
    m_socket.close(ec);
}

bool DistributedMatchmaker::initialize(const std::string& host, std::uint16_t port) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_host = host.empty() ? "127.0.0.1" : host;
    m_port = port;
    return ensureConnected();
}

bool DistributedMatchmaker::ensureConnected() {
    if (m_connected && m_socket.is_open()) {
        return true;
    }

    boost::system::error_code ec;
    boost::asio::ip::tcp::resolver resolver(m_ioContext);
    auto endpoints = resolver.resolve(m_host, std::to_string(m_port), ec);

    if (ec) {
        return false;
    }

    boost::asio::connect(m_socket, endpoints, ec);
    if (ec) {
        m_connected = false;
        return false;
    }

    m_connected = true;
    return true;
}

bool DistributedMatchmaker::sendCommand(const std::vector<std::string>& args, std::string& response) {
    if (!ensureConnected()) {
        return false;
    }

    std::ostringstream resp;
    resp << "*" << args.size() << "\r\n";
    for (const auto& arg : args) {
        resp << "$" << arg.length() << "\r\n" << arg << "\r\n";
    }

    std::string payload = resp.str();
    boost::system::error_code ec;
    boost::asio::write(m_socket, boost::asio::buffer(payload), ec);

    if (ec) {
        m_connected = false;
        return false;
    }

    boost::asio::streambuf responseBuf;
    boost::asio::read_until(m_socket, responseBuf, "\r\n", ec);
    if (ec) {
        m_connected = false;
        return false;
    }

    std::istream stream(&responseBuf);
    std::getline(stream, response);
    if (!response.empty() && response.back() == '\r') {
        response.pop_back();
    }

    return true;
}

std::vector<MatchedPair> DistributedMatchmaker::pollMatchedPairs() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<MatchedPair> pairs;
    std::string response;

    // 1. Atomically pop up to 2 waiting player raw elements from Redis set
    if (!sendCommand({"SPOP", "mm:public", "2"}, response)) {
        return pairs; // Network command failed, m_connected was handled in sendCommand
    }

    // RESP Array response starts with '*' (e.g., "*2\r\n" or "*0\r\n")
    if (response.rfind("*", 0) != 0) {
        return pairs;
    }

    int count = 0;
    try {
        count = std::stoi(response.substr(1));
    } catch (...) {
        return pairs;
    }

    // Documented edge case: Queue is empty, return promptly
    if (count <= 0) {
        return pairs;
    }

    std::vector<std::string> rawValues;
    std::vector<std::string> parsedUsernames;
    rawValues.reserve(count);
    parsedUsernames.reserve(count);

    for (int i = 0; i < count; ++i) {
        boost::asio::streambuf buf;
        boost::system::error_code ec;

        // Read Bulk String Header ($<length>\r\n)
        boost::asio::read_until(m_socket, buf, "\r\n", ec);
        if (ec) {
            m_connected = false;
            break;
        }

        // Read Actual String Value (<payload>\r\n)
        boost::asio::read_until(m_socket, buf, "\r\n", ec);
        if (ec) {
            m_connected = false;
            break;
        }

        std::istream is(&buf);
        std::string header, val;
        std::getline(is, header);
        std::getline(is, val);
        if (!val.empty() && val.back() == '\r') {
            val.pop_back();
        }

        // Parse format "username:rating:serverAddr"
        std::size_t colonPos = val.find(':');
        if (colonPos != std::string::npos) {
            std::string username = val.substr(0, colonPos);
            if (!username.empty()) {
                rawValues.push_back(val);              // Keep full original string for rollback
                parsedUsernames.push_back(username);   // Extract clean username
            }
        } else {
            std::cerr << "[Matchmaker] Warning: Malformed player value in Redis queue: " << val << std::endl;
        }
    }

    // Evaluate results and handle match pairing or rollback
    if (parsedUsernames.size() == 2) {
        // Matched pair created! roomCode = 0 denotes a public matchmaking pair.
        pairs.push_back({parsedUsernames[0], parsedUsernames[1], 0});
    } 
    else if (parsedUsernames.size() == 1) {
        // Only 1 player popped (or 2nd failed parsing/network error). 
        // Rollback: Re-add original full raw value ("username:rating:serverAddr") back to pool
        sendCommand({"SADD", "mm:public", rawValues[0]}, response);
    }

    return pairs;
}

} // namespace kungfu