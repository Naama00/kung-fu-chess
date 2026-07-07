#include "../infrastructure/include/BoardParser.hpp"
#include "../infrastructure/include/BoardFormatter.hpp"
#include <cassert>
#include <sstream>
#include <iostream>

void test_valid_parsing() {
    std::stringstream input("Board:\nwR . wB\n. bK .\nCommands:\n");
    Board board;
    ParseResult result = BoardParser::parse(input, board);
    assert(result == ParseResult::Success);
    assert(board.getWidth() == 3);
}

void test_parser_errors() {
    std::stringstream input("Board:\nwR . INVALID\nCommands:\n");
    Board board;
    ParseResult result = BoardParser::parse(input, board);
    assert(result == ParseResult::UnknownToken);
}

void run_parser_tests() {
    test_valid_parsing();
    test_parser_errors();
    std::cout << "[✓] All parser and I/O tests passed.\n";
}