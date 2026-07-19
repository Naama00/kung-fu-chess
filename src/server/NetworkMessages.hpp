#pragma once

#include <cstdint>
#include <vector>

namespace kungfu {

// סוגי ההודעות ברשת
enum class NetworkMessageType : std::uint8_t {
    JOIN_MATCH_REQUEST,  // לקוח מבקש להצטרף למשחק
    MATCH_FOUND,         // השרת מודיע שמצא משחק ומחזיר צבע ומזהה משחק
    GAME_MOVE,           // שליחת מהלך (מלקוח לשרת או משרת ללקוח)
    MOVE_RESULT,         // השרת מחזיר את תוצאת המהלך (חוקי/לא חוקי) -- נוסף פסיק מתוקן כאן
    HEARTBEAT            // הודעת סיגנל קטנה לשמירה על החיבור
};

// ייצוג קומפקטי של Position למעבר ברשת
struct NetworkPosition {
    std::int32_t x;
    std::int32_t y;
};

// המבנה הקבוע של הודעת מהלך שעוברת בסוקט
struct NetworkMovePacket {
    std::uint64_t matchId;
    std::uint64_t requestId;
    std::uint8_t  playerColor; // מומר מ-PlayerColor
    NetworkPosition from;
    NetworkPosition to;
};

// כותרת כללית לכל חבילה ברשת כדי שהשרת ידע איך לקרוא את ההמשך
struct NetworkHeader {
    NetworkMessageType type;
    std::uint32_t payloadSize; // אורך הנתונים שבאים מיד אחרי הכותרת
};

} // namespace kungfu