#pragma once

#include <string>
#include <memory>
#include <vector>
#include "engine/board/Board.hpp"

namespace kungfu {

class BoardParser {
public:
    // Parses a textual representation of a chess board into a Board object.
    // The input format is a multi-line string where each line represents a row of the board
    // In case of invalid input, the function will return nullptr (or throw std::invalid_argument).
    static std::shared_ptr<Board> parse(const std::string_view text);

private:
    static std::vector<std::string> splitLines(std::string_view text);
    static std::vector<std::string> tokenize(const std::string& line);
    static bool isValidToken(const std::string& token);
    static std::shared_ptr<Piece> createPiece(const std::string& token, const Position& pos);
};

}  // namespace kungfu