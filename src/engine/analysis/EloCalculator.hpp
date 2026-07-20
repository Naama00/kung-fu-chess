// engine/analysis/EloCalculator.hpp
#pragma once
#include <cmath>

namespace kungfu {

class EloCalculator {
public:
    /**
     * מחשב את הדירוג החדש של שחקן A לאחר משחק מול שחקן B.
     * @param ratingA הדירוג הנוכחי של שחקן A.
     * @param ratingB הדירוג הנוכחי של שחקן B.
     * @param scoreA תוצאת המשחק עבור שחקן A (1.0 = ניצחון, 0.5 = תיקו, 0.0 = הפסד).
     * @param kFactor מקדם השינוי המרבי (בד"כ 32).
     * @return הדירוג החדש המעוגל.
     */
    static int calculateNewRating(int ratingA, int ratingB, double scoreA, int kFactor = 32) {
        // חישוב סיכויי הזכייה הצפויים של שחקן A
        double expectedScoreA = 1.0 / (1.0 + std::pow(10.0, (ratingB - ratingA) / 400.0));
        
        // עדכון הדירוג
        double newRating = ratingA + kFactor * (scoreA - expectedScoreA);
        return static_cast<int>(std::round(newRating));
    }
};

} // namespace kungfu