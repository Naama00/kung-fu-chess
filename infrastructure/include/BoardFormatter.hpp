#ifndef BOARD_FORMATTER_HPP
#define BOARD_FORMATTER_HPP

#include "Board.hpp"
#include <ostream>

class BoardFormatter {
public:
    static void print(std::ostream& output, const Board& board);
};
#endif