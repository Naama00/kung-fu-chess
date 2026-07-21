// server/match/MatchFactory.cpp
#include "MatchFactory.hpp"
#include "LiveMatch.hpp"
#include "../network/NetworkSession.hpp"
#include "engine/io/BoardParser.hpp"
#include "engine/rules/RuleEngine.hpp"
#include "engine/common/GameConfig.hpp"
#include "engine/core/GameEngine.hpp"
#include "engine/common/BoardPresets.hpp"
#include <iostream>

namespace kungfu {

std::shared_ptr<LiveMatch> MatchFactory::createStandardMatch(
    boost::asio::io_context& ioContext,
    std::uint64_t matchId,
    std::shared_ptr<NetworkSession> player1,
    std::shared_ptr<NetworkSession> player2
) {
    // Isolate starting board string from the lobby management logic.
    static const std::string_view kStartBoard = BoardPresets::kStandardStartBoard;
    auto board = BoardParser::parse(kStartBoard);
    auto ruleEngine = std::make_shared<RuleEngine>(board);
    GameConfig config;
    auto engine = std::make_shared<GameEngine>(board, ruleEngine, config);
    auto match = std::make_shared<LiveMatch>(ioContext, matchId, engine);
    match->setPlayers(player1, player2);
    return match;
}

} // namespace kungfu