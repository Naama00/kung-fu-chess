// core/engine/IGameObserver.hpp
#pragma once
#include "engine/common/ArrivalEvent.hpp"

namespace kungfu {

class IGameObserver {
public:
    virtual ~IGameObserver() = default;
    // Called each time a move physically completes on the board
    virtual void onMoveCompleted(const ArrivalEvent& event, int currentTimeMs) = 0;
};

} // namespace kungfu