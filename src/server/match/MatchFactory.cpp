// server/match/MatchFactory.cpp
#include "MatchFactory.hpp"
#include "LiveMatch.hpp"
#include "../network/NetworkSession.hpp"
#include "engine/io/BoardParser.hpp"
#include "engine/rules/RuleEngine.hpp"
#include "engine/common/GameConfig.hpp"
#include "engine/core/GameEngine.hpp"
#include <iostream>

namespace kungfu {

std::shared_ptr<LiveMatch> MatchFactory::createStandardMatch(
    boost::asio::io_context& ioContext,
    std::uint64_t matchId,
    std::shared_ptr<NetworkSession> player1,
    std::shared_ptr<NetworkSession> player2
) {
    // Isolate starting board string from the lobby management logic.
    static const std::string kStartBoard =
        "bR bN bB bQ bK bB bN bR\n"
        "bP bP bP bP bP bP bP bP\n"
        ". . . . . . . .\n"
        ". . . . . . . .\n"
        ". . . . . . . .\n"
        ". . . . . . . .\n"
        "wP wP wP wP wP wP wP wP\n"
        "wR wN wB wQ wK wB wN wR\n";

    auto board = BoardParser::parse(kStartBoard);
    auto ruleEngine = std::make_shared<RuleEngine>(board);

    GameConfig config;
    config.allowSimultaneousMovement = true;
    config.enablePremoves = true;
    config.allowJumping = true;

    auto engine = std::make_shared<GameEngine>(board, ruleEngine, config);
    auto match = std::make_shared<LiveMatch>(ioContext, matchId, engine);

    match->setPlayers(player1, player2);

    return match;
}

} // namespace kungfu