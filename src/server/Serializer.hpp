#pragma once

#include "NetworkMessages.hpp"
#include "engine/actions/ActionRequest.hpp"
#include "engine/actions/ActionResult.hpp"
#include <cstring>
#include <vector>

namespace kungfu {

class Serializer {
public:
    // תרגום חבילת רשת לבקשת מנוע - תיקון בנאי ללא דיפולט של PlayerAction
    static ActionRequest deserializeToRequest(const NetworkMovePacket& packet) {
        // בבנאי של Position, הראשון הוא row (שורה, y) והשני הוא col (עמודה, x)
        Position from(packet.from.y, packet.from.x);
        Position to(packet.to.y, packet.to.x);
        
        // אתחול תקין של האקשן באמצעות הבנאי היחיד שקיים עבורו
        PlayerAction action(from, to);

        return ActionRequest(
            packet.requestId, 
            static_cast<PlayerColor>(packet.playerColor), 
            action
        );
    }

    // תרגום תוצאת מנוע לחבילת רשת - תיקון שגיאת הכתיב ל-ActionStatus::Rejected
    static ActionResult deserializeToResult(const std::vector<std::uint8_t>& buffer) {
        ActionResult result(0, ActionStatus::Rejected);
        if (buffer.size() >= sizeof(ActionResult)) {
            std::memcpy(&result, buffer.data(), sizeof(ActionResult));
        }
        return result;
    }
};

} // namespace kungfu