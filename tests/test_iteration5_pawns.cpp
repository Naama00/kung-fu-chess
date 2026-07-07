#include "../core/include/Board.hpp"
#include "../core/include/PathValidator.hpp"
#include <cassert>
#include <iostream>

void test_pawn_forward_move() {
    Board board;
    board.addRow({ "wP", "." });
    board.addRow({ ".", "." });
    
    // לבן מנסה לעלות למעלה (ממשבצת 1 ל-0) - חוקי
    assert(Board::isLegalMovePattern("wP", 1, 0, 0, 0) == true);
    // לבן מנסה לנוע 2 צעדים - לא חוקי
    assert(Board::isLegalMovePattern("wP", 2, 0, 0, 0) == false);
}

void test_pawn_capture_rules() {
    Board board;
    board.addRow({ "wP", "bB" });
    board.addRow({ ".", "." });

    // רגלי לבן ב-(1,0) מנסה להכות באלכסון את הרץ השחור ב-(0,1) - חוקי
    assert(Board::isLegalMovePattern("wP", 1, 0, 0, 1) == true);
    assert(PathValidator::isValidTarget(board, 1, 0, 0, 1) == true);

    // רגלי לבן מנסה לנוע ישר אל תוך כלי חסום - isValidTarget צריך להחזיר false
    Board blockedBoard;
    blockedBoard.addRow({ "bB" });
    blockedBoard.addRow({ "wP" });
    assert(PathValidator::isValidTarget(blockedBoard, 1, 0, 0, 0) == false);
}

void run_pawn_tests() {
    test_pawn_forward_move();
    test_pawn_capture_rules();
    std::cout << "[✓] All pawn movement and capture tests passed.\n";
}