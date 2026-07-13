// src/board/Piece.cpp
#include "board/Piece.hpp"
#include <atomic>

namespace kungfu {

// הגדרת המשתנה הסטטי של המחלקה והקצאת זיכרון עבורו (מתחיל מ-1)
std::uint64_t Piece::nextId_ = 1;

Piece::Piece(PieceType type, PlayerColor color, Position position)
    : type_(type), color_(color), position_(position), state_(PieceState::Idle), id_(nextId_++) {}

}  // namespace kungfu