#include "io/BoardParser.hpp"
#include "pieces/Piece.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace kungfu {

std::vector<std::string> BoardParser::splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string line;
    std::istringstream stream(text);
    while (std::getline(stream, line)) {
        // הסרת רווחים מיותרים מקצוות השורה
        line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        line.erase(std::find_if(line.rbegin(), line.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), line.end());

        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

std::vector<std::string> BoardParser::tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream stream(line);
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

bool BoardParser::isValidToken(const std::string& token) {
    if (token == ".") return true;
    if (token.size() != 2) return false;
    
    char color = token[0];
    char type = token[1];
    
    bool validColor = (color == 'w' || color == 'b');
    bool validType = (type == 'K' || type == 'Q' || type == 'R' || type == 'B' || type == 'N' || type == 'P');
    
    return validColor && validType;
}

std::shared_ptr<Piece> BoardParser::createPiece(const std::string& token, const Position& pos) {
    if (token == "." || token.size() < 2) {
        return nullptr;
    }
    
    PlayerColor color = (token[0] == 'w') ? PlayerColor::White : PlayerColor::Black;
    char type = token[1];
    
    switch (type) {
        case 'K': return std::make_shared<Piece>(PieceType::King,   color, pos);
        case 'Q': return std::make_shared<Piece>(PieceType::Queen,  color, pos);
        case 'R': return std::make_shared<Piece>(PieceType::Rook,   color, pos);
        case 'B': return std::make_shared<Piece>(PieceType::Bishop, color, pos);
        case 'N': return std::make_shared<Piece>(PieceType::Knight, color, pos);
        case 'P': return std::make_shared<Piece>(PieceType::Pawn,   color, pos);
        default:  return nullptr;
    }
}

std::shared_ptr<Board> BoardParser::parse(const std::string& text) {
    auto lines = splitLines(text);
    if (lines.empty()) {
        return nullptr;
    }

    int rows = static_cast<int>(lines.size());
    std::vector<std::vector<std::string>> grid;
    int cols = -1;

    for (int r = 0; r < rows; ++r) {
        auto tokens = tokenize(lines[r]);
        if (tokens.empty()) {
            return nullptr;
        }
        if (cols == -1) {
            cols = static_cast<int>(tokens.size());
        } else if (static_cast<int>(tokens.size()) != cols) {
            // הלוח אינו מלבני תקין
            return nullptr;
        }
        grid.push_back(tokens);
    }

    auto board = std::make_shared<Board>(rows, cols);

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const std::string& token = grid[r][c];
            if (!isValidToken(token)) {
                return nullptr; // טוקן שגוי בלוח
            }
            auto piece = createPiece(token, Position(r, c));
            if (piece) {
                board->placePiece(piece, Position(r, c));
            }
        }
    }

    return board;
}

}  // namespace kungfu