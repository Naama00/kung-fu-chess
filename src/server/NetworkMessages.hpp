#pragma once

#include <cstdint>
#include <cstddef>

namespace kungfu {

// סוגי ההודעות ברשת
enum class NetworkMessageType : std::uint8_t {
    JOIN_MATCH_REQUEST,       // לקוח מבקש להצטרף למשחק
    MATCH_FOUND,              // השרת מודיע שמצא משחק ומחזיר צבע ומזהה משחק
    GAME_MOVE,                // שליחת מהלך (מלקוח לשרת או משרת ללקוח)
    MOVE_RESULT,              // השרת מחזיר את תוצאת המהלך (חוקי/לא חוקי)
    HEARTBEAT,                // הודעת סיגנל קטנה לשמירה על החיבור
    GAME_OVER,                // השרת מודיע ששני הצדדים שהמשחק הסתיים (מצב טבעי)
    OPPONENT_DISCONNECTED     // השרת מודיע שהיריב התנתק והמשחק בוטל
};

// ייצוג קומפקטי של Position למעבר ברשת
struct NetworkPosition {
    std::int32_t x;
    std::int32_t y;
};

// המבנה הלוגי (בזיכרון) של הודעת מהלך.
// שים לב: מבנה זה משמש רק כייצוג פנימי נוח לעבודה בקוד.
// הוא *לא* נשלח ברשת באמצעות memcpy ישיר - הסריאליזציה בפועל
// (סדר בתים, גודל שדות) מתבצעת באופן מפורש ב-Serializer,
// כדי לא להיות תלויים בפריסת הזיכרון (padding/ABI) של הקומפיילר.
struct NetworkMovePacket {
    std::uint64_t matchId;
    std::uint64_t requestId;
    std::uint8_t  playerColor; // מומר מ-PlayerColor
    NetworkPosition from;
    NetworkPosition to;
};

// גודל קבוע ומפורש (לא sizeof!) של NetworkMovePacket כפי שהוא נכתב על הרשת:
// 8 (matchId) + 8 (requestId) + 1 (playerColor) + 4*4 (from.x/y, to.x/y) = 33 בתים
inline constexpr std::size_t kMovePacketWireSize = 8 + 8 + 1 + 4 * 4;

// כותרת כללית לכל חבילה ברשת: בית אחד לסוג ההודעה, 4 בתים (Big-Endian) לאורך ה-payload
inline constexpr std::size_t kHeaderSize = 5;

// הגנה מפני תוקף/באג שמנסה לגרום לשרת להקצות payload ענק (DoS).
// 64KB נדיב בהרבה מכל הודעה לגיטימית בפרוטוקול הנוכחי.
inline constexpr std::uint32_t kMaxPayloadSize = 64 * 1024;

} // namespace kungfu