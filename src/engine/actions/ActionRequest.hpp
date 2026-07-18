// engine/actions/ActionRequest.hpp
#pragma once

#include <cstdint>
#include "engine/actions/PlayerAction.hpp"
#include "engine/common/Enums.hpp"

namespace kungfu {

// עוטף PlayerAction עם הקשר זיהוי: מי שלח ואיזה בקשה.
// requestId מיועד למעקב ודיבוג — מאפשר לקשר בין בקשה לתוצאתה
// גם כשמספר שחקנים שולחים פעולות בו-זמנית.
struct ActionRequest {
    std::uint64_t requestId;    // מזהה ייחודי לבקשה (לצורך מעקב/דיבוג)
    PlayerColor   playerColor;  // צבע השחקן שהגיש את הבקשה
    PlayerAction  action;       // הפעולה המבוקשת

    ActionRequest(std::uint64_t requestId, PlayerColor playerColor, PlayerAction action)
        : requestId(requestId), playerColor(playerColor), action(action) {}
};

} // namespace kungfu
