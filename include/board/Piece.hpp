#pragma once

#include <cstdint>
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
    bool hasMoved() const { return hasMoved_; }

    // מזהה יציב וייחודי לכל אובייקט Piece, לאורך כל חייו - בלתי תלוי בכתובת הזיכרון שלו.
    // משמש כמפתח בטוח למבני נתונים חיצוניים (למשל cooldowns ב-RealTimeArbiter),
    // כדי למנוע התנגשות עם כלים חדשים שנוצרים לאחר שחרור זיכרון של כלי קודם.
    std::uint64_t id() const noexcept { return id_; }

    void setPosition(const Position& position) { position_ = position; }
    void setState(PieceState state) { state_ = state; }
    void markMoved() { hasMoved_ = true; }

    bool isMovable() const { return state_ != PieceState::Captured; }

protected:
    PieceType type_;
    PlayerColor color_;
    Position position_;
    PieceState state_ = PieceState::Idle;
    bool hasMoved_ = false;

private:
    static std::uint64_t nextId_;
    std::uint64_t id_;
};

using PiecePtr = std::shared_ptr<Piece>;
using ConstPiecePtr = std::shared_ptr<const Piece>;

}  // namespace kungfu