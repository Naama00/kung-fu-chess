#pragma once

#include "NetworkMessages.hpp"
#include "engine/actions/ActionRequest.hpp"
#include "engine/actions/ActionResult.hpp"
#include <cstring>
#include <cstdint>
#include <optional>
#include <vector>

namespace kungfu {

// ============================================================================
// Serializer
//
// אחראי על כל ההמרה בין מבני נתונים בזיכרון לבין רצפי בתים שעוברים ברשת.
//
// חשוב: לגבי NetworkMovePacket ו-NetworkHeader אנחנו כותבים/קוראים כל שדה
// בנפרד (Big-Endian), ולא מעתיקים struct שלם עם memcpy. הסיבה: פריסת
// הזיכרון של struct (padding, alignment) תלויה בקומפיילר ובפלטפורמה, ואינה
// מובטחת להיות זהה בין תהליך השרת לתהליך הלקוח. סריאליזציה מפורשת חוסנת
// אותנו מפני זה ומאפשרת לקליינטים שנכתבים בשפות/פלטפורמות אחרות (כולל Web)
// לפענח את הפרוטוקול בביטחון.
//
// לגבי ActionResult: זהו טיפוס פנימי של מנוע המשחק שאיננו מוגדר בקובץ הזה.
// כרגע הוא עדיין מועבר בהעתקה גולמית (memcpy), בהנחה ששרת ולקוח משתמשים
// באותו build של המנוע. אם בעתיד ActionResult ישלח ללקוחות חיצוניים
// (למשל דפדפן), יש להוסיף לו סריאליזציה מפורשת באותו האופן שנעשה כאן
// ל-NetworkMovePacket.
// ============================================================================
class Serializer {
public:
    // ---- כתיבה (Big-Endian) ----

    static void writeU8(std::vector<std::uint8_t>& buf, std::uint8_t v) {
        buf.push_back(v);
    }

    static void writeU32(std::vector<std::uint8_t>& buf, std::uint32_t v) {
        buf.push_back(static_cast<std::uint8_t>(v >> 24));
        buf.push_back(static_cast<std::uint8_t>(v >> 16));
        buf.push_back(static_cast<std::uint8_t>(v >> 8));
        buf.push_back(static_cast<std::uint8_t>(v));
    }

    static void writeI32(std::vector<std::uint8_t>& buf, std::int32_t v) {
        writeU32(buf, static_cast<std::uint32_t>(v));
    }

    static void writeU64(std::vector<std::uint8_t>& buf, std::uint64_t v) {
        for (int shift = 56; shift >= 0; shift -= 8) {
            buf.push_back(static_cast<std::uint8_t>(v >> shift));
        }
    }

    // ---- קריאה (Big-Endian) ----
    // כל פונקציית read מקדמת את offset ומחזירה false אם אין מספיק בתים.

    static bool readU8(const std::vector<std::uint8_t>& buf, std::size_t& offset, std::uint8_t& out) {
        if (offset + 1 > buf.size()) return false;
        out = buf[offset];
        offset += 1;
        return true;
    }

    static bool readU32(const std::vector<std::uint8_t>& buf, std::size_t& offset, std::uint32_t& out) {
        if (offset + 4 > buf.size()) return false;
        out = (static_cast<std::uint32_t>(buf[offset])     << 24) |
              (static_cast<std::uint32_t>(buf[offset + 1]) << 16) |
              (static_cast<std::uint32_t>(buf[offset + 2]) << 8)  |
               static_cast<std::uint32_t>(buf[offset + 3]);
        offset += 4;
        return true;
    }

    static bool readI32(const std::vector<std::uint8_t>& buf, std::size_t& offset, std::int32_t& out) {
        std::uint32_t raw = 0;
        if (!readU32(buf, offset, raw)) return false;
        out = static_cast<std::int32_t>(raw);
        return true;
    }

    static bool readU64(const std::vector<std::uint8_t>& buf, std::size_t& offset, std::uint64_t& out) {
        if (offset + 8 > buf.size()) return false;
        out = 0;
        for (int i = 0; i < 8; ++i) {
            out = (out << 8) | static_cast<std::uint64_t>(buf[offset + i]);
        }
        offset += 8;
        return true;
    }

    // ---- כותרת + payload -> frame מלא לשליחה ----

