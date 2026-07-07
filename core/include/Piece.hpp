#ifndef PIECE_HPP
#define PIECE_HPP

#include "Position.hpp"
#include <string>

enum class Color { White, Black, None };

class Piece {
private:
    std::string type; // למשל "wK", "bP", "."
    Color color;
    Position pos;
    bool moving;

public:
    Piece() : type("."), color(Color::None), pos({0, 0}), moving(false) {}
    Piece(std::string t, Position p) : type(t), pos(p), moving(false) {
        if (t == ".") color = Color::None;
        else color = (t[0] == 'w') ? Color::White : Color::Black;
    }

    std::string getType() const { return type; }
    Color getColor() const { return color; }
    Position getPosition() const { return pos; }
    void setPosition(Position p) { pos = p; }
};

#endif