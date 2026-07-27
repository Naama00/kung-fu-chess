// server/persistence/RedisUserRepository.cpp
#include "server/persistence/RedisUserRepository.hpp"
#include "server/ServerConfig.hpp"
#include <iostream>
#include <sstream>
#include <vector>

namespace kungfu {

RedisUserRepository::RedisUserRepository()
    : m_socket(m_ioContext) {}

RedisUserRepository::~RedisUserRepository() {
    boost::system::error_code ec;
    m_socket.close(ec);
}

bool RedisUserRepository::initialize(const std::string& connectionString) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::size_t colonPos = connectionString.find(':');
    if (colonPos != std::string::npos) {
        m_host = connectionString.substr(0, colonPos);
        m_port = static_cast<std::uint16_t>(std::atoi(connectionString.substr(colonPos + 1).c_str()));
    } else {
        m_host = connectionString.empty() ? "127.0.0.1" : connectionString;
        m_port = 6379;
    }

    return ensureConnected();
}

bool RedisUserRepository::ensureConnected() {
    if (m_connected && m_socket.is_open()) {
        return true;
    }

    boost::system::error_code ec;
    boost::asio::ip::tcp::resolver resolver(m_ioContext);
    auto endpoints = resolver.resolve(m_host, std::to_string(m_port), ec);

    if (ec) {
        std::cerr << "[Redis] Host resolution failed: " << ec.message() << std::endl;
        return false;
    }

    boost::asio::connect(m_socket, endpoints, ec);
    if (ec) {
        std::cerr << "[Redis] Connection failed to " << m_host << ":" << m_port << " - " << ec.message() << std::endl;
        m_connected = false;
        return false;
    }

    m_connected = true;
    std::cout << "[Redis] Connected successfully to " << m_host << ":" << m_port << std::endl;
    return true;
}

bool RedisUserRepository::sendCommand(const std::vector<std::string>& args, std::string& response) {
    if (!ensureConnected()) {
        return false;
    }

    // Format Redis command into RESP array protocol (*N\r\n$L\r\nArg\r\n...)
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

    // Read single response line or bulk string
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

bool RedisUserRepository::createUser(const UserRecord& user) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string key = "user:" + user.username;
    std::string response;

    // Use HSETNX to set password_hash only if the user does not exist
    if (!sendCommand({"HSETNX", key, "password_hash", user.passwordHash}, response)) {
        return false;
    }

    // RESP integer reply: :1 if key was created, :0 if user already exists
    if (response != ":1") {
        return false; // Username taken
    }

    // Set user rating
    sendCommand({"HSET", key, "rating", std::to_string(user.rating)}, response);
    return true;
}

std::optional<UserRecord> RedisUserRepository::findByUsername(const std::string& username) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string key = "user:" + username;
    std::string response;

    // Check if user key exists
    if (!sendCommand({"EXISTS", key}, response) || response != ":1") {
        return std::nullopt;
    }

    // Fetch password_hash
    std::string passHash;
    if (sendCommand({"HGET", key, "password_hash"}, response)) {
        if (response.rfind("$", 0) == 0 && response != "$-1") {
            // Read bulk string content
            boost::asio::streambuf buf;
            boost::system::error_code ec;
            boost::asio::read_until(m_socket, buf, "\r\n", ec);
            std::istream is(&buf);
            std::getline(is, passHash);
            if (!passHash.empty() && passHash.back() == '\r') passHash.pop_back();
        }
    }

    // Fetch rating
    int rating = ServerConfig::kDefaultRating;
    if (sendCommand({"HGET", key, "rating"}, response)) {
        if (response.rfind("$", 0) == 0 && response != "$-1") {
            boost::asio::streambuf buf;
            boost::system::error_code ec;
            boost::asio::read_until(m_socket, buf, "\r\n", ec);
            std::string ratingStr;
            std::istream is(&buf);
            std::getline(is, ratingStr);
            if (!ratingStr.empty() && ratingStr.back() == '\r') ratingStr.pop_back();
            try { rating = std::stoi(ratingStr); } catch (...) {}
        }
    }

    UserRecord record;
    record.username = username;
    record.passwordHash = passHash;
    record.rating = rating;

    return record;
}

bool RedisUserRepository::updateRating(const std::string& username, int newRating) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string key = "user:" + username;
    std::string response;

    if (!sendCommand({"EXISTS", key}, response) || response != ":1") {
        return false;
    }

    return sendCommand({"HSET", key, "rating", std::to_string(newRating)}, response);
}

} // namespace kungfu