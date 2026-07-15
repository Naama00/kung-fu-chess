#include "core/view/SnapshotBuilder.hpp"
#include <algorithm>
#include <cmath>
#include <memory>
#include <unordered_set>
#include <vector>

namespace kungfu
{
    namespace view
    {
        namespace
        {
            struct PixelPos { float x; float y; };

            // תיקון: לוגיקת האינטרפולציה (פרבולה עבור Airborne, smoothstep עבור
            // Moving) הייתה משוכפלת כמעט מילה במילה בין שתי הלולאות למטה.
            // ריכזתי אותה כאן כדי שכל שינוי עתידי בעקומת האנימציה יתבצע במקום אחד.
            PixelPos computeAnimatedPixel(PieceState state,
                                           const Position &from,
                                           const Position &to,
                                           int startTime,
                                           int arrivalTime,
                                           int currentTimeMs,
                                           float cellSize)
            {
                float startX = from.col() * cellSize;
                float startY = from.row() * cellSize;
                float endX = to.col() * cellSize;
                float endY = to.row() * cellSize;

                int duration = arrivalTime - startTime;
                float t = 1.0f;
                if (duration > 0)
                {
                    int elapsed = currentTimeMs - startTime;
                    t = static_cast<float>(elapsed) / static_cast<float>(duration);
                    t = std::max(0.0f, std::min(t, 1.0f));
                }

                PixelPos result{};
                if (state == PieceState::Airborne)
                {
                    result.x = startX + t * (endX - startX);
                    result.y = startY + t * (endY - startY);

                    float maxHeight = cellSize * 1.5f;
                    result.y -= maxHeight * 4.0f * t * (1.0f - t);
                }
                else
                {
                    float smoothed_t = t * t * (3.0f - 2.0f * t);
                    result.x = startX + smoothed_t * (endX - startX);
                    result.y = startY + smoothed_t * (endY - startY);
                }
                return result;
            }
        } // anonymous namespace

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

            // תיקון: קריאה יחידה ל-board.pieces() (ולא הסתמכות מרומזת על
            // ה-range-for לקרוא לה שוב בהמשך), ו-reserve כדי למנוע reallocations
            // חוזרות של snap.pieces תוך כדי מילוי.
            const auto &boardPieces = board.pieces();
            snap.pieces.reserve(boardPieces.size());

            // תיקון: unordered_set במקום vector+std::find - חיפוש O(1) במקום
            // O(n) על כל כלי (משפיע בעיקר כשיש הרבה כלים/אנימציות במקביל).
            std::unordered_set<std::uint64_t> drawnPieceIds;
            drawnPieceIds.reserve(boardPieces.size());

            // 1. ציור כלים שנמצאים פיזית על הלוח (כולל כלים מחליקים רגילים)
            for (const auto &piece : boardPieces)
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

                // תיקון: cooldownProgress מחושב פעם אחת כאן, כדי שה-View (למשל
                // ChessGameScreen) לא יצטרך לחפש שוב את הכלי בלוח בשביל זה.
                pSnap.cooldownProgress = arbiter.getCooldownProgress(
                    std::const_pointer_cast<Piece>(piece), currentTimeMs);

                pSnap.pixelX = piece->position().col() * cellSize;
                pSnap.pixelY = piece->position().row() * cellSize;

                if (piece->state() == PieceState::Moving || piece->state() == PieceState::Airborne)
                {
                    auto motionOpt = arbiter.getMotionForPiece(piece);
                    if (motionOpt.has_value())
                    {
                        const auto &motion = motionOpt.value();
                        int duration = motion.arrivalTime() - motion.startTime();
                        if (duration > 0)
                        {
                            auto px = computeAnimatedPixel(
                                piece->state(), motion.from(), motion.to(),
                                motion.startTime(), motion.arrivalTime(),
                                currentTimeMs, cellSize);
                            pSnap.pixelX = px.x;
                            pSnap.pixelY = px.y;
                        }
                    }
                }

                snap.pieces.push_back(pSnap);
                drawnPieceIds.insert(piece->id());
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
                if (drawnPieceIds.find(piece->id()) != drawnPieceIds.end())
                {
                    continue;
                }

                PieceSnapshot pSnap;
                pSnap.type = piece->type();
                pSnap.color = piece->color();
                pSnap.logicalPosition = piece->position();
                pSnap.state = piece->state();
                pSnap.cooldownProgress = arbiter.getCooldownProgress(
                    std::const_pointer_cast<Piece>(piece), currentTimeMs);

                auto px = computeAnimatedPixel(
                    piece->state(), motion.from(), motion.to(),
                    motion.startTime(), motion.arrivalTime(),
                    currentTimeMs, cellSize);
                pSnap.pixelX = px.x;
                pSnap.pixelY = px.y;

                snap.pieces.push_back(pSnap);
                drawnPieceIds.insert(piece->id());
            }

            return snap;
        }

    } // namespace view
} // namespace kungfu