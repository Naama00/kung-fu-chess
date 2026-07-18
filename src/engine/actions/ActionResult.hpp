// engine/actions/ActionResult.hpp
#pragma once

#include <cstdint>

namespace kungfu {

/// תוצאת עיבוד ActionRequest על ידי GameEngine.
enum class ActionStatus {
    Accepted,       // הבקשה התקבלה ובוצעה
    Rejected,       // הבקשה נדחתה (למשל מהלך לא חוקי, כלי בתנועה, כלי בצינון, יעד לא חוקי)
    StoredAsPending // הבקשה נשמרה כ-premove לביצוע עתידי
};

// תוצאה מלאה הכוללת סטטוס ומזהה הבקשה המקורית.
//   - IllegalMove      : המהלך אינו חוקי לפי חוקי השחמט
//   - PieceMoving      : הכלי כבר נמצא בתנועה
//   - PieceCoolingDown : הכלי נמצא בצינון
//   - InvalidTarget    : מיקום יעד מחוץ לגבולות הלוח
struct ActionResult {
    std::uint64_t requestId; // מזהה הבקשה המקורית
    ActionStatus  status;

    ActionResult(std::uint64_t requestId, ActionStatus status)
        : requestId(requestId), status(status) {}
};

} // namespace kungfu
