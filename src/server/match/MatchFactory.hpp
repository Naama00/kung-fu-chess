// server/match/MatchFactory.hpp
#pragma once

#include <memory>
#include <boost/asio.hpp>

namespace kungfu {

class LiveMatch;
class NetworkSession;

// Decouples game board construction and rule setup from MatchManager.
class MatchFactory {
public:
    static std::shared_ptr<LiveMatch> createStandardMatch(
        boost::asio::io_context& ioContext,
        std::uint64_t matchId,
        std::shared_ptr<NetworkSession> player1,
        std::shared_ptr<NetworkSession> player2
    );
};

} // namespace kungfu