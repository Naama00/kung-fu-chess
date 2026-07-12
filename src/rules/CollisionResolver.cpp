#include "rules/CollisionResolver.hpp"
#include <cmath>

namespace kungfu {

namespace {

// מחשבת את מסלול התנועה הדיסקרטי מהמשבצת שלפני היעד בחזרה לנקודת המוצא
Position findLastVacantPositionOnPath(
    const Position& from,
    const Position& to,
    const std::shared_ptr<IBoard>& board
) noexcept {
    int r1 = from.row();
    int c1 = from.col();
    int r2 = to.row();
    int c2 = to.col();

    int dr = r2 - r1;
    int dc = c2 - c1;

    if (dr == 0 && dc == 0) {
        return from;
    }

    // חישוב כיווני ההתקדמות (-1, 0, 1)
    int stepR = (dr > 0) ? 1 : ((dr < 0) ? -1 : 0);
    int stepC = (dc > 0) ? 1 : ((dc < 0) ? -1 : 0);

    std::vector<Position> path;
    
    // יצירת רשימת משבצות המסלול מהסוף (אחת לפני to) ועד ההתחלה (from)
    int curR = r2 - stepR;
    int curC = c2 - stepC;

    while (curR != r1 || curC != c1) {
        path.emplace_back(curR, curC);
        curR -= stepR;
        curC -= stepC;
    }
    path.push_back(from); // נקודת המוצא היא ברירת המחדל האחרונה בהחלט

    // מציאת המשבצת הפנויה הראשונה שנתקלים בה בנסיגה לאחור
    for (const auto& pos : path) {
        if (!board->pieceAt(pos).has_value()) {
            return pos;
        }
    }

    // מקרה חירום קיצוני (אם אפילו משבצת המוצא נתפסה בינתיים): מחפשים משבצת פנויה צמודה למוצא
    for (int drOffset = -1; drOffset <= 1; ++drOffset) {
        for (int dcOffset = -1; dcOffset <= 1; ++dcOffset) {
            Position fallback(from.row() + drOffset, from.col() + dcOffset);
            if (fallback.row() >= 0 && fallback.row() < board->rows() &&
                fallback.col() >= 0 && fallback.col() < board->cols()) {
                if (!board->pieceAt(fallback).has_value()) {
                    return fallback;
                }
            }
        }
    }

    return from; // מוצא אחרון בהחלט למניעת קריסה
}

} // namespace

CollisionResolver::CollisionResolver(
    std::shared_ptr<IBoard> board, 
    CooldownTracker& cooldownTracker, 
    const GameConfig& config
) noexcept
    : board_(std::move(board)), cooldownTracker_(cooldownTracker), config_(config) {}

void CollisionResolver::resolveMidRouteCollision(
    const Motion& winner,
    const Motion& loser,
    std::vector<ArrivalEvent>& events
) noexcept {
    loser.piece()->setState(PieceState::Captured);
    cooldownTracker_.clear(loser.piece()->id());
    board_->removePiece(loser.from());

    events.push_back({loser.from(), loser.from(), loser.piece(), false, true});
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

    // טיפול בנחיתה מקפיצה (Airborne)
    if (from == to) {
        piece->setState(PieceState::Idle);
        piece->setPosition(to); // החזרת המיקום הלוגי למקום הנחיתה
        cooldownTracker_.setCooldown(piece->id(), currentTimeMs + config_.cooldownDurationMs);
        events.push_back({from, to, piece, false, false});
        return true;
    }

    auto targetPieceOpt = board_->pieceAt(to);

    // בדיקת חסימה על ידי כלי ידידותי ביעד
    bool isFriendlyBlock = false;
    if (targetPieceOpt.has_value() && targetPieceOpt.value()) {
        auto targetPiece = targetPieceOpt.value();
        if (targetPiece->color() == piece->color()) {
            if (piece->type() != PieceType::Knight) { // פרשים מתעלמים מחסימות ידידותיות (מכים אותן)
                isFriendlyBlock = true;
            }
        }
    }

    Position finalDestination = to;

    if (isFriendlyBlock) {
        // מציאת המשבצת הפנויה האחרונה לאורך מסלול הנסיגה
        finalDestination = findLastVacantPositionOnPath(from, to, board_);

        piece->setState(PieceState::Idle);
        piece->setPosition(finalDestination); // מיקום הכלי במשבצת הנסיגה
        
        // הטלת צינון כי הכלי בכל זאת ביצע תנועה והתעייף
        cooldownTracker_.setCooldown(piece->id(), currentTimeMs + config_.cooldownDurationMs);
        
        events.push_back({from, finalDestination, piece, false, true});
        return true;
    }

    // נחיתה רגילה ללא חסימות ידידותיות
    // אכילת כלי עוין ביעד (אם קיים)
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
    cooldownTracker_.setCooldown(finalPiece->id(), currentTimeMs + config_.cooldownDurationMs);
    events.push_back({from, finalDestination, finalPiece, capturedKing, false});

    return true;
}

} // namespace kungfu