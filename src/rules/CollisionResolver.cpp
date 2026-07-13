#include "rules/CollisionResolver.hpp"
#include <cmath>
#include <algorithm>

namespace kungfu {

// מוצא את המשבצת האחרונה הפנויה בנתיב מ-from לכיוון to (הליכה קדימה).
// blockedPos הוא יעד של כלי אחר שגם הוא חוסם (למשל winner בהתנגשות ידידותית).
Position findLastVacantPositionOnPath(
    const Position& from,
    const Position& to,
    const std::shared_ptr<IBoard>& board,
    const Position* blockedPos = nullptr
) noexcept {
    int dr = to.row() - from.row();
    int dc = to.col() - from.col();

    if (dr == 0 && dc == 0) {
        return from;
    }

    int stepR = (dr > 0) ? 1 : ((dr < 0) ? -1 : 0);
    int stepC = (dc > 0) ? 1 : ((dc < 0) ? -1 : 0);

    Position last = from;
    int curR = from.row() + stepR;
    int curC = from.col() + stepC;

    while (true) {
        Position cur(curR, curC);

        // משבצת חסומה על ידי כלי קיים על הלוח (שאינו airborne)
        auto pieceOpt = board->pieceAt(cur);
        bool occupiedByStationary = pieceOpt.has_value() &&
                                    pieceOpt.value()->state() != PieceState::Airborne;

        // משבצת חסומה על ידי יעד winner
        bool occupiedByBlocked = (blockedPos != nullptr && cur == *blockedPos);

        if (occupiedByStationary || occupiedByBlocked) {
            break; // עצור לפני המשבצת החסומה
        }

        last = cur;

        if (cur == to) break;
        curR += stepR;
        curC += stepC;
    }

    return last;
}

CollisionResolver::CollisionResolver(
    std::shared_ptr<IBoard> board, 
    CooldownTracker& cooldownTracker, 
    const GameConfig& config
) noexcept
    : board_(std::move(board))
    , cooldownTracker_(cooldownTracker)
    , config_(config) {}

void CollisionResolver::resolveMidRouteCollision(
    const Motion& winner, // הגיע ראשון (המוקדם)
    const Motion& loser,  // הגיע שני (המאוחר)
    int currentTimeMs,
    std::vector<ArrivalEvent>& events
) noexcept {
    // מקרה 1: כלי אויב - הכלי שהגיע שני (המאוחר / loser) אוכל את הכלי שהגיע ראשון (המוקדם / winner)
    if (winner.piece()->color() != loser.piece()->color()) {
        winner.piece()->setState(PieceState::Captured);
        cooldownTracker_.clear(winner.piece()->id());
        
        winner.piece()->setPosition(winner.from());
        board_->removePiece(winner.from());

        events.push_back({winner.from(), winner.from(), winner.piece(), false, true});
    } 
    // מקרה 2: כלים ידידותיים - הכלי שהגיע מאוחר יותר (loser) נעצר במשבצת האחרונה הפנויה במסלולו
    else {
        // יעד ה-winner חוסם את מסלול ה-loser
        Position winnerDest = winner.to();
        Position stopPos = findLastVacantPositionOnPath(loser.from(), loser.to(), board_, &winnerDest);
        
        loser.piece()->setState(PieceState::Idle);
        loser.piece()->setPosition(stopPos);
        
        cooldownTracker_.setCooldown(loser.piece()->id(), currentTimeMs + config_.cooldownDurationMs);
        events.push_back({loser.from(), stopPos, loser.piece(), false, true});
    }
}

bool CollisionResolver::resolveArrival(
    const Motion& motion,
    int currentTimeMs,
    std::vector<ArrivalEvent>& events,
    const PromotionHandler& promoteCallback
) noexcept {
    Position from = motion.from();
    Position to = motion.to();
    auto piece = motion.piece();

    // מאפשרים הגעה מוקדמת רק אם משבצת הנחיתה בקפיצה במקום אכן פנויה
    if (from == to && !board_->pieceAt(to).has_value()) {
        piece->setState(PieceState::Idle);
        piece->setPosition(to);
        cooldownTracker_.setCooldown(piece->id(), motion.arrivalTime() + config_.cooldownDurationMs);
        events.push_back({from, to, piece, false, false});
        return true;
    }

    auto targetPieceOpt = board_->pieceAt(to);

    // כלי airborne על משבצת היעד — הוא "לא שם" לוגית.
    // הכלי המגיע יושב על המשבצת, והairborne יאכל אותו כשינחת.
    if (targetPieceOpt.has_value() &&
        targetPieceOpt.value() &&
        targetPieceOpt.value()->state() == PieceState::Airborne) {
        targetPieceOpt = std::nullopt;
    }

    // בדיקת חסימה על ידי כלי ידידותי ביעד
    bool isFriendlyBlock = false;
    if (targetPieceOpt.has_value() && targetPieceOpt.value()) {
        auto targetPiece = targetPieceOpt.value();
        if (targetPiece->color() == piece->color()) {
            if (piece->type() != PieceType::Knight) { // פרשים מתעלמים מחסימות ידידותיות
                isFriendlyBlock = true;
            }
        }
    }

    Position finalDestination = to;

    if (isFriendlyBlock) {
        if (from == to) {
            // תיקון באג 3: קפיצה במקום שנחסמה על ידי כלי ידידותי - מציאת משבצת פנויה סמוכה
            finalDestination = from;
            for (int rOffset = -1; rOffset <= 1; ++rOffset) {
                for (int cOffset = -1; cOffset <= 1; ++cOffset) {
                    if (rOffset == 0 && cOffset == 0) continue;
                    Position adj(from.row() + rOffset, from.col() + cOffset);
                    if (adj.row() >= 0 && adj.row() < board_->rows() &&
                        adj.col() >= 0 && adj.col() < board_->cols()) {
                        if (!board_->pieceAt(adj).has_value()) {
                            finalDestination = adj;
                            break;
                        }
                    }
                }
                if (finalDestination != from) break;
            }
        } else {
            finalDestination = findLastVacantPositionOnPath(from, to, board_);
        }

        piece->setState(PieceState::Idle);
        piece->setPosition(finalDestination);
        
        // תיקון באג 5: שימוש בזמן הגעה תאורטי מדוייק של הכלי לצורך קביעת הצינון
        cooldownTracker_.setCooldown(piece->id(), motion.arrivalTime() + config_.cooldownDurationMs);
        
        events.push_back({from, finalDestination, piece, false, true});
        return true;
    }

    // נחיתה רגילה ללא חסימות ידידותיות
    bool capturedKing = false;
    if (targetPieceOpt.has_value() && targetPieceOpt.value()) {
        auto targetPiece = targetPieceOpt.value();
        if (targetPiece->type() == PieceType::King) {
            capturedKing = true;
        }
        targetPiece->setState(PieceState::Captured);
        cooldownTracker_.clear(targetPiece->id());
        board_->removePiece(targetPiece->position());
    }

    piece->setPosition(finalDestination);
    piece->setState(PieceState::Idle);
    piece->markMoved();

    PiecePtr finalPiece = piece;
    if (promoteCallback) {
        finalPiece = promoteCallback(piece, finalDestination);
    }
    if (finalPiece != piece) {
        cooldownTracker_.clear(piece->id());
    }
    cooldownTracker_.setCooldown(finalPiece->id(), motion.arrivalTime() + config_.cooldownDurationMs);
    events.push_back({from, finalDestination, finalPiece, capturedKing, false});

    return true;
}

} // namespace kungfu