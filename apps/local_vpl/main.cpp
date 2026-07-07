#include "BoardParser.hpp"
#include "BoardFormatter.hpp"
#include "GameEngine.hpp"
#include <iostream>
#include <string>
#include <sstream>

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Board board;
    ParseResult result = BoardParser::parse(std::cin, board);

    if (result == ParseResult::UnknownToken) {
        std::cout << "ERROR UNKNOWN_TOKEN\n";
        return 0;
    } else if (result == ParseResult::RowWidthMismatch) {
        std::cout << "ERROR ROW_WIDTH_MISMATCH\n";
        return 0;
    }

    GameEngine engine;
    engine.initBoard(board);

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string command;
        ss >> command;

        if (command == "click") {
            int x, y;
            if (ss >> x >> y) {
                engine.handleClick(x, y);
            }
        } 
        else if (command == "wait") {
            long long ms;
            if (ss >> ms) {
                engine.advanceTime(ms);
            }
        } 
        else if (command == "print" && ss >> command && command == "board") {
            BoardFormatter::print(std::cout, engine.getRenderedBoard());
        }   
    }

    return 0;
}