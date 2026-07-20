// engine/common/PieceValues.hpp
#pragma once

#include "engine/common/Enums.hpp"

namespace kungfu {

class PieceValues {
public:
    // Traditional values (for example, for displaying a material balance in the UI)
    static int getStandardValue(PieceType type) noexcept {
        switch (type) {
            case PieceType::Pawn:   return 1;
            case PieceType::Knight: return 3;
            case PieceType::Bishop: return 3;
            case PieceType::Rook:   return 5;
            case PieceType::Queen:  return 9;
            case PieceType::King:   return 0; // the king is not counted in the standard material scoring calculation (maximum 39)
            default:                return 0;
        }
    }

    // Values in centipawns (for engine, AI, and tactical evaluation)
    static int getCentipawnValue(PieceType type) noexcept {
        switch (type) {
            case PieceType::Pawn:   return 100;
            case PieceType::Knight: return 300;
            case PieceType::Bishop: return 300;
            case PieceType::Rook:   return 500;
            case PieceType::Queen:  return 900;
            case PieceType::King:   return 10000;
            default:                return 0;
        }
    }
};

} // namespace kungfu