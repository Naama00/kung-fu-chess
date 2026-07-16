#pragma once

#include <string>
#include <memory>
#include <vector>
#include "engine/board/Board.hpp"

namespace kungfu {

class BoardParser {
public:
    // מפענח מחרוזת טקסט ומייצר אובייקט Board מוגדר דינמית.
    // במקרה של קלט לא תקין, הפונקציה תחזיר nullptr (או תזרוק std::invalid_argument).
    static std::shared_ptr<Board> parse(const std::string& text);

private:
    static std::vector<std::string> splitLines(const std::string& text);
    static std::vector<std::string> tokenize(const std::string& line);
    static bool isValidToken(const std::string& token);
    static std::shared_ptr<Piece> createPiece(const std::string& token, const Position& pos);
};

}  // namespace kungfu