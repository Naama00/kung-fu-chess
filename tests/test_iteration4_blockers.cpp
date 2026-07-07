#include "../core/include/Board.hpp"
#include "../core/include/PathValidator.hpp"
#include <cassert>
#include <iostream>

void test_knight_jumps_over_blockers() {
    Board board;
    // יצירת לוח זמני עם חסימה בדרך של הפרש
    board.addRow({ "wN", ".", "." });
    board.addRow({ ".", "bP", "." });
    board.addRow({ ".", ".", "." });

    // פרש ב-(0,0) רוצה לנוע ל-(2,1). יש כלי ב-(1,1) אבל פרש מדלג
    assert(PathValidator::isPathClear(board, 0, 0, 2, 1) == true);
}

void test_rook_blocked() {
    Board board;
    board.addRow({ "wR", "wP", "." }); // צריח חסום על ידי רגלי חברותי משמאל
    assert(PathValidator::isPathClear(board, 0, 0, 0, 2) == false);
}

void test_friendly_capture_denied() {
    Board board;
    board.addRow({ "wR", "wK" }); // צריח ומלך מאותו צבע
    assert(PathValidator::isValidTarget(board, 0, 0, 0, 1) == false);
}

void run_blockers_tests() {
    test_knight_jumps_over_blockers();
    test_rook_blocked();
    test_friendly_capture_denied();
    std::cout << "[✓] All blockers and capture tests passed.\n";
}