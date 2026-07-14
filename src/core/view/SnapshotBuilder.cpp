#include "core/view/SnapshotBuilder.hpp"
#include <algorithm>

namespace kungfu
{
    namespace view
    {

        GameSnapshot SnapshotBuilder::build(
            const IBoard &board,
            const RealTimeArbiter &arbiter,
            int currentTimeMs,
            bool gameOver,
            std::optional<Position> selectedCell,
            float cellSize) noexcept
        {
            GameSnapshot snap;
            snap.boardCols = board.cols();
            snap.boardRows = board.rows();
            snap.isGameOver = gameOver;
            snap.selectedCell = selectedCell;

            for (const auto &piece : board.pieces())
            {
                if (!piece || piece->state() == PieceState::Captured)
                {
                    continue;
                }

                PieceSnapshot pSnap;
                pSnap.type = piece->type();
                pSnap.color = piece->color();
                pSnap.logicalPosition = piece->position();
                pSnap.state = piece->state();

                float currentX = piece->position().col() * cellSize;
                float currentY = piece->position().row() * cellSize;

                pSnap.pixelX = currentX;
                pSnap.pixelY = currentY;

                if (piece->state() == PieceState::Moving || piece->state() == PieceState::Airborne)
                {
                    auto motionOpt = arbiter.getMotionForPiece(piece);
                    if (motionOpt.has_value())
                    {
                        const auto &motion = motionOpt.value();
                        float startX = motion.from().col() * cellSize;
                        float startY = motion.from().row() * cellSize;
                        float endX = motion.to().col() * cellSize;
                        float endY = motion.to().row() * cellSize;

                        int duration = motion.arrivalTime() - motion.startTime();
                        if (duration > 0)
                        {
                            int elapsed = currentTimeMs - motion.startTime();
                            float t = static_cast<float>(elapsed) / static_cast<float>(duration);
                            t = std::max(0.0f, std::min(t, 1.0f));

                            pSnap.pixelX = startX + t * (endX - startX);
                            pSnap.pixelY = startY + t * (endY - startY);
                        }
                    }
                }

                snap.pieces.push_back(pSnap);
            }

            return snap;
        }

    } // namespace view
} // namespace kungfu