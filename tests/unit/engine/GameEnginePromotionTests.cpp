#include <catch2/catch_test_macros.hpp>
#include "board/Board.hpp"
#include "io/BoardParser.hpp"
#include "rules/RuleEngine.hpp"
#include "engine/GameEngine.hpp"

namespace {

// חוק הכתרה מדומה לצורך בדיקה: רגלי שמגיע לשורה האחרונה לעולם לא מוכתר
class NoPromotionRule : public kungfu::IPromotionRule {
public:
    kungfu::PiecePtr maybePromote(const kungfu::PiecePtr& piece, const kungfu::Position&, kungfu::IBoard&) const override {
        return piece;
    }
};

} // namespace

TEST_CASE("GameEngine Supports Injectable Promotion Rules", "[engine][promotion]") {
    std::string boardStr =
        ". . .\n"
        "wP . .\n"
        ". . .\n";

    auto board = kungfu::BoardParser::parse(boardStr);
    auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
    kungfu::GameConfig cfg;
    auto noPromotion = std::make_shared<NoPromotionRule>();

    kungfu::GameEngine game(board, ruleEngine, cfg, noPromotion);

    auto res = game.requestMove(kungfu::Position(1, 0), kungfu::Position(2, 0));
    REQUIRE(res.isAccepted);

    game.wait(1000);

    auto pieceOpt = board->pieceAt(kungfu::Position(2, 0));
    REQUIRE(pieceOpt.has_value());
    REQUIRE(pieceOpt.value()->type() == kungfu::PieceType::Pawn);
}

TEST_CASE("Pawn Promotion to Queen on Last Row", "[engine][promotion]") {
    std::string boardStr =
        ". . .\n"
        "wP . .\n"
        ". . .\n"; // לוח 3x3. השורה האחרונה עבור לבן היא שורה 2.

    auto board = kungfu::BoardParser::parse(boardStr);
    REQUIRE(board != nullptr);
    auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
    kungfu::GameEngine game(board, ruleEngine);

    auto res = game.requestMove(kungfu::Position(1, 0), kungfu::Position(2, 0));
    REQUIRE(res.isAccepted);

    game.wait(1000);

    auto pieceOpt = board->pieceAt(kungfu::Position(2, 0));
    REQUIRE(pieceOpt.has_value());
    REQUIRE(pieceOpt.value()->type() == kungfu::PieceType::Queen);
}

TEST_CASE("Promoted Piece Gets Its Own Fresh Cooldown, Independent of the Original Pawn", "[engine][promotion]") {
    std::string boardStr =
        ". . .\n"
        "wP . .\n"
        ". . .\n";

    auto board = kungfu::BoardParser::parse(boardStr);
    auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
    kungfu::GameEngine game(board, ruleEngine);

    auto res = game.requestMove(kungfu::Position(1, 0), kungfu::Position(2, 0));
    REQUIRE(res.isAccepted);
    game.wait(1000); // הרגלי מגיע ומוכתר למלכה

    auto queen = board->pieceAt(kungfu::Position(2, 0)).value();
    REQUIRE(queen->type() == kungfu::PieceType::Queen);

    auto immediateMove = game.requestMove(kungfu::Position(2, 0), kungfu::Position(2, 1));
    REQUIRE_FALSE(immediateMove.isAccepted);
    REQUIRE(immediateMove.reason == "piece_on_cooldown");

    game.wait(2000);
    auto afterCooldown = game.requestMove(kungfu::Position(2, 0), kungfu::Position(2, 1));
    REQUIRE(afterCooldown.isAccepted);
}

TEST_CASE("Promotion Does Not Leave Orphaned Cooldown Entries", "[engine][promotion]") {
    std::string boardStr =
        ". . .\n"
        "wP . .\n"
        ". . .\n";

    auto board = kungfu::BoardParser::parse(boardStr);
    auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
    kungfu::GameEngine game(board, ruleEngine);

    auto res = game.requestMove(kungfu::Position(1, 0), kungfu::Position(2, 0));
    REQUIRE(res.isAccepted);
    game.wait(1000); // הרגלי מגיע ומוכתר למלכה

    // בדיוק רשומת cooldown אחת - זו של המלכה החדשה, לא שתיים
    REQUIRE(game.getArbiter().cooldownEntryCount() == 1);
}