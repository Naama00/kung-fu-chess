#ifndef BOARD_HPP
#define BOARD_HPP

#include <vector>
#include <string>
#include <string_view>
#include <unordered_set>

class Board {
private:
    std::vector<std::vector<std::string>> grid;

public:
    Board() = default;
    
    void clear() { grid.clear(); }
    void addRow(const std::vector<std::string>& row) { grid.push_back(row); }
    
    const std::vector<std::vector<std::string>>& getGrid() const { return grid; }
    bool isEmpty() const { return grid.empty(); }
    size_t getWidth() const { return grid.empty() ? 0 : grid[0].size(); }
    size_t getHeight() const { return grid.size(); }
    
    std::string getTokenAt(int r, int c) const { return grid[r][c]; }
    void setTokenAt(int r, int c, const std::string& token) { grid[r][c] = token; }

    // העברת המימוש לכאן כפונקציית inline מונעת שגיאות לינקר במערכות אוטומטיות
    static bool isValidToken(std::string_view token) {
        static const std::unordered_set<std::string_view> validTokens = {
            ".", "wK", "wQ", "wR", "wB", "wN", "wP",
            "bK", "bQ", "bR", "bB", "bN", "bP"
        };
        return validTokens.find(token) != validTokens.end();
    }

    static bool isLegalMovePattern(const std::string& pieceType, int srcRow, int srcCol, int destRow, int destCol);
};

#endif