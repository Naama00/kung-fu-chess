#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <optional>
#include <vector>

#include "engine/actions/ActionRequest.hpp"
#include "engine/actions/PlayerAction.hpp"
#include "engine/core/GameEngine.hpp"
#include "engine/io/BoardParser.hpp"
#include "engine/snapshot/SnapshotBuilder.hpp"
#include "players/ai/GenericAIPlayer.hpp"
#include "players/ai/RealTimeStrategies.hpp"

TEST_CASE("AIPlayer can produce an action that GameEngine executes", "[integration][ai]") {
    std::string setup =
        "wR . .\n"
        ". . .\n"
        ". . bK\n";

    auto board = kungfu::BoardParser::parse(setup);
    REQUIRE(board != nullptr);

    auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
    auto gameEngine = std::make_shared<kungfu::GameEngine>(board, ruleEngine);
    
    // יצירת הבוט הקל באמצעות הטיפוסים הקונקרטיים בפרויקט שלך
    kungfu::GenericAIPlayer aiPlayer(
        kungfu::PlayerColor::Black, 
        std::make_unique<kungfu::RealTimeEasyStrategy>()
    );

    std::vector<kungfu::ActionRequest> whiteRequests;
    whiteRequests.emplace_back(1, kungfu::PlayerColor::White, kungfu::PlayerAction(kungfu::Position(0, 0), kungfu::Position(0, 1)));
    auto whiteResults = gameEngine->processActionRequests(whiteRequests);
    REQUIRE(whiteResults.size() == 1);
    REQUIRE(whiteResults.front().status == kungfu::ActionStatus::Accepted);

    auto snapshot = kungfu::view::SnapshotBuilder::build(
        *board,
        gameEngine->getArbiter(),
        gameEngine->getCurrentTimeMs(),
        gameEngine->isGameOver(),
        std::nullopt,
        100.0f);

    auto aiRequests = aiPlayer.decideActions(snapshot);
    REQUIRE(!aiRequests.empty());

    auto aiResults = gameEngine->processActionRequests(aiRequests);
    REQUIRE(aiResults.size() == 1);
    REQUIRE(aiResults.front().status == kungfu::ActionStatus::Accepted);

    gameEngine->wait(1500);

    bool boardChanged = false;
    for (const auto& piece : board->pieces()) {
        if (piece && piece->color() == kungfu::PlayerColor::Black && piece->type() == kungfu::PieceType::King) {
            boardChanged = piece->position() != kungfu::Position(2, 2);
            break;
        }
    }

    REQUIRE(boardChanged);
}