 #include "../core/include/Board.hpp"
#include <cassert>
#include <iostream>

void test_rook_movement() {
    assert(Board::isLegalMovePattern("wR", 4, 4, 4, 7) == true);
    assert(Board::isLegalMovePattern("wR", 4, 4, 1, 4) == true);
    assert(Board::isLegalMovePattern("wR", 4, 4, 5, 5) == false);
}

void test_bishop_movement() {
    assert(Board::isLegalMovePattern("bB", 3, 3, 5, 5) == true);
    assert(Board::isLegalMovePattern("bB", 3, 3, 1, 5) == true);
    assert(Board::isLegalMovePattern("bB", 3, 3, 3, 5) == false);
}

void test_knight_movement() {
    assert(Board::isLegalMovePattern("wN", 4, 4, 6, 5) == true);
    assert(Board::isLegalMovePattern("wN", 4, 4, 5, 6) == true);
    assert(Board::isLegalMovePattern("wN", 4, 4, 4, 6) == false);
}

void test_king_movement() {
    assert(Board::isLegalMovePattern("wK", 4, 4, 5, 5) == true);
    assert(Board::isLegalMovePattern("wK", 4, 4, 4, 5) == true);
    assert(Board::isLegalMovePattern("wK", 4, 4, 4, 6) == false);
}

void run_pieces_tests() {
    test_rook_movement();
    test_bishop_movement();
    test_knight_movement();
    test_king_movement();
    std::cout << "[✓] All piece movement tests passed.\n";
}