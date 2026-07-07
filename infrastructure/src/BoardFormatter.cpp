#include "BoardFormatter.hpp"

void BoardFormatter::print(std::ostream& output, const Board& board) {
    const auto& grid = board.getGrid();
    for (const auto& row : grid) {
        for (size_t i = 0; i < row.size(); ++i) {
            if (i > 0) {
                output << ' ';
            }
            output << row[i];
        }
        output << '\n';
    }
}