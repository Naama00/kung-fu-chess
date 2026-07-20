// engine/analysis/EloCalculator.hpp
#pragma once
#include <cmath>

namespace kungfu {

class EloCalculator {
public:
    /**
     * Computes the new rating of player A after a match against player B.
     * @param ratingA the current rating of player A.
     * @param ratingB the current rating of player B.
     * @param scoreA the game result for player A (1.0 = win, 0.5 = draw, 0.0 = loss).
     * @param kFactor מקדם השינוי המרבי (בד"כ 32).
     * @return הדירוג החדש המעוגל.
     */
    static int calculateNewRating(int ratingA, int ratingB, double scoreA, int kFactor = 32) {
        // compute player A's expected win probabilities
        double expectedScoreA = 1.0 / (1.0 + std::pow(10.0, (ratingB - ratingA) / 400.0));
        
        // update the rating
        double newRating = ratingA + kFactor * (scoreA - expectedScoreA);
        return static_cast<int>(std::round(newRating));
    }
};

} // namespace kungfu