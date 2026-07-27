// server/network/RedisPubSubClient.cpp
#include "server/network/RedisPubSubClient.hpp"
#include <iostream>
#include <sstream>

namespace kungfu {

RedisPubSubClient::RedisPubSubClient(boost::asio::io_context& ioContext)
    : m_ioContext(ioContext),
      m_strand(boost::asio::make_strand(ioContext.get_executor())),
      m_pubSocket(ioContext),
      m_subSocket(ioContext) {}

RedisPubSubClient::~RedisPubSubClient() {
    close();
}

void RedisPubSubClient::close() {
    boost::system::error_code ec;
    if (m_pubSocket.is_open()) m_pubSocket.close(ec);
    if (m_subSocket.is_open()) m_subSocket.close(ec);
    m_pubConnected = false;
    m_subConnected = false;
}

std::string RedisPubSubClient::formatRespArray(const std::vector<std::string>& args) {
    std::ostringstream ss;
    ss << "*" << args.size() << "\r\n";
    for (const auto& arg : args) {
        ss << "$" << arg.length() << "\r\n" << arg << "\r\n";
    }
    return ss.str();
}

bool RedisPubSubClient::connectPublisher(const std::string& host, std::uint16_t port) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_host = host;
    m_port = port;

    boost::system::error_code ec;
    tcp::resolver resolver(m_ioContext);
    auto endpoints = resolver.resolve(m_host, std::to_string(m_port), ec);

    if (ec) {
        std::cerr << "[RedisPubSub] Publisher host resolution failed: " << ec.message() << std::endl;
        return false;
    }

    boost::asio::connect(m_pubSocket, endpoints, ec);
    if (ec) {
        std::cerr << "[RedisPubSub] Publisher connection failed: " << ec.message() << std::endl;
        m_pubConnected = false;
        return false;
    }

    m_pubConnected = true;
    std::cout << "[RedisPubSub] Publisher connected to Redis at " << m_host << ":" << m_port << std::endl;
    return true;
}

void RedisPubSubClient::publish(const std::string& channel, const std::string& payload) {
    auto self = shared_from_this();
    boost::asio::post(m_strand, [this, self, channel, payload]() {
        if (!m_pubConnected || !m_pubSocket.is_open()) {
            if (!connectPublisher(m_host, m_port)) {
                std::cerr << "[RedisPubSub] Failed to auto-reconnect publisher." << std::endl;
                return;
            }
        }

        std::string command = formatRespArray({"PUBLISH", channel, payload});
        boost::system::error_code ec;
        boost::asio::write(m_pubSocket, boost::asio::buffer(command), ec);

        if (ec) {
            std::cerr << "[RedisPubSub] Publish error on channel '" << channel << "': " << ec.message() << std::endl;
            m_pubConnected = false;
            return;
        }

        // Read and discard RESP response integer reply (*1 / :clients_received)
        boost::asio::streambuf responseBuf;
        boost::asio::read_until(m_pubSocket, responseBuf, "\r\n", ec);
    });
}

bool RedisPubSubClient::subscribe(const std::string& channel, MessageCallback callback, 
                                  const std::string& host, std::uint16_t port) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_host = host;
    m_port = port;
    m_onMessageCallback = std::move(callback);

    boost::system::error_code ec;
    tcp::resolver resolver(m_ioContext);
    auto endpoints = resolver.resolve(m_host, std::to_string(m_port), ec);

    if (ec) {
        std::cerr << "[RedisPubSub] Subscriber host resolution failed: " << ec.message() << std::endl;
        return false;
    }

    boost::asio::connect(m_subSocket, endpoints, ec);
    if (ec) {
        std::cerr << "[RedisPubSub] Subscriber connection failed: " << ec.message() << std::endl;
        m_subConnected = false;
        return false;
    }

    // Issue RESP SUBSCRIBE command
    std::string command = formatRespArray({"SUBSCRIBE", channel});
    boost::asio::write(m_subSocket, boost::asio::buffer(command), ec);

    if (ec) {
        std::cerr << "[RedisPubSub] Subscribe command send failed: " << ec.message() << std::endl;
        return false;
    }

    m_subConnected = true;
    std::cout << "[RedisPubSub] Subscribed to channel '" << channel << "' on Redis " << m_host << ":" << m_port << std::endl;

    startSubscribeReadLoop();
    return true;
}

void RedisPubSubClient::startSubscribeReadLoop() {
    auto self = shared_from_this();
    boost::asio::async_read_until(m_subSocket, m_readBuffer, "\r\n",
        boost::asio::bind_executor(m_strand, [this, self](boost::system::error_code ec, std::size_t) {
            if (ec) {
                if (ec != boost::asio::error::operation_aborted) {
                    std::cerr << "[RedisPubSub] Subscription stream error: " << ec.message() << std::endl;
                }
                m_subConnected = false;
                return;
            }

            std::istream is(&m_readBuffer);
            std::string line;
            std::getline(is, line);

            // Parse Redis Multi-Bulk Array format (*3 for Pub/Sub push messages)
            if (!line.empty() && line[0] == '*') {
                int arraySize = std::stoi(line.substr(1));
                if (arraySize == 3) {
                    std::string messageType, channel, payload;
                    
                    // Read type ("message")
                    std::string lenLine;
                    std::getline(is, lenLine); // $7
                    std::getline(is, messageType);
                    if (!messageType.empty() && messageType.back() == '\r') messageType.pop_back();

                    // Read channel name
                    std::getline(is, lenLine);
                    std::getline(is, channel);
                    if (!channel.empty() && channel.back() == '\r') channel.pop_back();

                    // Read payload
                    std::getline(is, lenLine);
                    std::getline(is, payload);
                    if (!payload.empty() && payload.back() == '\r') payload.pop_back();

                    if (messageType == "message" && m_onMessageCallback) {
                        m_onMessageCallback(channel, payload);
                    }
                }
            }

            // Continue listening for the next incoming Pub/Sub message asynchronously
            startSubscribeReadLoop();
        }));
}

} // namespace kungfu