    static std::vector<std::uint8_t> buildFrame(NetworkMessageType type,
                                                  const std::vector<std::uint8_t>& payload) {
        std::vector<std::uint8_t> frame;
        frame.reserve(kHeaderSize + payload.size());

        writeU8(frame, static_cast<std::uint8_t>(type));
        writeU32(frame, static_cast<std::uint32_t>(payload.size()));
        frame.insert(frame.end(), payload.begin(), payload.end());

        return frame;
    }

    // ---- NetworkMovePacket ----

    static std::vector<std::uint8_t> serializeMovePacket(const NetworkMovePacket& packet) {
        std::vector<std::uint8_t> buf;
        buf.reserve(kMovePacketWireSize);

        writeU64(buf, packet.matchId);
        writeU64(buf, packet.requestId);
        writeU8(buf, packet.playerColor);
        writeI32(buf, packet.from.x);
        writeI32(buf, packet.from.y);
        writeI32(buf, packet.to.x);
        writeI32(buf, packet.to.y);

        return buf;
    }

    // מחזיר std::nullopt אם ה-payload שגוי/קצר מדי, במקום להתפרק בשקט.
    static std::optional<NetworkMovePacket> deserializeMovePacket(const std::vector<std::uint8_t>& buf) {
        if (buf.size() != kMovePacketWireSize) {
            return std::nullopt;
        }

        std::size_t offset = 0;
        NetworkMovePacket packet{};

        bool ok = true;
        ok &= readU64(buf, offset, packet.matchId);
        ok &= readU64(buf, offset, packet.requestId);
        ok &= readU8(buf, offset, packet.playerColor);
        ok &= readI32(buf, offset, packet.from.x);
        ok &= readI32(buf, offset, packet.from.y);
        ok &= readI32(buf, offset, packet.to.x);
        ok &= readI32(buf, offset, packet.to.y);

        if (!ok) return std::nullopt;
        return packet;
    }

    // תרגום חבילת רשת לבקשת מנוע.
    // בבנאי של Position, הפרמטר הראשון הוא row (y) והשני הוא col (x).
    static ActionRequest deserializeToRequest(const NetworkMovePacket& packet) {
        Position from(packet.from.y, packet.from.x);
        Position to(packet.to.y, packet.to.x);

        PlayerAction action(from, to);

        return ActionRequest(
            packet.requestId,
            static_cast<PlayerColor>(packet.playerColor),
            action
        );
    }

    // ---- ActionResult ----
    // ראה הערה בראש הקובץ לגבי ההנחה על ABI משותף.

    static std::vector<std::uint8_t> serializeActionResult(const ActionResult& result) {
        std::vector<std::uint8_t> buf(sizeof(ActionResult));
        std::memcpy(buf.data(), &result, sizeof(ActionResult));
        return buf;
    }

    static std::optional<ActionResult> deserializeToResult(const std::vector<std::uint8_t>& buffer) {
        if (buffer.size() < sizeof(ActionResult)) {
            return std::nullopt;
        }
        ActionResult result(0, ActionStatus::Rejected);
        std::memcpy(&result, buffer.data(), sizeof(ActionResult));
        return result;
    }

    static std::vector<std::uint8_t> serializeAuthRequest(const std::string& username, const std::string& password) {
        std::vector<std::uint8_t> buf;
        // אריזה: 4 בתים אורך המשתמש + המשתמש עצמו, ואז 4 בתים אורך סיסמה + הסיסמה עצמה
        writeU32(buf, static_cast<std::uint32_t>(username.size()));
        for (char c : username) buf.push_back(static_cast<std::uint8_t>(c));

        writeU32(buf, static_cast<std::uint32_t>(password.size()));
        for (char c : password) buf.push_back(static_cast<std::uint8_t>(c));

        return buf;
    }

    static bool deserializeAuthRequest(const std::vector<std::uint8_t>& buf, std::string& outUsername, std::string& outPassword) {
        std::size_t offset = 0;
        std::uint32_t userLen = 0;
        if (!readU32(buf, offset, userLen) || offset + userLen > buf.size()) return false;

        outUsername.assign(buf.begin() + offset, buf.begin() + offset + userLen);
        offset += userLen;

        std::uint32_t passLen = 0;
        if (!readU32(buf, offset, passLen) || offset + passLen > buf.size()) return false;

        outPassword.assign(buf.begin() + offset, buf.begin() + offset + passLen);
        return true;
    }
};

} // namespace kungfu