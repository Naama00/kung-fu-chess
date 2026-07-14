#include "core/io/BoardPrinter.hpp"
#include "core/common/PieceTokenCodec.hpp"
#include <sstream>
#include <algorithm>

namespace kungfu {

// פונקציית עזר יחידה ואחידה לקריאת כלי (עבור ConstPiecePtr)
std::string BoardPrinter::getPieceToken(const ConstPiecePtr& piece) {
    if (!piece) {
        return "."; // תו יחיד לייצוג משבצת ריקה
    }

    std::string token = (piece->color() == PlayerColor::White) ? "w" : "b";
    token += PieceTokenCodec::toChar(piece->type());
    return token;
}

// מימוש פונקציית ההדפסה הראשית
std::string BoardPrinter::print(const Board& board) {
    std::ostringstream out;
    int rows = board.rows();
    int cols = board.cols();

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            auto pieceOpt = board.pieceAt(Position(r, c));
            
            if (pieceOpt.has_value()) {
                // קריאה לפונקציה שמקבלת את ה-ConstPiecePtr שהתקבל מהלוח
                out << getPieceToken(pieceOpt.value());
            } else {
                out << "."; // תו יחיד לייצוג משבצת ריקה
            }
            
            if (c + 1 < cols) {
                out << " "; // רווח בין משבצות באותה שורה
            }
        }
        out << "\n";
    }
    return out.str();
}

}  // namespace kungfu