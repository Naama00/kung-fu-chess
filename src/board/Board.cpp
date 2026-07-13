// src/board/Board.cpp
#include "board/Board.hpp"
#include <algorithm>

namespace kungfu {

Board::Board() : rows_(8), cols_(8) {}

Board::Board(int rows, int cols) : rows_(rows), cols_(cols) {}

std::optional<PiecePtr> Board::pieceAt(const Position& position) {
    for (auto& piece : pieces_) {
        if (piece && piece->position() == position) {
            return piece;
        }
    }
    return std::nullopt;
}

std::optional<std::shared_ptr<const Piece>> Board::pieceAt(const Position& position) const {
    for (const auto& piece : pieces_) {
        if (piece && piece->position() == position) {
            return piece;
        }
    }
    return std::nullopt;
}

bool Board::placePiece(const PiecePtr& piece, const Position& position) {
    if (!piece) {
        return false;
    }

    if (pieceAt(position).has_value()) {
        return false;
    }

    piece->setPosition(position);
    pieces_.push_back(piece);
    return true;
}

bool Board::removePiece(const Position& position) {
    auto it = std::find_if(pieces_.begin(), pieces_.end(), [&](const PiecePtr& piece) {
        return piece && piece->position() == position;
    });

    if (it == pieces_.end()) {
        return false;
    }

    pieces_.erase(it);
    return true;
}

bool Board::movePiece(const Position& from, const Position& to) {
    PiecePtr movingPiece = nullptr;
    for (auto& piece : pieces_) {
        if (piece && piece->position() == from) {
            movingPiece = piece;
            break;
        }
    }

    if (!movingPiece) {
        return false;
    }

    if (pieceAt(to).has_value()) {
        return false;
    }

    movingPiece->setPosition(to);
    return true;
}

bool Board::replacePiece(const Position& position, const PiecePtr& newPiece) {
    auto it = std::find_if(pieces_.begin(), pieces_.end(), [&](const PiecePtr& p) {
        return p && p->position() == position;
    });

    if (it == pieces_.end() || !newPiece) {
        return false;
    }

    newPiece->setPosition(position);
    *it = newPiece;
    return true;
}

std::vector<PiecePtr> Board::pieces() {
    return pieces_;
}

std::vector<std::shared_ptr<const Piece>> Board::pieces() const {
    std::vector<std::shared_ptr<const Piece>> const_pieces;
    const_pieces.reserve(pieces_.size());
    for (const auto& piece : pieces_) {
        const_pieces.push_back(piece);
    }
    return const_pieces;
}

}  // namespace kungfu