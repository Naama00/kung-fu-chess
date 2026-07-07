#ifndef POSITION_HPP
#define POSITION_HPP

struct Position {
    int row;
    int col;

    bool operator==(const Position& other) const {
        return row == other.row && col == other.col;
    }
};

struct Point2D {
    double x;
    double y;
};

struct PendingMove {
    std::string token;
    Position src;
    Position dest;
    long long arrivalTimeMs;
    bool isActive = false;
};
#endif