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

    // 1. Check board boundaries
    auto isOutOfBounds = [maxRows, maxCols](const Position& pos) noexcept {
        return pos.row() < 0 || pos.row() >= maxRows || pos.col() < 0 || pos.col() >= maxCols;
    };

    if (isOutOfBounds(from) || isOutOfBounds(to)) {
        return {false, "outside_board"};
    }

    // 2. Check that a piece exists at the source position
    auto sourcePieceOpt = board_->pieceAt(from);
    if (!sourcePieceOpt.has_value() || !sourcePieceOpt.value()) {
        return {false, "empty_source"};
    }

    auto piece = sourcePieceOpt.value();

    // 3. Prevent movement to the exact same square
    if (from == to) {
        return {false, "illegal_piece_move"};
    }

    // 4. Prevent capturing a friendly piece at the target
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

    // 5. Query geometric legality against the appropriate strategy
    const auto& rule = PieceRuleFactory::getRule(piece->type());
    auto legalDestinations = rule.getLegalDestinations(*board_, *piece);

    auto it = std::find(legalDestinations.begin(), legalDestinations.end(), to);
    if (it != legalDestinations.end()) {
        return {true, "ok"};
    }

    return {false, "illegal_piece_move"};
}

// Implementation of the hypothetical verification that temporarily manipulates the board and then cleans up afterward
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

    // If the piece is not physically located on the square from which we want to perform the premove, place it there temporarily
    if (temporaryReposition) {
        if (board_->pieceAt(from).has_value()) {
            return {false, "source_occupied"};
        }

        piece->setPosition(from);
        placedTemporarily = board_->placePiece(piece, from);
        if (!placedTemporarily) {
            piece->setPosition(originalPos);
            return {false, "internal_error"};
        }
    }

    // Run the standard validation
    MoveValidation validation = validateMove(from, to);

    // Clean up and restore the board and piece to their exact original state
    if (temporaryReposition) {
        if (placedTemporarily) {
            board_->removePiece(piece);
        }
        piece->setPosition(originalPos);
    }

    return validation;
}

}  // namespace kungfu