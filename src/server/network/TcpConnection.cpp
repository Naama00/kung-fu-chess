// server/network/TcpConnection.cpp
#include "TcpConnection.hpp"
#include "Serializer.hpp"
#include <iostream>

namespace kungfu {

TcpConnection::TcpConnection(tcp::socket socket)
    : m_socket(std::move(socket)),
      m_strand(boost::asio::make_strand(m_socket.get_executor())) {}

tcp::endpoint TcpConnection::remoteEndpoint() const {
    boost::system::error_code ec;
    return m_socket.remote_endpoint(ec);
}

void TcpConnection::start(MessageHandler onMessage, DisconnectHandler onDisconnect) {
    m_onMessage = std::move(onMessage);
    m_onDisconnect = std::move(onDisconnect);
    readHeader();
}

void TcpConnection::readHeader() {
    auto self = shared_from_this();
    boost::asio::async_read(m_socket, boost::asio::buffer(m_headerBuffer),
        boost::asio::bind_executor(m_strand, [this, self](boost::system::error_code ec, std::size_t) {
            if (ec) {
                handleError(ec);
                return;
            }

            std::size_t offset = 0;
            std::uint8_t rawType = 0;
            std::uint32_t payloadSize = 0;
            std::vector<std::uint8_t> header(m_headerBuffer.data(), m_headerBuffer.data() + kHeaderSize);

            bool ok = Serializer::readU8(header, offset, rawType) &&
                      Serializer::readU32(header, offset, payloadSize);

            if (!ok || payloadSize > kMaxPayloadSize) {
                std::cerr << "[TcpConnection] Malformed or oversized frame header, closing connection." << std::endl;
                close();
                return;
            }

            readPayload(static_cast<NetworkMessageType>(rawType), payloadSize);
        }));
}

void TcpConnection::readPayload(NetworkMessageType type, std::uint32_t payloadSize) {
    m_payloadBuffer.assign(payloadSize, 0);

    if (payloadSize == 0) {
        if (m_onMessage) m_onMessage(type, m_payloadBuffer);
        readHeader();
        return;
    }

    auto self = shared_from_this();
    boost::asio::async_read(m_socket, boost::asio::buffer(m_payloadBuffer),
        boost::asio::bind_executor(m_strand, [this, self, type](boost::system::error_code ec, std::size_t) {
            if (ec) {
                handleError(ec);
                return;
            }
            if (m_onMessage) m_onMessage(type, m_payloadBuffer);
            readHeader();
        }));
}

void TcpConnection::send(NetworkMessageType type, const std::vector<std::uint8_t>& payload) {
    auto frame = Serializer::buildFrame(type, payload);
    auto self = shared_from_this();

    boost::asio::post(m_strand, [this, self, frame = std::move(frame)]() mutable {
        bool writeInProgress = !m_writeQueue.empty();
        m_writeQueue.push_back(std::move(frame));
        if (!writeInProgress) {
            writeNext();
        }
    });
}

void TcpConnection::writeNext() {
    auto self = shared_from_this();
    boost::asio::async_write(m_socket, boost::asio::buffer(m_writeQueue.front()),
        boost::asio::bind_executor(m_strand, [this, self](boost::system::error_code ec, std::size_t) {
            if (ec) {
                handleError(ec);
                return;
            }
            m_writeQueue.pop_front();
            if (!m_writeQueue.empty()) {
                writeNext();
            }
        }));
}

void TcpConnection::handleError(const boost::system::error_code& ec) {
    if (m_closed) return;
    if (ec != boost::asio::error::eof) {
        std::cerr << "[TcpConnection] Connection error: " << ec.message() << std::endl;
    }
    close();
}

void TcpConnection::close() {
    if (m_closed) return;
    m_closed = true;

    boost::system::error_code ignored;
    m_socket.shutdown(tcp::socket::shutdown_both, ignored);
    m_socket.close(ignored);

    if (m_onDisconnect) m_onDisconnect();
}

} // namespace kungfu
