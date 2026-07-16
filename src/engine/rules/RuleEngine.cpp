#include "engine/rules/RuleEngine.hpp"
#include "engine/rules/PieceRules.hpp"
#include <algorithm>

namespace kungfu {

RuleEngine::RuleEngine(std::shared_ptr<IBoard> board) noexcept : board_(std::move(board)) {}

MoveValidation RuleEngine::validateMove(const Position& from, const Position& to) const {
    if (!board_) {
        return {false, "internal_error"};
    }

    int maxRows = board_->rows();
    int maxCols = board_->cols();

    // 1. בדיקת גבולות לוח
    auto isOutOfBounds = [maxRows, maxCols](const Position& pos) noexcept {
        return pos.row() < 0 || pos.row() >= maxRows || pos.col() < 0 || pos.col() >= maxCols;
    };

    if (isOutOfBounds(from) || isOutOfBounds(to)) {
        return {false, "outside_board"};
    }

    // 2. בדיקה שיש כלי במיקום המוצא
    auto sourcePieceOpt = board_->pieceAt(from);
    if (!sourcePieceOpt.has_value() || !sourcePieceOpt.value()) {
        return {false, "empty_source"};
    }

    auto piece = sourcePieceOpt.value();

    // 3. מניעת תנועה לאותה משבצת בדיוק
    if (from == to) {
        return {false, "illegal_piece_move"};
    }

    // 4. מניעת פגיעה בכלי ידידותי ביעד.
    // כלים שנמצאים בתנועה (Moving) או באוויר (Airborne) אינם חוסמים את היעד —
    // מיקומם על הלוח הוא אינטרפולטיבי בלבד ועשוי לחפוף את היעד בטעות.
    auto targetPieceOpt = board_->pieceAt(to);
    if (targetPieceOpt.has_value()) {
        auto targetPiece = targetPieceOpt.value();
        if (targetPiece &&
            targetPiece->state() != PieceState::Moving &&
            targetPiece->state() != PieceState::Airborne &&
            targetPiece->color() == piece->color()) {
            return {false, "friendly_destination"};
        }
    }

    // 5. שאילתת חוקיות גיאומטרית מול ה-Strategy המתאים
    const auto& rule = PieceRuleFactory::getRule(piece->type());
    auto legalDestinations = rule.getLegalDestinations(*board_, *piece);

    auto it = std::find(legalDestinations.begin(), legalDestinations.end(), to);
    if (it != legalDestinations.end()) {
        return {true, "ok"};
    }

    return {false, "illegal_piece_move"};
}

// מימוש האימות ההיפותטי שמבצע את המניפולציה הזמנית בלוח ומנקה אחריו
MoveValidation RuleEngine::validateHypotheticalMove(const PiecePtr& piece, const Position& from, const Position& to) const {
    if (!board_ || !piece) {
        return {false, "internal_error"};
    }

    int maxRows = board_->rows();
    int maxCols = board_->cols();

    auto isOutOfBounds = [maxRows, maxCols](const Position& pos) noexcept {
        return pos.row() < 0 || pos.row() >= maxRows || pos.col() < 0 || pos.col() >= maxCols;
    };

    if (isOutOfBounds(from) || isOutOfBounds(to)) {
        return {false, "outside_board"};
    }

    Position originalPos = piece->position();
    bool temporaryReposition = (originalPos != from);
    bool placedTemporarily = false;

    // אם הכלי לא ממוקם פיזית במשבצת שממנה רוצים לבצע את ה-Premove, נציב אותו שם זמנית
    if (temporaryReposition) {
        if (board_->pieceAt(from).has_value()) {
            return {false, "source_occupied"};
        }

        piece->setPosition(from);
        placedTemporarily = board_->placePiece(piece, from);
        if (!placedTemporarily) {
            // הצבה נכשלה מסיבה בלתי צפויה - משחזרים מיד ולא ממשיכים לאמת
            // מהלך על מצב לוח שקרי.
            piece->setPosition(originalPos);
            return {false, "internal_error"};
        }
    }

    // הרצת האימות הסטנדרטי
    MoveValidation validation = validateMove(from, to);

    // ניקוי והחזרת הלוח והכלי למצבם המקורי המדויק
    if (temporaryReposition) {
        if (placedTemporarily) {
            board_->removePiece(piece);
        }
        piece->setPosition(originalPos);
    }

    return validation;
}

}  // namespace kungfu