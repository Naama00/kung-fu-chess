#pragma once

#include <optional>
#include "engine/common/Enums.hpp"

namespace kungfu {

// מקור אמת יחיד למיפוי בין תו הטוקן הטקסטואלי לבין PieceType.
// משמש הן ב-BoardParser (פענוח) והן ב-BoardPrinter (הדפסה) כדי למנוע כפילות.
class PieceTokenCodec {
public:
    static std::optional<PieceType> fromChar(char c) noexcept {
        switch (c) {
            case 'K': return PieceType::King;
            case 'Q': return PieceType::Queen;
            case 'R': return PieceType::Rook;
            case 'B': return PieceType::Bishop;
            case 'N': return PieceType::Knight;
            case 'P': return PieceType::Pawn;
            default:  return std::nullopt;
        }
    }

    static char toChar(PieceType type) noexcept {
        switch (type) {
            case PieceType::King:   return 'K';
            case PieceType::Queen:  return 'Q';
            case PieceType::Rook:   return 'R';
            case PieceType::Bishop: return 'B';
            case PieceType::Knight: return 'N';
            case PieceType::Pawn:   return 'P';
        }
        return '?';
    }
};

}  // namespace kungfu