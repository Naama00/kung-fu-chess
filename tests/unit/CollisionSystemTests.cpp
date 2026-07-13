#include <catch2/catch_test_macros.hpp>
#include "realtime/CollisionDetector.hpp"
#include "rules/CollisionResolver.hpp"
#include "board/Board.hpp"
#include "io/BoardParser.hpp"

TEST_CASE("CollisionDetector Spatial Calculations", "[realtime][collision]") {
    kungfu::GameConfig config;
    config.allowSimultaneousMovement = true;

    auto whiteRook = std::make_shared<kungfu::Piece>(kungfu::PieceType::Rook, kungfu::PlayerColor::White, kungfu::Position(0, 0));
    auto blackRook = std::make_shared<kungfu::Piece>(kungfu::PieceType::Rook, kungfu::PlayerColor::Black, kungfu::Position(0, 2));

    SECTION("Mid-route collision is detected when non-knights overlap in time on a square") {
        // m1 יוצא לדרך ב-t=0 (המקדים ביותר)
        kungfu::Motion m1(whiteRook, kungfu::Position(0, 0), kungfu::Position(0, 2), 0, 2000);
        // m2 יוצא לדרך ב-t=100 (המאוחר יותר)
        kungfu::Motion m2(blackRook, kungfu::Position(0, 2), kungfu::Position(0, 0), 100, 2000);

        std::vector<kungfu::Motion> motions = {m1, m2};
        auto collisions = kungfu::CollisionDetector::detectMidRouteCollisions(motions, config);

        REQUIRE(collisions.size() == 1);
        // לפי הכלל המקורי החדש: מי שיצא קודם (m1 / whiteRook) הוא ה-winner שישרוד
        REQUIRE(collisions[0].winner.piece() == whiteRook);
        // מי שיצא מאוחר (m2 / blackRook) הוא ה-loser שיושמד
        REQUIRE(collisions[0].loser.piece() == blackRook);
    }

    SECTION("Knights jumping past each other do not collide mid-route") {
        auto whiteKnight = std::make_shared<kungfu::Piece>(kungfu::PieceType::Knight, kungfu::PlayerColor::White, kungfu::Position(0, 0));
        auto blackKnight = std::make_shared<kungfu::Piece>(kungfu::PieceType::Knight, kungfu::PlayerColor::Black, kungfu::Position(2, 1));

        kungfu::Motion m1(whiteKnight, kungfu::Position(0, 0), kungfu::Position(2, 1), 0, 2000);
        kungfu::Motion m2(blackKnight, kungfu::Position(2, 1), kungfu::Position(0, 0), 0, 2000);

        std::vector<kungfu::Motion> motions = {m1, m2};
        auto collisions = kungfu::CollisionDetector::detectMidRouteCollisions(motions, config);

        REQUIRE(collisions.empty());
    }

    SECTION("Detect arrivals correctly filters completed motions") {
        kungfu::Motion m1(whiteRook, kungfu::Position(0, 0), kungfu::Position(0, 2), 0, 1000);
        kungfu::Motion m2(blackRook, kungfu::Position(0, 2), kungfu::Position(0, 0), 0, 2000);

        std::vector<kungfu::Motion> motions = {m1, m2};

        REQUIRE(kungfu::CollisionDetector::detectArrivals(motions, 500).empty());

        auto arrivalsAt1500 = kungfu::CollisionDetector::detectArrivals(motions, 1500);
        REQUIRE(arrivalsAt1500.size() == 1);
        REQUIRE(arrivalsAt1500[0].piece() == whiteRook);

        auto arrivalsAt2500 = kungfu::CollisionDetector::detectArrivals(motions, 2500);
        REQUIRE(arrivalsAt2500.size() == 2);
    }
}

