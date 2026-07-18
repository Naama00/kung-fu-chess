#include "engine/rules/CollisionResolver.hpp"
#include "engine/realtime/CollisionDetector.hpp"
#include <cmath>
#include <algorithm>

namespace kungfu
{

    // הצהרה קדימה — מוגדרת ב-CollisionDetector.cpp
    std::vector<Position> getDetailedPath(const Position &from, const Position &to);

    // מוצא את המשבצת האחרונה הפנויה בנתיב מ-from לכיוון to (הליכה קדימה).
    Position findLastVacantPositionOnPath(
        const Position &from,
        const Position &to,
        const std::shared_ptr<IBoard> &board,
        const Position *blockedPos = nullptr) noexcept
    {
        int dr = to.row() - from.row();
        int dc = to.col() - from.col();

        if (dr == 0 && dc == 0)
        {
            return from;
        }

        int stepR = (dr > 0) ? 1 : ((dr < 0) ? -1 : 0);
        int stepC = (dc > 0) ? 1 : ((dc < 0) ? -1 : 0);

        Position last = from;
        int curR = from.row() + stepR;
        int curC = from.col() + stepC;

        while (true)
        {
            Position cur(curR, curC);

            auto pieceOpt = board->pieceAt(cur);
            bool occupiedByStationary = pieceOpt.has_value() &&
                                        pieceOpt.value() &&
                                        pieceOpt.value()->state() != PieceState::Airborne &&
                                        pieceOpt.value()->state() != PieceState::Moving;

            bool occupiedByBlocked = (blockedPos != nullptr && cur == *blockedPos);

            if (occupiedByStationary || occupiedByBlocked)
            {
                break;
            }

            last = cur;

            if (cur == to)
                break;
            curR += stepR;
            curC += stepC;
        }

        return last;
    }

    CollisionResolver::CollisionResolver(
        std::shared_ptr<IBoard> board,
        CooldownTracker &cooldownTracker,
        const GameConfig &config) noexcept
        : board_(std::move(board)), cooldownTracker_(cooldownTracker), config_(config) {}

    void CollisionResolver::resolveMidRouteCollision(
        const Motion &winner,
        const Motion &loser,
        int currentTimeMs,
        std::vector<ArrivalEvent> &events) noexcept
    {
        if (winner.piece()->color() != loser.piece()->color())
        {
            Position collisionPos = loser.piece()->position();

            // המפסיד (loser) נלכד, מסומן כ-Captured ומוסר מהלוח
            loser.piece()->setState(PieceState::Captured);
            cooldownTracker_.clear(loser.piece()->id());
            board_->removePiece(loser.piece());

            // המנצח (winner) שורד וממשיך בדרכו
            bool capturedKing = (loser.piece()->type() == PieceType::King);
            events.push_back({loser.from(), collisionPos, loser.piece(), capturedKing, false, true});
        }
        else
        {
            Position winnerDest = winner.to();
            Position stopPos = findLastVacantPositionOnPath(loser.from(), loser.to(), board_, &winnerDest);

            loser.piece()->setState(PieceState::Idle);
            loser.piece()->setPosition(stopPos);

            cooldownTracker_.setCooldown(loser.piece()->id(), currentTimeMs + config_.cooldownDurationMs);
            events.push_back({loser.from(), stopPos, loser.piece(), false, true, false});
        }
    }

