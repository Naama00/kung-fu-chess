#include "rules/RuleEngine.hpp"
#include "rules/PieceRules.hpp"
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

    // 4. מניעת פגיעה בכלי ידידותי ביעד – לא חל על פרש, שיכול לנחות על כל משבצת
    if (piece->type() != PieceType::Knight) {
        auto targetPieceOpt = board_->pieceAt(to);
        if (targetPieceOpt.has_value() && targetPieceOpt.value()->color() == piece->color()) {
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

}  // namespace kungfu