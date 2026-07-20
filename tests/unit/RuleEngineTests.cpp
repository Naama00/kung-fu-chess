#include <catch2/catch_test_macros.hpp>
#include "engine/board/Board.hpp"
#include "engine/io/BoardParser.hpp"
#include "engine/rules/RuleEngine.hpp"

TEST_CASE("RuleEngine Basic Validations", "[rules]") {
    auto board = std::make_shared<kungfu::Board>(8, 8);
    kungfu::RuleEngine engine(board);

    SECTION("Empty source error") {
        auto result = engine.validateMove(kungfu::Position(0, 0), kungfu::Position(1, 1));
        REQUIRE_FALSE(result.isValid);
        REQUIRE(result.reason == "empty_source");
    }

    SECTION("Out of board bounds error") {
        auto result = engine.validateMove(kungfu::Position(-1, 0), kungfu::Position(0, 0));
        REQUIRE_FALSE(result.isValid);
        REQUIRE(result.reason == "outside_board");
    }
}

TEST_CASE("Piece Specific Rules Integration", "[rules]") {

    SECTION("Rook sliding and blocking") {
        std::string boardStr =
            ". . . . .\n"
            ". wR . bP .\n" // The rook at (1,1) is blocked by an enemy piece at (1,3) and can capture it
            ". . . . .\n"
            ". wP . . .\n" // The rook is blocked by a friendly piece at (3,1) and cannot reach it or pass through it
            ". . . . .\n";

        auto board = kungfu::BoardParser::parse(boardStr);
        REQUIRE(board != nullptr);
        kungfu::RuleEngine engine(board);

        // Clear move to the right
        REQUIRE(engine.validateMove(kungfu::Position(1, 1), kungfu::Position(1, 2)).isValid);

        // Capturing an enemy piece at the target is allowed
        REQUIRE(engine.validateMove(kungfu::Position(1, 1), kungfu::Position(1, 3)).isValid);

        // Passing through an enemy piece is forbidden
        REQUIRE_FALSE(engine.validateMove(kungfu::Position(1, 1), kungfu::Position(1, 4)).isValid);

        // Blocked by a friendly piece
        auto blockResult = engine.validateMove(kungfu::Position(1, 1), kungfu::Position(3, 1));
        REQUIRE_FALSE(blockResult.isValid);
        REQUIRE(blockResult.reason == "friendly_destination");
    }

    SECTION("Knight jumps over other pieces") {
        std::string boardStr =
            "wN wP .\n"
            "wP wP .\n"
            ". . .\n";

        auto board = kungfu::BoardParser::parse(boardStr);
        REQUIRE(board != nullptr);
        kungfu::RuleEngine engine(board);

        // הפרש ב-(0,0) מוקף לחלוטין בכלים ידידותיים ב-(0,1), (1,0), (1,1)
        // הוא עדיין יכול לקפוץ מעליהם ל-(2,1) או (1,2)
        REQUIRE(engine.validateMove(kungfu::Position(0, 0), kungfu::Position(2, 1)).isValid);
        REQUIRE(engine.validateMove(kungfu::Position(0, 0), kungfu::Position(1, 2)).isValid);
    }

    SECTION("Simplified Pawn rules") {
        std::string boardStr =
            ". . .\n"
            "bP . wP\n"
            ". . .\n"; // לוח בגודל 3X3. שחור ב-(1,0), לבן ב-(1,2)

        auto board = kungfu::BoardParser::parse(boardStr);
        REQUIRE(board != nullptr);
        kungfu::RuleEngine engine(board);

        // pawn לבן נע למעלה ל-(0,2) (1 - 1 = 0)
        REQUIRE(engine.validateMove(kungfu::Position(1, 2), kungfu::Position(0, 2)).isValid);

        // pawn שחור נע למטה ל-(2,0) (1 + 1 = 2)
        REQUIRE(engine.validateMove(kungfu::Position(1, 0), kungfu::Position(2, 0)).isValid);

        // אסור לזוז באלכסון אם המשבצת ריקה
        REQUIRE_FALSE(engine.validateMove(kungfu::Position(1, 2), kungfu::Position(0, 1)).isValid);
    }
}

