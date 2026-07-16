// core/engine/IGameObserver.hpp
#pragma once
#include "engine/common/ArrivalEvent.hpp"

namespace kungfu {

class IGameObserver {
public:
    virtual ~IGameObserver() = default;
    // נקרא בכל פעם שמהלך מסתיים פיזית על הלוח
    virtual void onMoveCompleted(const ArrivalEvent& event, int currentTimeMs) = 0;
};

} // namespace kungfu