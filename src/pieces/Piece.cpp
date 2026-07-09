#include "pieces/Piece.hpp"

namespace kungfu {

Piece::Piece(PieceType type, PlayerColor color, Position position)
    : type_(type), color_(color), position_(position), state_(PieceState::Idle) {}

}  // namespace kungfu
