#include "io/BoardPrinter.hpp"
#include "common/PieceTokenCodec.hpp"
#include <sstream>

namespace kungfu {

std::string BoardPrinter::getPieceToken(const PiecePtr& piece) {
    if (!piece) {
        return ".";
    }

    std::string token = (piece->color() == PlayerColor::White) ? "w" : "b";
    token += PieceTokenCodec::toChar(piece->type());
    return token;
}

std::string BoardPrinter::print(const Board& board) {
    std::ostringstream out;
    int rows = board.rows();
    int cols = board.cols();

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            auto pieceOpt = board.pieceAt(Position(r, c));
            if (pieceOpt.has_value()) {
                out << getPieceToken(pieceOpt.value());
            } else {
                out << ".";
            }
            if (c + 1 < cols) {
                out << " ";
            }
        }
        out << "\n";
    }
    return out.str();
}

}  // namespace kungfu