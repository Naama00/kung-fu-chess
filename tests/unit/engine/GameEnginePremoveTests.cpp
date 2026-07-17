#include <catch2/catch_test_macros.hpp>
#include "engine/board/Board.hpp"
#include "engine/io/BoardParser.hpp"
#include "engine/rules/RuleEngine.hpp"
#include "engine/core/GameEngine.hpp"

TEST_CASE("Requests while a piece is on cooldown are rejected", "[engine][premove]") {
    std::string boardStr =
        "wR . .\n"
        ". . .\n"
        ". . .\n";

    auto board = kungfu::BoardParser::parse(boardStr);
    auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
    
    // אנו מכבים פרה-מובס במפורש כדי לוודא שפעולות רגילות נדחות בזמן צינון
    kungfu::GameConfig cfg;
    cfg.enablePremoves = false;
    kungfu::GameEngine game(board, ruleEngine, cfg);

    auto firstMove = game.requestMove(kungfu::Position(0, 0), kungfu::Position(0, 1));
    REQUIRE(firstMove.isAccepted);

    game.wait(1500);

    auto cooldownMove = game.requestMove(kungfu::Position(0, 1), kungfu::Position(0, 2));
    REQUIRE_FALSE(cooldownMove.isAccepted);
    REQUIRE(cooldownMove.reason == "piece_on_cooldown");
}

TEST_CASE("Premoves Are Disabled in Turn-Based Mode", "[engine][premove]") {
    std::string boardStr =
        "wR . .\n"
        ". . .\n"
        ". . bR\n";

    auto board = kungfu::BoardParser::parse(boardStr);
    auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
    kungfu::GameConfig cfg;
    cfg.allowSimultaneousMovement = false;
    cfg.enablePremoves = true; // מאופשר בקונפיג, אבל לא רלוונטי במצב תור-תור
    kungfu::GameEngine game(board, ruleEngine, cfg);

    auto whiteMove = game.requestMove(kungfu::Position(0, 0), kungfu::Position(0, 2));
    REQUIRE(whiteMove.isAccepted);

    auto secondAttempt = game.requestMove(kungfu::Position(0, 0), kungfu::Position(0, 1));
    REQUIRE_FALSE(secondAttempt.isAccepted);
    REQUIRE(secondAttempt.reason != "premove_registered");

    REQUIRE(game.currentTurn() == kungfu::PlayerColor::Black);
}

TEST_CASE("Premoves Still Work Normally in Simultaneous Mode", "[engine][premove]") {
    std::string boardStr =
        "wR . .\n"
        ". . .\n"
        ". . .\n";

    auto board = kungfu::BoardParser::parse(boardStr);
    auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
    kungfu::GameConfig cfg; // allowSimultaneousMovement=true, enablePremoves=true כברירת מחדל
    kungfu::GameEngine game(board, ruleEngine, cfg);

    auto firstMove = game.requestMove(kungfu::Position(0, 0), kungfu::Position(0, 1));
    REQUIRE(firstMove.isAccepted);

    auto premove = game.requestMove(kungfu::Position(0, 0), kungfu::Position(0, 2));
    REQUIRE(premove.isAccepted);
    REQUIRE(premove.reason == "premove_registered");
}

TEST_CASE("Premove Queue and Auto-Execution", "[engine][premove]") {
    std::string boardStr =
        "wR . .\n"
        ". . .\n"
        ". . .\n"; // צריח לבן ב-(0,0)

    auto board = kungfu::BoardParser::parse(boardStr);
    REQUIRE(board != nullptr);
    auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
    kungfu::GameEngine game(board, ruleEngine);

    auto res1 = game.requestMove(kungfu::Position(0, 0), kungfu::Position(0, 1));
    REQUIRE(res1.isAccepted);
    REQUIRE(res1.reason == "ok");

    game.wait(100);
    auto res2 = game.requestMove(kungfu::Position(0, 0), kungfu::Position(0, 2));
    REQUIRE(res2.isAccepted);
    REQUIRE(res2.reason == "premove_registered");

    game.wait(900);
    REQUIRE(board->pieceAt(kungfu::Position(0, 1)).has_value());
    REQUIRE_FALSE(board->pieceAt(kungfu::Position(0, 2)).has_value());

    game.wait(1000);
    REQUIRE_FALSE(board->pieceAt(kungfu::Position(0, 2)).has_value());

    game.wait(1000);
    REQUIRE_FALSE(board->pieceAt(kungfu::Position(0, 2)).has_value());

    game.wait(1000);
    REQUIRE_FALSE(board->pieceAt(kungfu::Position(0, 1)).has_value());
    REQUIRE(board->pieceAt(kungfu::Position(0, 2)).has_value());
}

