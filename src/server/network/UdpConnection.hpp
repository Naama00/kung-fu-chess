// server/network/UdpConnection.hpp
#pragma once

#include <boost/asio.hpp>
#include <memory>
#include "IConnection.hpp"

namespace kungfu {

using boost::asio::ip::udp;

// Realtime (UDP) send-endpoint for one bound client.
//
// The physical udp::socket is owned by UdpServer and shared by every client
// (that is how UDP servers normally work - one socket, many remote
// endpoints). This class only adds the per-client remote endpoint and the
// write serialization required because a single udp::socket must never have
// two concurrent async_send_to calls in flight.
class UdpConnection : public IConnection, public std::enable_shared_from_this<UdpConnection> {
private:
    udp::socket& m_socket;
    udp::endpoint m_remoteEndpoint;
    boost::asio::strand<boost::asio::any_io_executor> m_strand;

public:
    UdpConnection(udp::socket& socket, udp::endpoint remoteEndpoint);

    udp::endpoint remoteEndpoint() const { return m_remoteEndpoint; }

    void send(NetworkMessageType type, const std::vector<std::uint8_t>& payload) override;
};

} // namespace kungfu