TEST_CASE("CollisionResolver Rules Resolution", "[rules][collision]") {
    auto board = std::make_shared<kungfu::Board>(8, 8);
    kungfu::CooldownTracker tracker;
    kungfu::GameConfig config;
    config.cooldownDurationMs = 2000;

    kungfu::CollisionResolver resolver(board, tracker, config);
    std::vector<kungfu::ArrivalEvent> events;

    SECTION("Mid-route ENEMY collision captures the loser (latecomer) and clears its cooldown") {
        auto whiteRook = std::make_shared<kungfu::Piece>(kungfu::PieceType::Rook, kungfu::PlayerColor::White, kungfu::Position(0, 0));
        auto blackRook = std::make_shared<kungfu::Piece>(kungfu::PieceType::Rook, kungfu::PlayerColor::Black, kungfu::Position(0, 2));

        board->placePiece(whiteRook, kungfu::Position(0, 0));
        board->placePiece(blackRook, kungfu::Position(0, 2));

        // נדמה שהכלים התקדמו פיזית ונפגשו במשבצת האמצעית (0, 1)
        whiteRook->setPosition(kungfu::Position(0, 1));
        blackRook->setPosition(kungfu::Position(0, 1));

        // נגדיר זמן צינון למפסיד העתידי (loser / blackRook) כדי לוודא שהוא אכן יתנקה
        tracker.setCooldown(blackRook->id(), 5000);

        // winner (whiteRook) יצא ב-t=0. loser (blackRook) יצא מאוחר יותר ב-t=100.
        kungfu::Motion winner(whiteRook, kungfu::Position(0, 0), kungfu::Position(0, 2), 0, 1000);
        kungfu::Motion loser(blackRook, kungfu::Position(0, 2), kungfu::Position(0, 0), 100, 1000);

        resolver.resolveMidRouteCollision(winner, loser, 1000, events);

        // הכלי שהתחיל לנוע מאוחר יותר (loser / blackRook) מובס ומסומן כנלכד [1]
        REQUIRE(loser.piece()->state() == kungfu::PieceState::Captured);
        
        // הכלי שהתחיל לנוע ראשון (winner / whiteRook) שורד וממשיך [1]
        REQUIRE(winner.piece()->state() == kungfu::PieceState::Idle);

        // המפסיד מוסר מהלוח בנקודת המפגש הנוכחית שלו (0, 1) ולא מנקודת המוצא הישנה שלו (0, 2) [1]
        REQUIRE_FALSE(board->pieceAt(kungfu::Position(0, 1)).has_value());
        
        // זמן הצינון של הכלי שנלכד (loser) מנוקה מהזיכרון
        REQUIRE_FALSE(tracker.isOnCooldown(blackRook->id(), 3000));
        
        REQUIRE(events.size() == 1);
        // האירוע מדווח על השמדה בנקודת המפגש האמיתית באמצע המסלול [1]
        REQUIRE(events[0].to == kungfu::Position(0, 1));
    }

    SECTION("Mid-route FRIENDLY collision stops the later piece at the last vacant square") {
        auto whiteRook1 = std::make_shared<kungfu::Piece>(kungfu::PieceType::Rook, kungfu::PlayerColor::White, kungfu::Position(0, 0));
        auto whiteRook2 = std::make_shared<kungfu::Piece>(kungfu::PieceType::Rook, kungfu::PlayerColor::White, kungfu::Position(0, 4));

        board->placePiece(whiteRook1, kungfu::Position(0, 0));
        board->placePiece(whiteRook2, kungfu::Position(0, 4));

        kungfu::Motion firstArrived(whiteRook1, kungfu::Position(0, 0), kungfu::Position(0, 3), 0, 1000);
        kungfu::Motion secondArrived(whiteRook2, kungfu::Position(0, 4), kungfu::Position(0, 1), 100, 1000);

        resolver.resolveMidRouteCollision(firstArrived, secondArrived, 1000, events);

        REQUIRE(secondArrived.piece()->state() == kungfu::PieceState::Idle);
        REQUIRE(secondArrived.piece()->position() == kungfu::Position(0, 4));
        REQUIRE(events.size() == 1);
    }

    SECTION("Non-knight arrival on friendly piece triggers block and falls back correctly") {
        auto whiteRook = std::make_shared<kungfu::Piece>(kungfu::PieceType::Rook, kungfu::PlayerColor::White, kungfu::Position(0, 0));
        auto friendlyPawn = std::make_shared<kungfu::Piece>(kungfu::PieceType::Pawn, kungfu::PlayerColor::White, kungfu::Position(0, 2));

        board->placePiece(whiteRook, kungfu::Position(0, 0));
        board->placePiece(friendlyPawn, kungfu::Position(0, 2));

        kungfu::Motion motion(whiteRook, kungfu::Position(0, 0), kungfu::Position(0, 2), 0, 1000);

        bool success = resolver.resolveArrival(motion, 1000, events, nullptr);

        REQUIRE(success == true);
        REQUIRE(whiteRook->state() == kungfu::PieceState::Idle);
        REQUIRE(whiteRook->position() == kungfu::Position(0, 1));
        REQUIRE(board->pieceAt(kungfu::Position(0, 2)).value() == friendlyPawn);
    }

    SECTION("Knight landing on friendly piece captures it (Kung-Fu friendly fire)") {
        auto whiteKnight = std::make_shared<kungfu::Piece>(kungfu::PieceType::Knight, kungfu::PlayerColor::White, kungfu::Position(0, 0));
        auto friendlyPawn = std::make_shared<kungfu::Piece>(kungfu::PieceType::Pawn, kungfu::PlayerColor::White, kungfu::Position(2, 1));

        board->placePiece(whiteKnight, kungfu::Position(0, 0));
        board->placePiece(friendlyPawn, kungfu::Position(2, 1));

        kungfu::Motion motion(whiteKnight, kungfu::Position(0, 0), kungfu::Position(2, 1), 0, 1000);

        bool success = resolver.resolveArrival(motion, 1000, events, nullptr);

        REQUIRE(success == true);
        REQUIRE(whiteKnight->state() == kungfu::PieceState::Idle);
        REQUIRE(whiteKnight->position() == kungfu::Position(2, 1));
        REQUIRE(friendlyPawn->state() == kungfu::PieceState::Captured);
        REQUIRE(board->pieceAt(kungfu::Position(2, 1)).value() == whiteKnight);
    }

    SECTION("Normal arrival captures enemy piece and sets landing piece's cooldown") {
        auto whiteRook = std::make_shared<kungfu::Piece>(kungfu::PieceType::Rook, kungfu::PlayerColor::White, kungfu::Position(0, 0));
        auto enemyPawn = std::make_shared<kungfu::Piece>(kungfu::PieceType::Pawn, kungfu::PlayerColor::Black, kungfu::Position(0, 2));

        board->placePiece(whiteRook, kungfu::Position(0, 0));
        board->placePiece(enemyPawn, kungfu::Position(0, 2));

        kungfu::Motion motion(whiteRook, kungfu::Position(0, 0), kungfu::Position(0, 2), 0, 1000);

        bool success = resolver.resolveArrival(motion, 1000, events, nullptr);

        REQUIRE(success == true);
        REQUIRE(whiteRook->position() == kungfu::Position(0, 2));
        REQUIRE(enemyPawn->state() == kungfu::PieceState::Captured);
        REQUIRE(tracker.isOnCooldown(whiteRook->id(), 2999));
        REQUIRE_FALSE(tracker.isOnCooldown(whiteRook->id(), 3000));
    }

    SECTION("Non-knight blocked by friendly piece stops at the last vacant square on its path") {
        auto whiteRook = std::make_shared<kungfu::Piece>(kungfu::PieceType::Rook, kungfu::PlayerColor::White, kungfu::Position(-1, -1));
        auto friendlyPawn = std::make_shared<kungfu::Piece>(kungfu::PieceType::Pawn, kungfu::PlayerColor::White, kungfu::Position(0, 4));
        auto friendlyBishop = std::make_shared<kungfu::Piece>(kungfu::PieceType::Bishop, kungfu::PlayerColor::White, kungfu::Position(0, 3));

        board->placePiece(whiteRook, kungfu::Position(-1, -1));
        board->placePiece(friendlyPawn, kungfu::Position(0, 4));
        board->placePiece(friendlyBishop, kungfu::Position(0, 3));

        kungfu::Motion motion(whiteRook, kungfu::Position(0, 0), kungfu::Position(0, 4), 0, 2000);

        bool success = resolver.resolveArrival(motion, 2000, events, nullptr);

        REQUIRE(success == true);
        REQUIRE(whiteRook->state() == kungfu::PieceState::Idle);
        REQUIRE(whiteRook->position() == kungfu::Position(0, 2));
    }
}