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

bool DistributedMatchmaker::addWaitingPlayer(const std::string& username, int rating,
                                             std::uint64_t roomCode, const std::string& serverAddr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string response;

    std::string poolKey = (roomCode != 0) ? ("mm:room:" + std::to_string(roomCode)) : "mm:public";
    std::string value = username + ":" + std::to_string(rating) + ":" + serverAddr;

    sendCommand({"SADD", poolKey, value}, response);
    return true;
}

bool DistributedMatchmaker::removeWaitingPlayer(const std::string& username) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string response;

    sendCommand({"SREM", "mm:public", username}, response);
    return true;
}

std::vector<MatchedPair> DistributedMatchmaker::pollMatchedPairs() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<MatchedPair> pairs;
    std::string response;

    // Atomically pop up to 2 waiting players from Redis pool
    if (sendCommand({"SPOP", "mm:public", "2"}, response)) {
        if (response.rfind("*", 0) == 0) {
            int count = std::stoi(response.substr(1));
            std::vector<std::string> players;

            for (int i = 0; i < count; ++i) {
                boost::asio::streambuf buf;
                boost::system::error_code ec;
                boost::asio::read_until(m_socket, buf, "\r\n", ec);
                boost::asio::read_until(m_socket, buf, "\r\n", ec);
                std::istream is(&buf);
                std::string header, val;
                std::getline(is, header);
                std::getline(is, val);
                if (!val.empty() && val.back() == '\r') val.pop_back();
                
                std::size_t colonPos = val.find(':');
                if (colonPos != std::string::npos) {
                    players.push_back(val.substr(0, colonPos));
                }
            }

            if (players.size() == 2) {
                pairs.push_back({players[0], players[1], 0});
            } else if (players.size() == 1) {
                // Re-add unmatched single player back to pool
                sendCommand({"SADD", "mm:public", players[0]}, response);
            }
        }
    }

    return pairs;
}

} // namespace kungfu