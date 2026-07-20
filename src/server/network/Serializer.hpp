#pragma once

#include "server/network/NetworkMessages.hpp"
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
// Responsible for all conversion between in-memory data structures and the
// byte sequences that travel over the network.
//
// Important: for NetworkMovePacket and the packet header, we write/read each
// field individually (Big-Endian), rather than copying a whole struct via
// memcpy. Reason: a struct's memory layout (padding, alignment) depends on
// the compiler and platform, and is not guaranteed to be identical between
// the server process and a client process. Explicit serialization protects
// us from this and lets clients written in other languages/platforms
// (including a web client) decode the protocol reliably.
//
// Regarding ActionResult: this is an internal game-engine type not defined in
// this file. It is currently still transferred via a raw copy (memcpy), under
// the assumption that server and client share the same engine build. If
// ActionResult is ever sent to external clients (e.g. a browser), it should
// get explicit field-by-field serialization the same way NetworkMovePacket
// does.
// ============================================================================
class Serializer {
public:
    // ---- Writing (Big-Endian) ----

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

    // ---- Reading (Big-Endian) ----
    // Every read function advances offset and returns false if there aren't
    // enough bytes remaining.

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

    // ---- Header + payload -> full frame ready to send ----

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

    // Returns std::nullopt if the payload is malformed/too short, instead of
    // silently reading garbage.
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

    // Translates a network packet into an engine action request.
    // Note: in the Position constructor, the first parameter is row (y) and
    // the second is col (x).
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
    // See the note at the top of this file regarding the shared-ABI assumption.

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
        // Layout: 4-byte username length + username bytes, then 4-byte
        // password length + password bytes.
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