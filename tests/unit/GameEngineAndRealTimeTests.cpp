#include <catch2/catch_test_macros.hpp>
#include "board/Board.hpp"
#include "io/BoardParser.hpp"
#include "rules/RuleEngine.hpp"
#include "engine/GameEngine.hpp"

TEST_CASE("Deterministic Simulated Time Flow", "[engine]") {
    std::string boardStr = 
        ". . . .\n"
        "wR . . bK\n" // צריח ב-(1,0), מלך שחור ב-(1,3)
        ". . . .\n";
        
    auto board = kungfu::BoardParser::parse(boardStr);
    REQUIRE(board != nullptr);
    auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
    kungfu::GameEngine game(board, ruleEngine);

    SECTION("Piece remains on source cell during movement") {
        // הזמנת מהלך של שתי משבצות ל-(1,2). משך זמן מתוכנן: 2000 מילישניות.
        auto moveRes = game.requestMove(kungfu::Position(1, 0), kungfu::Position(1, 2));
        REQUIRE(moveRes.isAccepted);

        // אחרי 1500 מילישניות - הכלי עדיין במשבצת המקור (1,0)
        game.wait(1500);
        REQUIRE(board->pieceAt(kungfu::Position(1, 0)).has_value());
        REQUIRE_FALSE(board->pieceAt(kungfu::Position(1, 2)).has_value());

        // אחרי 500 מילישניות נוספות (סה"כ 2000) - הכלי הגיע ליעד (1,2)
        game.wait(500);
        REQUIRE_FALSE(board->pieceAt(kungfu::Position(1, 0)).has_value());
        REQUIRE(board->pieceAt(kungfu::Position(1, 2)).has_value());
    }

    SECTION("Enforce single active motion limitation") {
        // נתחיל מהלך עבור הצריח
        game.requestMove(kungfu::Position(1, 0), kungfu::Position(1, 2));
        
        // ניסיון להזמין מהלך נוסף בזמן שהתנועה פעילה יידחה
        auto secondaryMove = game.requestMove(kungfu::Position(1, 0), kungfu::Position(1, 1));
        REQUIRE_FALSE(secondaryMove.isAccepted);
        REQUIRE(secondaryMove.reason == "motion_in_progress");
    }

    SECTION("Capturing the enemy King ends the game") {
        // מהלך אכילה של המלך ב-(1,3). מרחק: 3 משבצות = 3000 מילישניות.
        game.requestMove(kungfu::Position(1, 0), kungfu::Position(1, 3));
        REQUIRE_FALSE(game.isGameOver());

        // התקדמות של 3000 מילישניות מביאה לאכילה וסיום המשחק
        game.wait(3000);
        REQUIRE(game.isGameOver());

        // לאחר סיום המשחק, בקשות תנועה חדשות נדחות
        auto postGameOverMove = game.requestMove(kungfu::Position(1, 3), kungfu::Position(1, 2));
        REQUIRE_FALSE(postGameOverMove.isAccepted);
        REQUIRE(postGameOverMove.reason == "game_over");
    }

    SECTION("Piece Cooldown after Arrival") {
    // מהלך מהיר של משבצת אחת ל-(1,1) -> משך תנועה: 1000ms.
    auto moveRes = game.requestMove(kungfu::Position(1, 0), kungfu::Position(1, 1));
    REQUIRE(moveRes.isAccepted);

    // המתנה של 1000ms להגעת הכלי ליעד
    game.wait(1000);
    REQUIRE(board->pieceAt(kungfu::Position(1, 1)).has_value());

    // 1. ניסיון תנועה מיידי מיד לאחר ההגעה (0ms מתום הצינון) -> נדחה
    auto quickMove = game.requestMove(kungfu::Position(1, 1), kungfu::Position(1, 2));
    REQUIRE_FALSE(quickMove.isAccepted);
    REQUIRE(quickMove.reason == "piece_on_cooldown");

    // 2. המתנה חלקית של 1500ms (סך הכל פחות מ-2000ms צינון) -> עדיין נדחה
    game.wait(1500);
    auto partialWaitMove = game.requestMove(kungfu::Position(1, 1), kungfu::Position(1, 2));
    REQUIRE_FALSE(partialWaitMove.isAccepted);
    REQUIRE(partialWaitMove.reason == "piece_on_cooldown");

    // 3. השלמת זמן הצינון (עוד 500ms, סה"כ 2000ms מרגע ההגעה) -> המהלך מאושר
    game.wait(500);
    auto afterCooldownMove = game.requestMove(kungfu::Position(1, 1), kungfu::Position(1, 2));
    REQUIRE(afterCooldownMove.isAccepted);
    }
}