TEST_CASE("King, Bishop, and Queen Illegal Movements", "[rules]") {
    std::string boardStr =
        "wK . . . .\n"
        ". . . . .\n"
        ". . wB . .\n"
        ". . . . wQ\n"; // מלך ב-(0,0), רץ ב-(2,2), מלכה ב-(3,4)

    auto board = kungfu::BoardParser::parse(boardStr);
    REQUIRE(board != nullptr);
    kungfu::RuleEngine engine(board);

    SECTION("King cannot move two cells") {
        // מלך ב-(0,0) מנסה לזוז 2 משבצות ימינה ל-(0,2) או למטה ל-(2,0)
        auto res1 = engine.validateMove(kungfu::Position(0, 0), kungfu::Position(0, 2));
        REQUIRE_FALSE(res1.isValid);
        REQUIRE(res1.reason == "illegal_piece_move");

        // מלך ב-(0,0) מנסה לזוז משבצת אחת באלכסון - תקין
        auto res2 = engine.validateMove(kungfu::Position(0, 0), kungfu::Position(1, 1));
        REQUIRE(res2.isValid);
    }

    SECTION("Bishop cannot move orthogonally") {
        // רץ ב-(2,2) מנסה לזוז ימינה ל-(2,3) או למעלה ל-(1,2)
        auto res = engine.validateMove(kungfu::Position(2, 2), kungfu::Position(2, 3));
        REQUIRE_FALSE(res.isValid);
        REQUIRE(res.reason == "illegal_piece_move");
    }

    SECTION("Rook cannot move diagonally") {
        // נניח צריח לבן ב-(1,3) — משבצת פנויה בלוח
        auto rook = std::make_shared<kungfu::Piece>(kungfu::PieceType::Rook, kungfu::PlayerColor::White, kungfu::Position(1, 3));
        board->placePiece(rook, kungfu::Position(1, 3));

        // צריח מנסה לזוז באלכסון ל-(2,4)
        auto res = engine.validateMove(kungfu::Position(1, 3), kungfu::Position(2, 4));
        REQUIRE_FALSE(res.isValid);
        REQUIRE(res.reason == "illegal_piece_move");
    }

    SECTION("Queen cannot jump like a knight") {
        // מלכה ב-(3,4) מנסה לקפוץ תנועת פרש ל-(1,3)
        auto res = engine.validateMove(kungfu::Position(3, 4), kungfu::Position(1, 3));
        REQUIRE_FALSE(res.isValid);
        REQUIRE(res.reason == "illegal_piece_move");
    }
}

TEST_CASE("Pawn Movement and Capture Verification", "[rules]") {
    std::string boardStr =
        ". . . . .\n"
        "bP . bB bR .\n" // שורה 1: שחור ב-(1,0), רץ שחור ב-(1,2), צריח שחור ב-(1,3)
        "wR wN wP . .\n" // שורה 2: צריח לבן ב-(2,0), פרש לבן ב-(2,1), לבן ב-(2,2)
        ". . . . .\n";

    auto board = kungfu::BoardParser::parse(boardStr);
    REQUIRE(board != nullptr);
    kungfu::RuleEngine engine(board);

    SECTION("White Pawn rules") {
        // 1. התקדמות קדימה (למעלה) חסומה על ידי כלי עוין (רץ שחור ב-1,2) -> אסור לאכול קדימה (עבור רגלי לבן ב-2,2)
        auto forwardBlock = engine.validateMove(kungfu::Position(2, 2), kungfu::Position(1, 2));
        REQUIRE_FALSE(forwardBlock.isValid);

        // 2. אכילה באלכסון ימינה (למעלה) של הצריח השחור ב-(1,3) -> מותר
        auto diagonalCapture = engine.validateMove(kungfu::Position(2, 2), kungfu::Position(1, 3));
        REQUIRE(diagonalCapture.isValid);

        // 3. תנועה של 2 משבצות קדימה ל-(0,2) -> אסור
        auto moveTwoCells = engine.validateMove(kungfu::Position(2, 2), kungfu::Position(0, 2));
        REQUIRE_FALSE(moveTwoCells.isValid);
    }

    SECTION("Black Pawn rules") {
        // 1. התקדמות קדימה חסומה על ידי כלי עוין (צריח לבן ב-2,0) -> אסור לאכול קדימה
        auto forwardBlock = engine.validateMove(kungfu::Position(1, 0), kungfu::Position(2, 0));
        REQUIRE_FALSE(forwardBlock.isValid);

        // 2. אכילה באלכסון ימינה של הפרש הלבן ב-(2,1) -> מותר
        auto diagonalCapture = engine.validateMove(kungfu::Position(1, 0), kungfu::Position(2, 1));
        REQUIRE(diagonalCapture.isValid);

        // 3. תנועה של 2 משבצות קדימה ל-(3,0) -> אסור
        auto moveTwoCells = engine.validateMove(kungfu::Position(1, 0), kungfu::Position(3, 0));
        REQUIRE_FALSE(moveTwoCells.isValid);
    }
}

