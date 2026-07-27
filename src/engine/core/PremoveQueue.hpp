// engine/core/PremoveQueue.hpp
#pragma once

#include <vector>
#include <functional>
#include <unordered_map>
#include "engine/board/Piece.hpp"
#include "engine/common/Position.hpp"
#include "engine/core/IGameEngine.hpp"
#include "engine/snapshot/MatchStateSnapshot.hpp"

namespace kungfu {

using PieceBusyPredicate = std::function<bool(const PiecePtr&)>;
using MoveExecutor = std::function<MoveResult(const Position& from, const Position& to)>;

class PremoveQueue {
public:
    // Registers a new premove, or updates an existing premove for the same piece
    void registerOrUpdate(const PiecePtr& piece, const Position& to);

    // Cancels an existing premove for a given piece (if one exists)
    void cancel(const PiecePtr& piece) noexcept;

    // Replaces a piece reference in the queue (e.g., during pawn promotion)
    void replacePiece(const PiecePtr& oldPiece, const PiecePtr& newPiece) noexcept;

    // Processes premoves that are ready to execute
    void processReady(const PieceBusyPredicate& isBusy, const MoveExecutor& execute);

    bool empty() const noexcept { return entries_.empty(); }
    size_t size() const noexcept { return entries_.size(); }
    const std::vector<std::pair<PiecePtr, Position>>& entries() const noexcept { return entries_; }

    // Exports the current queue of pending premoves for live state snapshots
    std::vector<PremoveSnapshot> exportSnapshot() const;

    // Restores pending premoves from snapshot data using piece ID mapping
    void restoreSnapshot(const std::vector<PremoveSnapshot>& snapshots,
                         const std::unordered_map<std::uint64_t, PiecePtr>& pieceMap) noexcept;

    // Clears all pending premoves from memory
    void clear() noexcept { entries_.clear(); }

private:
    std::vector<std::pair<PiecePtr, Position>> entries_;
};

}  // namespace kungfu