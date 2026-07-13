#pragma once

#include <string>
#include "board/Board.hpp"

namespace kungfu {

class BoardPrinter {
public:
    // מדפיס את המצב הלוגי הנוכחי של הלוח
    static std::string print(const Board& board);

private:
    static std::string getPieceToken(const ConstPiecePtr& piece);
};

}  // namespace kungfu