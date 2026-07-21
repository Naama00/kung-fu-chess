// engine/snapshot/SnapshotBuilder.cpp
#include "engine/snapshot/SnapshotBuilder.hpp"
#include <algorithm>
#include <memory>
#include <unordered_set>
#include <vector>

namespace kungfu {
namespace view {

namespace {

PieceSnapshot createPieceSnapshot(
    const std::shared_ptr<const Piece>& piece,
    const RealTimeArbiter& arbiter,
    int currentTimeMs) 
{
    PieceSnapshot snapshot;
    snapshot.id = piece->id();
    snapshot.type = piece->type();
    snapshot.color = piece->color();
    snapshot.logicalPosition = piece->position();
    snapshot.state = piece->state();
    snapshot.hasMoved = piece->hasMoved();

    snapshot.cooldownProgress = arbiter.getCooldownProgress(
        std::const_pointer_cast<Piece>(piece),
        currentTimeMs
    );

    // Extract pending motion characteristics from the arbiter
    auto motionOpt = arbiter.getMotionForPiece(piece);
    if (motionOpt.has_value()) {
        const auto& motion = motionOpt.value();
        snapshot.isMoving = true;
        snapshot.transitFrom = motion.from();
        snapshot.transitTo = motion.to();
        snapshot.startTimeMs = motion.startTime();
        snapshot.arrivalTimeMs = motion.arrivalTime();
    }

    return snapshot;
}

} // anonymous namespace

GameSnapshot SnapshotBuilder::build(
    const IBoard& board,
    const RealTimeArbiter& arbiter,
    int currentTimeMs,
    bool gameOver,
    std::optional<Position> selectedCell) noexcept 
{
    GameSnapshot snap;
    snap.boardCols = board.cols();
    snap.boardRows = board.rows();
    snap.isGameOver = gameOver;
    snap.selectedCell = selectedCell;

    const auto& boardPieces = board.pieces();
    snap.pieces.reserve(boardPieces.size());

    std::unordered_set<std::uint64_t> drawnPieceIds;
    drawnPieceIds.reserve(boardPieces.size());

    // 1. Process pieces that are physically positioned on the board matrix
    for (const auto& piece : boardPieces) {
        if (!piece || piece->state() == PieceState::Captured) {
            continue;
        }

        PieceSnapshot pSnap = createPieceSnapshot(piece, arbiter, currentTimeMs);
        snap.pieces.push_back(pSnap);
        drawnPieceIds.insert(piece->id());
    }

    // 2. Process pieces suspended mid-motion (not yet landed or registered on the board grid)
    for (const auto& motion : arbiter.activeMotions()) {
        auto piece = motion.piece();
        if (!piece || piece->state() == PieceState::Captured) {
            continue;
        }

        if (drawnPieceIds.find(piece->id()) != drawnPieceIds.end()) {
            continue;
        }

        PieceSnapshot pSnap = createPieceSnapshot(piece, arbiter, currentTimeMs);
        snap.pieces.push_back(pSnap);
        drawnPieceIds.insert(piece->id());
    }

    return snap;
}

} // namespace view
} // namespace kungfu