#pragma once

#include <memory>
#include "common/Enums.hpp"
#include "common/Position.hpp"

namespace kungfu {

class Piece {
public:
    Piece(PieceType type, PlayerColor color, Position position);
    virtual ~Piece() = default;

    PieceType type() const { return type_; }
    PlayerColor color() const { return color_; }
    Position position() const { return position_; }
    PieceState state() const { return state_; }

    void setPosition(const Position& position) { position_ = position; }
    void setState(PieceState state) { state_ = state; }

    bool isMovable() const { return true; }

protected:
    PieceType type_;
    PlayerColor color_;
    Position position_;
    PieceState state_ = PieceState::Idle;
};

using PiecePtr = std::shared_ptr<Piece>;

}  // namespace kungfu