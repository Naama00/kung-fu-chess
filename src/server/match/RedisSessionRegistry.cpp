// server/match/RedisSessionRegistry.cpp
#include "server/match/RedisSessionRegistry.hpp"
#include <iostream>
#include <sstream>

namespace kungfu {

RedisSessionRegistry::RedisSessionRegistry()
    : m_socket(m_ioContext) {}

RedisSessionRegistry::~RedisSessionRegistry() {
    boost::system::error_code ec;
    m_socket.close(ec);
}

bool RedisSessionRegistry::initialize(const std::string& host, std::uint16_t port) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_host = host.empty() ? "127.0.0.1" : host;
    m_port = port;
    return ensureConnected();
}

bool RedisSessionRegistry::ensureConnected() {
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

bool RedisSessionRegistry::sendCommand(const std::vector<std::string>& args, std::string& response) {
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

bool RedisSessionRegistry::registerMatch(std::uint64_t matchId, const std::string& serverAddr,
                                         const std::string& whiteUsername, const std::string& blackUsername) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string response;

    std::string matchKey = "match:" + std::to_string(matchId) + ":server";
    sendCommand({"SET", matchKey, serverAddr}, response);

    if (!whiteUsername.empty()) {
        sendCommand({"SET", "user:" + whiteUsername + ":match", std::to_string(matchId)}, response);
    }

    if (!blackUsername.empty()) {
        sendCommand({"SET", "user:" + blackUsername + ":match", std::to_string(matchId)}, response);
    }

    return true;
}

bool RedisSessionRegistry::unregisterMatch(std::uint64_t matchId, const std::string& whiteUsername, const std::string& blackUsername) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string response;

    sendCommand({"DEL", "match:" + std::to_string(matchId) + ":server"}, response);

    if (!whiteUsername.empty()) {
        sendCommand({"DEL", "user:" + whiteUsername + ":match"}, response);
    }

    if (!blackUsername.empty()) {
        sendCommand({"DEL", "user:" + blackUsername + ":match"}, response);
    }

    return true;
}

std::optional<std::string> RedisSessionRegistry::findMatchServer(std::uint64_t matchId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string response;

    std::string key = "match:" + std::to_string(matchId) + ":server";
    if (sendCommand({"GET", key}, response)) {
        if (response.rfind("$", 0) == 0 && response != "$-1") {
            boost::asio::streambuf buf;
            boost::system::error_code ec;
            boost::asio::read_until(m_socket, buf, "\r\n", ec);
            std::istream is(&buf);
            std::string serverAddr;
            std::getline(is, serverAddr);
            if (!serverAddr.empty() && serverAddr.back() == '\r') serverAddr.pop_back();
            return serverAddr;
        }
    }
    return std::nullopt;
}

std::optional<std::uint64_t> RedisSessionRegistry::findUserMatch(const std::string& username) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string response;

    std::string key = "user:" + username + ":match";
    if (sendCommand({"GET", key}, response)) {
        if (response.rfind("$", 0) == 0 && response != "$-1") {
            boost::asio::streambuf buf;
            boost::system::error_code ec;
            boost::asio::read_until(m_socket, buf, "\r\n", ec);
            std::istream is(&buf);
            std::string matchIdStr;
            std::getline(is, matchIdStr);
            if (!matchIdStr.empty() && matchIdStr.back() == '\r') matchIdStr.pop_back();
            try { return std::stoull(matchIdStr); } catch (...) {}
        }
    }
    return std::nullopt;
}

} // namespace kungfu