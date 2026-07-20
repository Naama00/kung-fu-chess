// engine/events/GameEvents.hpp
#pragma once
#include "engine/common/ArrivalEvent.hpp"
#include "engine/common/Enums.hpp"
#include "engine/common/Position.hpp"
#include <string>

namespace kungfu {

// 1. אירוע סיום מהלך פיזי בלוח
struct MoveCompletedEvent {
    ArrivalEvent detail;
};

// 2. אירוע שינוי ניקוד המשחק
struct ScoreChangedEvent {
    int whiteScore;
    int blackScore;
};

// 3. אירוע בקשה לניגון קול (מפריד לחלוטין בין הלוגיקה למימוש ה-Sound Player)
struct PlaySoundEvent {
    std::string soundId;
};

// 4. אירוע תנועת מעבר משחק (אנימציות התחלה / סיום)
enum class GameTransitionType {
    Started,
    Ended
};

struct GameTransitionEvent {
    GameTransitionType type;
    PlayerColor winnerColor; // רלוונטי ל-Ended
    bool isDraw = false;
};

} // namespace kungfu