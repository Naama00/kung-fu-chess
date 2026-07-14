#include "core/view/SnapshotBuilder.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

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

            // שמירת ה-IDs של הכלים שצויירו כדי למנוע כפילויות
            std::vector<std::uint64_t> drawnPieceIds;

            // 1. ציור כלים שנמצאים פיזית על הלוח (כולל כלים מחליקים רגילים)
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

                            if (piece->state() == PieceState::Airborne)
                            {
                                pSnap.pixelX = startX + t * (endX - startX);
                                pSnap.pixelY = startY + t * (endY - startY);

                                float maxHeight = cellSize * 1.5f;
                                float heightOffset = maxHeight * 4.0f * t * (1.0f - t);
                                pSnap.pixelY -= heightOffset;
                            }
                            else
                            {
                                float smoothed_t = t * t * (3.0f - 2.0f * t);
                                pSnap.pixelX = startX + smoothed_t * (endX - startX);
                                pSnap.pixelY = startY + smoothed_t * (endY - startY);
                            }
                        }
                    }
                }

                snap.pieces.push_back(pSnap);
                drawnPieceIds.push_back(piece->id());
            }

            // 2. זיהוי וציור כלים שנמצאים במצב מעוף (Airborne) והוסרו זמנית מהלוח
            for (const auto &motion : arbiter.activeMotions())
            {
                auto piece = motion.piece();
                if (!piece || piece->state() == PieceState::Captured)
                {
                    continue;
                }

                // אם הכלי כבר צוייר בלולאה הראשונה, נדלג עליו
                if (std::find(drawnPieceIds.begin(), drawnPieceIds.end(), piece->id()) != drawnPieceIds.end())
                {
                    continue;
                }

                PieceSnapshot pSnap;
                pSnap.type = piece->type();
                pSnap.color = piece->color();
                pSnap.logicalPosition = piece->position();
                pSnap.state = piece->state();

                float startX = motion.from().col() * cellSize;
                float startY = motion.from().row() * cellSize;
                float endX = motion.to().col() * cellSize;
                float endY = motion.to().row() * cellSize;

                int duration = motion.arrivalTime() - motion.startTime();
                float t = 1.0f;
                if (duration > 0)
                {
                    int elapsed = currentTimeMs - motion.startTime();
                    t = static_cast<float>(elapsed) / static_cast<float>(duration);
                    t = std::max(0.0f, std::min(t, 1.0f));
                }

                if (piece->state() == PieceState::Airborne)
                {
                    pSnap.pixelX = startX + t * (endX - startX);
                    pSnap.pixelY = startY + t * (endY - startY);

                    // הוספת היסט אנכי מבוסס פרבולה
                    float maxHeight = cellSize * 1.5f;
                    float heightOffset = maxHeight * 4.0f * t * (1.0f - t);
                    pSnap.pixelY -= heightOffset;
                }
                else
                {
                    float smoothed_t = t * t * (3.0f - 2.0f * t);
                    pSnap.pixelX = startX + smoothed_t * (endX - startX);
                    pSnap.pixelY = startY + smoothed_t * (endY - startY);
                }

                snap.pieces.push_back(pSnap);
                drawnPieceIds.push_back(piece->id());
            }

            return snap;
        }

    } // namespace view
} // namespace kungfu