// players/network/NetworkSession.hpp
#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include "NetworkPlayer.hpp"

namespace kungfu {

// Bundles a persistent, already-authenticated connection (io_context +
// NetworkPlayer + the thread running that io_context) so it can be carried
// across screen transitions (LoginScreen -> StartScreen -> ChessGameScreen)
// without opening a second TCP connection for the same login.
//
// Held via shared_ptr: whichever screens currently reference it keep it
// alive, and the io_context/thread are torn down exactly once - in this
// destructor, when the last reference goes away - regardless of which
// screen that turns out to be.
struct NetworkSession {
    boost::asio::io_context ioContext;
    std::shared_ptr<NetworkPlayer> player;
    std::thread thread;

    NetworkSession() = default;
    NetworkSession(const NetworkSession&) = delete;
    NetworkSession& operator=(const NetworkSession&) = delete;

    ~NetworkSession() {
        ioContext.stop();
        if (thread.joinable()) {
            thread.join();
        }
    }
};

} // namespace kungfu