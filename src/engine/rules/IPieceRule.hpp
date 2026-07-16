#pragma once

#include <vector>
#include "engine/common/Position.hpp"
#include "engine/board/IBoard.hpp"
#include "engine/board/Piece.hpp"

namespace kungfu {

class IPieceRule {
public:
    virtual ~IPieceRule() = default;

    // מחשבת ומחזירה את כל קואורדינטות היעד הגיאומטריות האפשריות עבור הכלי הנתון במצב הלוח הנוכחי.
    virtual std::vector<Position> getLegalDestinations(const IBoard& board, const Piece& piece) const = 0;
};

}  // namespace kungfu