TEST_CASE("Advanced Pawn Rules (Double Step and Path Clearance)", "[rules]") {
    // שורת ההתחלה של שחור היא 1.
    std::string setupStr =
        ". . . . . . . .\n" // 0
        "bP . bP . . . . .\n" // 1 (bP ב-1,0 חופשי, bP ב-1,2 חסום ב-2,2)
        ". . wR . . . . .\n" // 2 (wR ב-2,2 חוסם את הדרך)
        ". . . . . . . .\n" // 3
        ". . . . . . . .\n" // 4
        ". . . . . . . .\n" // 5
        ". . . . . . . .\n" // 6
        ". . . . . . . .\n"; // 7

    auto board = kungfu::BoardParser::parse(setupStr);
    REQUIRE(board != nullptr);
    kungfu::RuleEngine engine(board);

    // 1. רגלי חופשי בשורת התחלה יכול לצעוד צעד כפול
    auto doubleStepOk = engine.validateMove(kungfu::Position(1, 0), kungfu::Position(3, 0));
    REQUIRE(doubleStepOk.isValid);

    // 2. רגלי שדרך הביניים שלו חסומה - אינו יכול לבצע צעד כפול
    auto doubleStepBlocked = engine.validateMove(kungfu::Position(1, 2), kungfu::Position(3, 2));
    REQUIRE_FALSE(doubleStepBlocked.isValid);

    // 3. רגלי עדיין יכול לבצע צעד בודד רגיל אם הדרך פנויה
    auto singleStepOk = engine.validateMove(kungfu::Position(1, 0), kungfu::Position(2, 0));
    REQUIRE(singleStepOk.isValid);
}

TEST_CASE("Pawn Double Step Depends on hasMoved, Not Absolute Row", "[rules]") {
    // לוח לא סטנדרטי לגמרי - הרגלי השחור מתחיל בשורה 4, לא בשורה 1 הקבועה מהעבר
    std::string boardStr =
        ". . .\n"
        ". . .\n"
        ". . .\n"
        ". . .\n"
        "bP . .\n"
        ". . .\n"
        ". . .\n";

    auto board = kungfu::BoardParser::parse(boardStr);
    REQUIRE(board != nullptr);
    kungfu::RuleEngine engine(board);
    auto pawn = board->pieceAt(kungfu::Position(4, 0)).value();

    SECTION("Fresh pawn can double-step regardless of its absolute row") {
        auto result = engine.validateMove(kungfu::Position(4, 0), kungfu::Position(6, 0));
        REQUIRE(result.isValid);
    }

    SECTION("A pawn that already moved cannot double-step, even if the path is clear") {
        pawn->markMoved();
        auto result = engine.validateMove(kungfu::Position(4, 0), kungfu::Position(6, 0));
        REQUIRE_FALSE(result.isValid);
    }
}

TEST_CASE("Knight Cannot Target Friendly Squares", "[rules][knight]") {
    // פרש לבן ב-(0,0) מוקף ברגלים לבנים – תנועה לריבוע ידידותי אסורה.
    std::string boardStr =
        "wN . .\n"
        ". . .\n"
        ". wP .\n";

    auto board = kungfu::BoardParser::parse(boardStr);
    REQUIRE(board != nullptr);
    kungfu::RuleEngine engine(board);

    // קפיצה ל-(2,1) שם יושב רגלי ידידותי – אסור
    auto res = engine.validateMove(kungfu::Position(0, 0), kungfu::Position(2, 1));
    REQUIRE_FALSE(res.isValid);
    REQUIRE(res.reason == "friendly_destination");
}

TEST_CASE("Non-Knight Pieces Cannot Target Friendly Squares", "[rules][knight]") {
    // וידוא שהחריג לפרשים לא השפיע על כלים אחרים
    std::string boardStr =
        "wR . .\n"
        ". . .\n"
        "wP . .\n";

    auto board = kungfu::BoardParser::parse(boardStr);
    REQUIRE(board != nullptr);
    kungfu::RuleEngine engine(board);

    // צריח לבן ב-(0,0) מנסה לנוע ל-(2,0) שם יושב רגלי לבן – אסור
    auto res = engine.validateMove(kungfu::Position(0, 0), kungfu::Position(2, 0));
    REQUIRE_FALSE(res.isValid);
    REQUIRE(res.reason == "friendly_destination");
}