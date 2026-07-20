#include <catch2/catch_test_macros.hpp>
#include "engine/io/BoardParser.hpp"
#include "engine/io/BoardPrinter.hpp"

TEST_CASE("Board Parser and Printer Integration", "[io]") {
    
    SECTION("Parse and Print a standard rectangular board") {
        std::string inputBoard = 
            "wK . . bR\n"
            ". wP . .\n"
            ". . bK .\n";
            
        auto board = kungfu::BoardParser::parse(inputBoard);
        REQUIRE(board != nullptr);
        REQUIRE(board->rows() == 3);
        REQUIRE(board->cols() == 4);
        
        std::string printed = kungfu::BoardPrinter::print(*board);
        REQUIRE(printed == inputBoard);
    }

    SECTION("Parser rejects non-rectangular boards") {
        std::string invalidBoard = 
            "wK . .\n"
            ". wP . . .\n"  // line too long
            ". . bK\n";
            
        auto board = kungfu::BoardParser::parse(invalidBoard);
        REQUIRE(board == nullptr);
    }

    SECTION("Parser rejects invalid tokens") {
        std::string invalidTokenBoard = 
            "wK . .\n"
            ". wX .\n"  // X is not a legal chess piece
            ". . bK\n";
            
        auto board = kungfu::BoardParser::parse(invalidTokenBoard);
        REQUIRE(board == nullptr);
    }
}