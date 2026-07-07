#include "BoardParser.hpp"
#include <sstream>

ParseResult BoardParser::parse(std::istream& input, Board& outBoard) {
    outBoard.clear();
    std::string line;

    // קריאת שורת הכותרת "Board:"
    if (!std::getline(input, line)) {
        return ParseResult::Success;
    }

    while (std::getline(input, line)) {
        if (line == "Commands:") {
            break;
        }
        if (line.empty()) {
            continue;
        }

        std::stringstream ss(line);
        std::vector<std::string> row;
        std::string token;

        while (ss >> token) {
            if (!Board::isValidToken(token)) {
                return ParseResult::UnknownToken;
            }
            row.push_back(token);
        }

        if (!outBoard.isEmpty() && row.size() != outBoard.getWidth()) {
            return ParseResult::RowWidthMismatch;
        }

        outBoard.addRow(row);
    }

    return ParseResult::Success;
}