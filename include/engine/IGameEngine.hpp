// include/engine/IGameEngine.hpp
#pragma once

#include <string>
#include <optional>
#include "common/Position.hpp"
#include "common/Enums.hpp"
#include "common/MoveResult.hpp"

namespace kungfu {

class IGameEngine {
public:
    virtual ~IGameEngine() = default;

    virtual MoveResult requestMove(const Position& from, const Position& to) = 0;
    virtual bool hasPieceAt(const Position& pos) const = 0;
    virtual int getBoardRows() const = 0;
    virtual int getBoardCols() const = 0;
    virtual std::optional<PlayerColor> getPieceColorAt(const Position& pos) const = 0;
};

}  // namespace kungfu