// players/IPlayer.hpp
#pragma once

#include <vector>
#include "engine/actions/ActionRequest.hpp"
#include "engine/snapshot/GameSnapshot.hpp"

namespace kungfu {

/// @brief ממשק אחיד לכל סוגי השחקנים במשחק.
///
/// כל מימוש מקבל תמונת מצב לקריאה בלבד (GameSnapshot) ומחזיר
/// אוסף של בקשות פעולה (ActionRequest) שהוא מעוניין לבצע.
///
/// @note מימושים עתידיים — HumanPlayer, AIPlayer, NetworkPlayer,
///       ReplayPlayer — אינם רשאים לגשת ישירות ל-Board או ל-GameEngine.
///       ערוץ הכתיבה היחיד הוא החזרת ActionRequest מהמתודה הזו.
///       ה-GameEngine אחראי לאמת ולבצע כל בקשה בנפרד.
class IPlayer {
public:
    virtual ~IPlayer() = default;

    /// @brief מחשב ומחזיר את הפעולות הרצויות בהתבסס על מצב המשחק הנוכחי.
    /// @param snapshot תמונת מצב עדכנית של המשחק (לקריאה בלבד)
    /// @return אוסף בקשות פעולה; יכול להיות ריק אם אין פעולה רצויה כעת
    virtual std::vector<ActionRequest> decideActions(const view::GameSnapshot& snapshot) = 0;
};

} // namespace kungfu
