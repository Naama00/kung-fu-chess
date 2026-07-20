#pragma once

#include <vector>
#include <functional>
#include "engine/board/Piece.hpp"
#include "engine/common/Position.hpp"
#include "engine/core/IGameEngine.hpp"

namespace kungfu {

using PieceBusyPredicate = std::function<bool(const PiecePtr&)>;
using MoveExecutor = std::function<MoveResult(const Position& from, const Position& to)>;

class PremoveQueue {
public:
    // Registers a new premove, or updates an existing premove for the same piece.
    // Only the destination (to) is stored — execution always uses the piece's current live position as from,
    // because the piece may move in the meantime (for example after its previous move has landed).
    void registerOrUpdate(const PiecePtr& piece, const Position& to);
    // Cancels an existing premove for a given piece (if one exists).
    // Called when the player makes an illegal move — intended to override and cancel the premove.
    void cancel(const PiecePtr& piece) noexcept;
    void replacePiece(const PiecePtr& oldPiece, const PiecePtr& newPiece) noexcept;
    void processReady(const PieceBusyPredicate& isBusy, const MoveExecutor& execute);
    bool empty() const noexcept { return entries_.empty(); }
    size_t size() const noexcept { return entries_.size(); }
    const std::vector<std::pair<PiecePtr, Position>>& entries() const noexcept { return entries_; }
private:
    std::vector<std::pair<PiecePtr, Position>> entries_;
};

}  // namespace kungfu