TEST_CASE("Failed Premove Execution Is Observable, Not Silently Swallowed", "[engine][premove]") {
    std::string boardStr =
        "wR . bR\n"
        ". . .\n"
        ". . .\n"; // צריח לבן ב-(0,0), צריח שחור ב-(0,2)

    auto board = kungfu::BoardParser::parse(boardStr);
    auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
    kungfu::GameEngine game(board, ruleEngine);

    auto firstMove = game.requestMove(kungfu::Position(0, 0), kungfu::Position(0, 1));
    REQUIRE(firstMove.isAccepted);

    game.wait(100);
    auto premove = game.requestMove(kungfu::Position(0, 0), kungfu::Position(0, 3)); // מחוץ ללוח
    REQUIRE(premove.isAccepted);
    REQUIRE(premove.reason == "premove_registered");

    // t=1000: הכלי נוחת אך עדיין בצינון (2000ms) - ה-premove לא מנוסה עדיין
    game.wait(900);
    REQUIRE(game.lastPremoveFailures().empty());

    // t=3000: הצינון מסתיים - עכשיו ה-premove ה"לא חוקי" מנוסה ונכשל בגלוי
    game.wait(2000);
    REQUIRE_FALSE(game.lastPremoveFailures().empty());
    REQUIRE(game.lastPremoveFailures()[0].reason == "outside_board");

    REQUIRE(board->pieceAt(kungfu::Position(0, 1)).has_value());
}

TEST_CASE("Illegal Premove Is Overridden by New Premove", "[engine][premove]") {
    // שחקן רושם premove לא חוקי, ואז דורס אותו ב-premove חוקי
    std::string boardStr =
        "wR . . .\n"
        ". . . .\n"
        ". . . .\n";

    auto board = kungfu::BoardParser::parse(boardStr);
    REQUIRE(board != nullptr);
    auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
    kungfu::GameEngine game(board, ruleEngine);

    // מהלך ראשון מתחיל תנועה מ-(0,0) ל-(0,1); 1000ms/תא → נחיתה ב-t=1000
    auto firstMove = game.requestMove(kungfu::Position(0, 0), kungfu::Position(0, 1));
    REQUIRE(firstMove.isAccepted);

    // "ביטול" ע"י premove לא חוקי – יעד מחוץ ללוח
    auto cancelAttempt = game.requestMove(kungfu::Position(0, 0), kungfu::Position(0, 9));
    REQUIRE(cancelAttempt.isAccepted);
    REQUIRE(cancelAttempt.reason == "premove_registered");

    // דורסים עם premove חוקי ל-(0,2)
    auto realPremove = game.requestMove(kungfu::Position(0, 0), kungfu::Position(0, 2));
    REQUIRE(realPremove.isAccepted);
    REQUIRE(realPremove.reason == "premove_registered");

    // t=1000: נחיתה ב-(0,1), cooldown עד t=3000
    game.wait(1000);
    REQUIRE(board->pieceAt(kungfu::Position(0, 1)).has_value());

    // t=3000: cooldown מסתיים → premove מופעל; 1000ms נסיעה → נחיתה ב-t=4000
    game.wait(2000);

    // t=4500: הצריח צריך להיות ב-(0,2)
    game.wait(1500);
    REQUIRE(board->pieceAt(kungfu::Position(0, 2)).has_value());
    REQUIRE(game.lastPremoveFailures().empty());
}

TEST_CASE("Premove Registered for Piece on Cooldown Executes After Cooldown", "[engine][premove]") {
    // פרש לבן ב-(0,0); אחרי תנועה רושמים premove בזמן התנועה
    // jumpDurationMs=1000ms; cooldown=2000ms
    std::string boardStr =
        "wN . . .\n"
        ". . . .\n"
        ". . . .\n"
        ". . . .\n";

    auto board = kungfu::BoardParser::parse(boardStr);
    REQUIRE(board != nullptr);
    auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
    kungfu::GameEngine game(board, ruleEngine);

    // פרש קופץ מ-(0,0) ל-(2,1) – distance=max(2,1)=2 × 1000ms/תא = 2000ms → נחיתה ב-t=2000
    auto firstMove = game.requestMove(kungfu::Position(0, 0), kungfu::Position(2, 1));
    REQUIRE(firstMove.isAccepted);

    // הפרש בתנועה – רושמים premove לקפיצה הבאה מ-(2,1) ל-(3,3) – תנועת פרש חוקית
    auto premove = game.requestMove(kungfu::Position(0, 0), kungfu::Position(3, 3));
    REQUIRE(premove.isAccepted);
    REQUIRE(premove.reason == "premove_registered");

    // t=2000: נחיתה ב-(2,1), cooldown עד t=4000
    game.wait(2000);
    REQUIRE(board->pieceAt(kungfu::Position(2, 1)).has_value()); // נחת – עדיין בצינון

    // t=4000: cooldown מסתיים → premove מופעל; distance=max(1,2)=2 × 1000ms = 2000ms → נחיתה ב-t=6000
    game.wait(2000);

    // t=6500: הפרש צריך להיות ב-(3,3)
    game.wait(2500);
    REQUIRE(board->pieceAt(kungfu::Position(3, 3)).has_value());
    REQUIRE(board->pieceAt(kungfu::Position(3, 3)).value()->type() == kungfu::PieceType::Knight);
}
