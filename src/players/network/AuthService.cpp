// players/network/AuthService.cpp
#include "players/network/AuthService.hpp"
#include "players/network/ClientAuth.hpp"
#include "server/network/Serializer.hpp"
#include <boost/asio.hpp>
#include <thread>
#include <vector>

namespace kungfu {

AuthResult AuthService::authenticate(const std::string& username,
                                     const std::string& password,
                                     bool isRegister,
                                     const std::string& host,
                                     const std::string& port) {
    AuthResult res{false, "Connection error", static_cast<int>(ClientConfig::kDefaultRating)};
    try {
        boost::asio::io_context ioContext;
        boost::asio::ip::tcp::socket socket(ioContext);
        boost::asio::ip::tcp::resolver resolver(ioContext);

        boost::system::error_code ec;
        auto endpoints = resolver.resolve(host, port, ec);
        if (ec) {
            res.message = "Could not resolve server address";
            return res;
        }

        boost::asio::connect(socket, endpoints, ec);
        if (ec) {
            res.message = "Server is offline";
            return res;
        }

        // Serialize and send credential frame
        auto payload = Serializer::serializeAuthRequest(username, password);
        auto type = isRegister ? NetworkMessageType::REGISTER_REQUEST : NetworkMessageType::LOGIN_REQUEST;
        auto frame = Serializer::buildFrame(type, payload);

        boost::asio::write(socket, boost::asio::buffer(frame), ec);
        if (ec) {
            res.message = "Failed to send credentials";
            return res;
        }

        // Read response header
        std::vector<std::uint8_t> headerBuf(kHeaderSize);
        boost::asio::read(socket, boost::asio::buffer(headerBuf), ec);
        if (ec) {
            res.message = "No response from server";
            return res;
        }

        std::size_t offset = 0;
        std::uint8_t resType = 0;
        std::uint32_t payloadSize = 0;
        Serializer::readU8(headerBuf, offset, resType);
        Serializer::readU32(headerBuf, offset, payloadSize);

        // Read response payload
        std::vector<std::uint8_t> responsePayload(payloadSize);
        if (payloadSize > 0) {
            boost::asio::read(socket, boost::asio::buffer(responsePayload), ec);
            if (ec) {
                res.message = "Incomplete response from server";
                return res;
            }
        }

        // Parse result
        if (resType == static_cast<std::uint8_t>(NetworkMessageType::REGISTER_RESPONSE)) {
            if (!responsePayload.empty() && responsePayload[0] == 1) {
                res.success = true;
                res.message = "Registration successful!";
            } else {
                res.message = "Registration failed. Username taken.";
            }
        }

        boost::system::error_code ignored;
        socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
        socket.close(ignored);
    } catch (const std::exception& e) {
        res.message = e.what();
    }
    return res;
}

std::shared_ptr<NetworkSession> AuthService::createAuthenticatedSession(const std::string& username,
                                                                         const std::string& password,
                                                                         const std::string& host,
                                                                         const std::string& port) {
    // Save credentials in global ClientAuth context
    ClientAuth::username = username;
    ClientAuth::password = password;
    ClientAuth::isAuthenticated = true;

    // Instantiate and spin up persistent NetworkSession
    auto authSession = std::make_shared<NetworkSession>();
    authSession->player = std::make_shared<NetworkPlayer>(
        authSession->ioContext, host, port, false, 0, 0, true);
    authSession->player->connectAndJoin();

    auto session = authSession;
    authSession->thread = std::thread([session]() {
        boost::asio::io_context::work work(session->ioContext);
        session->ioContext.run();
    });

    return authSession;
}

} // namespace kungfu