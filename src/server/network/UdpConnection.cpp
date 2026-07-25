// server/network/UdpConnection.cpp
#include "UdpConnection.hpp"
#include "Serializer.hpp"
#include <iostream>

namespace kungfu {

UdpConnection::UdpConnection(udp::socket& socket, udp::endpoint remoteEndpoint)
    : m_socket(socket),
      m_remoteEndpoint(std::move(remoteEndpoint)),
      m_strand(boost::asio::make_strand(socket.get_executor())) {}

void UdpConnection::send(NetworkMessageType type, const std::vector<std::uint8_t>& payload) {
    auto frame = std::make_shared<std::vector<std::uint8_t>>(Serializer::buildFrame(type, payload));
    auto self = shared_from_this();

    boost::asio::post(m_strand, [this, self, frame]() {
        m_socket.async_send_to(boost::asio::buffer(*frame), m_remoteEndpoint,
            boost::asio::bind_executor(m_strand,
            [self](boost::system::error_code ec, std::size_t) {
                if (ec) {
                    std::cerr << "[UdpConnection] Send error: " << ec.message() << std::endl;
                }
            }));
    });
}

} // namespace kungfu
