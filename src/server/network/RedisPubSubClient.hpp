// server/network/RedisPubSubClient.hpp
#pragma once

#include <boost/asio.hpp>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <cstdint>

namespace kungfu {

using boost::asio::ip::tcp;

// Callback signature triggered when a published message is received on a subscribed channel
using MessageCallback = std::function<void(const std::string& channel, const std::string& payload)>;

/**
 * @class RedisPubSubClient
 * @brief High-performance asynchronous Redis Pub/Sub client built on Boost.Asio.
 * 
 * Provides inter-service messaging capabilities allowing independent microservices
 * (API/Realtime Gateways, Matchmaker, Game Allocator, Game Server Shards) to seamlessly
 * communicate using standard Redis RESP protocol formatting.
 */
class RedisPubSubClient : public std::enable_shared_from_this<RedisPubSubClient> {
private:
    boost::asio::io_context& m_ioContext;
    boost::asio::strand<boost::asio::any_io_executor> m_strand;

    // Separate sockets for publish and subscribe operations as required by Redis specification
    tcp::socket m_pubSocket;
    tcp::socket m_subSocket;

    std::string m_host{"127.0.0.1"};
    std::uint16_t m_port{6379};

    boost::asio::streambuf m_readBuffer;
    MessageCallback m_onMessageCallback;
    std::mutex m_mutex;

    bool m_pubConnected{false};
    bool m_subConnected{false};

    // Formats a command array into raw Redis RESP binary protocol format
    static std::string formatRespArray(const std::vector<std::string>& args);

    // Initiates the asynchronous stream loop for incoming Pub/Sub push notifications
    void startSubscribeReadLoop();

    // Helper parser for RESP bulk string protocol messages
    bool parseRespMessage(std::istream& stream, std::string& channel, std::string& payload);

public:
    explicit RedisPubSubClient(boost::asio::io_context& ioContext);
    ~RedisPubSubClient();

    RedisPubSubClient(const RedisPubSubClient&) = delete;
    RedisPubSubClient& operator=(const RedisPubSubClient&) = delete;

    /**
     * @brief Connects publisher socket to the target Redis instance.
     * @param host Redis server hostname or IP address.
     * @param port Redis TCP port (default 6379).
     * @return True if socket connection succeeds.
     */
    bool connectPublisher(const std::string& host = "127.0.0.1", std::uint16_t port = 6379);

    /**
     * @brief Connects subscriber socket and binds an asynchronous message handler.
     * @param channel The Redis channel to subscribe to.
     * @param callback Callback executed on the io_context thread when a message arrives.
     * @param host Redis server hostname.
     * @param port Redis TCP port.
     * @return True if connection and subscription succeed.
     */
    bool subscribe(const std::string& channel, MessageCallback callback, 
                   const std::string& host = "127.0.0.1", std::uint16_t port = 6379);

    /**
     * @brief Asynchronously publishes a message payload to a target Redis channel.
     * @param channel Target channel name.
     * @param payload Serialized string payload (e.g., JSON or binary wire packet).
     */
    void publish(const std::string& channel, const std::string& payload);

    /**
     * @brief Gracefully closes underlying sockets and timer resources.
     */
    void close();
};

} // namespace kungfu