    bool CollisionResolver::resolveArrival(
        const Motion &motion,
        int currentTimeMs,
        std::vector<ArrivalEvent> &events,
        const PromotionHandler &promoteCallback) noexcept
    {
        Position from = motion.from();
        Position to = motion.to();
        auto piece = motion.piece();

        // טיפול בנחיתת קפיצה עצמית (from == to)
        if (from == to)
        {
            auto landingTarget = board_->pieceAt(to);

            if (landingTarget.has_value() && landingTarget.value() &&
                (landingTarget.value()->state() == PieceState::Moving ||
                 landingTarget.value()->state() == PieceState::Airborne))
            {
                landingTarget = std::nullopt;
            }

            if (!landingTarget.has_value())
            {
                piece->setState(PieceState::Idle);
                piece->setPosition(to);
                board_->placePiece(piece, to);
                cooldownTracker_.setCooldown(piece->id(), motion.arrivalTime() + config_.cooldownDurationMs);
                events.push_back({from, to, piece, false, false, false});
                return true;
            }

            auto targetPiece = landingTarget.value();
            
            // פרש מבצע Self-Kill (לכידה ידידותית) גם בנחיתה מקפיצה עצמית
            if (targetPiece->color() != piece->color() || piece->type() == PieceType::Knight)
            {
                bool capturedKing = (targetPiece->type() == PieceType::King);
                targetPiece->setState(PieceState::Captured);
                cooldownTracker_.clear(targetPiece->id());
                board_->removePiece(targetPiece);

                piece->setState(PieceState::Idle);
                piece->setPosition(to);
                board_->placePiece(piece, to);
                piece->markMoved();
                cooldownTracker_.setCooldown(piece->id(), motion.arrivalTime() + config_.cooldownDurationMs);
                events.push_back({from, to, piece, capturedKing, false, true});
                return true;
            }
            else
            {
                Position finalDestination = from;
                for (int rOffset = -1; rOffset <= 1; ++rOffset)
                {
                    for (int cOffset = -1; cOffset <= 1; ++cOffset)
                    {
                        if (rOffset == 0 && cOffset == 0)
                            continue;
                        Position adj(from.row() + rOffset, from.col() + cOffset);
                        if (adj.row() >= 0 && adj.row() < board_->rows() &&
                            adj.col() >= 0 && adj.col() < board_->cols())
                        {
                            if (!board_->pieceAt(adj).has_value())
                            {
                                finalDestination = adj;
                                break;
                            }
                        }
                    }
                    if (finalDestination != from)
                        break;
                }
                piece->setState(PieceState::Idle);
                piece->setPosition(finalDestination);
                board_->placePiece(piece, finalDestination);
                cooldownTracker_.setCooldown(piece->id(), motion.arrivalTime() + config_.cooldownDurationMs);
                events.push_back({from, finalDestination, piece, false, true, false});
                return true;
            }
        }

        auto targetPieceOpt = board_->pieceAt(to);

        if (targetPieceOpt.has_value() &&
            targetPieceOpt.value() &&
            (targetPieceOpt.value()->state() == PieceState::Airborne ||
             targetPieceOpt.value()->state() == PieceState::Moving))
        {
            targetPieceOpt = std::nullopt;
        }

        bool isFriendlyBlock = false;
        if (targetPieceOpt.has_value() && targetPieceOpt.value())
        {
            auto targetPiece = targetPieceOpt.value();
            if (targetPiece->color() == piece->color())
            {
                // פרש אינו נחסם על ידי כלי ידידותי ביעד - הוא מבצע Self-Kill
                if (piece->type() == PieceType::Knight)
                {
                    isFriendlyBlock = false;
                }
                else
                {
                    isFriendlyBlock = true;
                }
            }
        }

        if (targetPieceOpt.has_value() && targetPieceOpt.value() &&
            piece->type() == PieceType::Pawn &&
            targetPieceOpt.value()->color() != piece->color() &&
            to.col() == from.col())
        {
            Position stopPos = findLastVacantPositionOnPath(from, to, board_);
            piece->setState(PieceState::Idle);
            piece->setPosition(stopPos);
            cooldownTracker_.setCooldown(piece->id(), motion.arrivalTime() + config_.cooldownDurationMs);
            events.push_back({from, stopPos, piece, false, true, false});
            return true;
        }

        Position finalDestination = to;

        if (isFriendlyBlock)
        {
            // כלים מחליקים שנחסמים ביעד על ידי ידיד עוצרים פשוט במשבצת הפנויה האחרונה בנתיב
            finalDestination = findLastVacantPositionOnPath(from, to, board_);

            piece->setState(PieceState::Idle);
            piece->setPosition(finalDestination);

            cooldownTracker_.setCooldown(piece->id(), motion.arrivalTime() + config_.cooldownDurationMs);
            events.push_back({from, finalDestination, piece, false, true, false});
            return true;
        }

        bool capturedKing = false;
        bool isCapture = targetPieceOpt.has_value() && targetPieceOpt.value() != nullptr;
        if (isCapture)
        {
            auto targetPiece = targetPieceOpt.value();
            if (targetPiece->type() == PieceType::King)
            {
                capturedKing = true;
            }
            targetPiece->setState(PieceState::Captured);
            cooldownTracker_.clear(targetPiece->id());
            board_->removePiece(targetPiece);
        }

        piece->setPosition(finalDestination);
        piece->setState(PieceState::Idle);
        piece->markMoved();

        PiecePtr finalPiece = piece;
        if (promoteCallback)
        {
            finalPiece = promoteCallback(piece, finalDestination);
        }
        if (finalPiece != piece)
        {
            cooldownTracker_.clear(piece->id());
        }
        cooldownTracker_.setCooldown(finalPiece->id(), motion.arrivalTime() + config_.cooldownDurationMs);
        events.push_back({from, finalDestination, finalPiece, capturedKing, false, isCapture});

        return true;
    }

} // namespace kungfu