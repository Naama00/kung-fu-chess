// engine/events/GameEvents.hpp
#pragma once
#include "engine/common/ArrivalEvent.hpp"
#include "engine/common/Enums.hpp"
#include "engine/common/Position.hpp"
#include <string>

namespace kungfu {

// 1. Event for the end of physical movement on the board
struct MoveCompletedEvent {
    ArrivalEvent detail;
};

// 2. Event for changing the game score
struct ScoreChangedEvent {
    int whiteScore;
    int blackScore;
};

// 3. Event requesting sound playback (completely separating logic from Sound Player implementation)
struct PlaySoundEvent {
    std::string soundId;
};

// 4. Game transition movement event (start/end animations)
enum class GameTransitionType {
    Started,
    Ended
};

struct GameTransitionEvent {
    GameTransitionType type;
    PlayerColor winnerColor; // relevant to Ended
    bool isDraw = false;
};

} // namespace kungfu