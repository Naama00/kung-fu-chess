// src/board/Piece.cpp
#include "board/Piece.hpp"
#include <atomic>

namespace kungfu {

static std::atomic<std::uint64_t> nextId_{1};

Piece::Piece(PieceType type, PlayerColor color, Position position)
    : type_(type), color_(color), position_(position), state_(PieceState::Idle), id_(nextId_++) {}

}  // namespace kungfu