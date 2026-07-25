// server/network/TcpConnection.hpp
#pragma once

#include <boost/asio.hpp>
#include <array>
#include <deque>
#include <functional>
#include <memory>
#include <vector>
#include "IConnection.hpp"

namespace kungfu {

using boost::asio::ip::tcp;

// Reliable (TCP) send/receive endpoint for a single client connection.
//
// Unlike UDP, TCP has no built-in message boundaries - it is a raw byte
// stream. This class is responsible for re-assembling the [1-byte type]
// [4-byte length][payload] frames defined in NetworkMessages.hpp/
// Serializer.hpp out of that stream (header first, then exactly
// 'payloadSize' bytes) before handing a complete, decoded message up to its
// owner via the message callback given to start().
class TcpConnection : public IConnection, public std::enable_shared_from_this<TcpConnection> {
public:
    using MessageHandler = std::function<void(NetworkMessageType, const std::vector<std::uint8_t>&)>;
    using DisconnectHandler = std::function<void()>;

private:
    tcp::socket m_socket;
    boost::asio::strand<boost::asio::any_io_executor> m_strand;

    std::array<std::uint8_t, kHeaderSize> m_headerBuffer{};
    std::vector<std::uint8_t> m_payloadBuffer;

    // Outgoing write queue: async_write calls on the same socket must never
    // overlap, so writes are serialized through this queue instead of being
    // fired concurrently from sendPacket().
    std::deque<std::vector<std::uint8_t>> m_writeQueue;

    MessageHandler m_onMessage;
    DisconnectHandler m_onDisconnect;
    bool m_closed = false;

public:
    explicit TcpConnection(tcp::socket socket);

    // Begins the read loop and registers the callbacks used to hand
    // decoded messages / disconnect notifications back to the owner
    // (TcpServer). Must be called exactly once per connection.
    void start(MessageHandler onMessage, DisconnectHandler onDisconnect);

    void send(NetworkMessageType type, const std::vector<std::uint8_t>& payload) override;
    void close();

    tcp::endpoint remoteEndpoint() const;

private:
    void readHeader();
    void readPayload(NetworkMessageType type, std::uint32_t payloadSize);
    void writeNext();
    void handleError(const boost::system::error_code& ec);
};

} // namespace kungfu
