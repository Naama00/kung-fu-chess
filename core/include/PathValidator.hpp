#ifndef PATH_VALIDATOR_HPP
#define PATH_VALIDATOR_HPP

#include "Board.hpp"
#include <string>

class PathValidator {
public:
    // בודק שהמסלול בין המקור ליעד אינו חסום (עבור רץ וצריח)
    static bool isPathClear(const Board& board, int srcRow, int srcCol, int destRow, int destCol);
    
    // בודק את חוקיות נקודת היעד (אי אפשר לאכול כלי מאותו צבע)
    static bool isValidTarget(const Board& board, int srcRow, int srcCol, int destRow, int destCol);
};

#endif