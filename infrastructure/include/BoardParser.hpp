#ifndef BOARD_PARSER_HPP
#define BOARD_PARSER_HPP

#include "Board.hpp"
#include <istream>

enum class ParseResult {
    Success,
    UnknownToken,
    RowWidthMismatch
};

class BoardParser {
public:
    // הפונקציה מקבלת stream כלשהו (זה יכול להיות cin או קובץ בטסטים שלך)
    static ParseResult parse(std::istream& input, Board& outBoard);
};

#endif