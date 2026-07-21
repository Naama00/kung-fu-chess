#pragma once

#include "server/network/NetworkMessages.hpp"
#include "engine/actions/ActionRequest.hpp"
#include "engine/actions/ActionResult.hpp"
#include <string_view>
#include <cstring>
#include <cstdint>
#include <optional>
#include <vector>

namespace kungfu
{
// ============================================================================
// Serializer
//
// Converts between in-memory objects and the network wire format.
//
// All protocol fields are serialized explicitly in Big-Endian order instead of
// copying structs with memcpy. This avoids compiler-dependent padding,
// alignment, and endianness issues, keeping the protocol portable across
// platforms and programming languages.
//
// Note: ActionResult is currently serialized with memcpy because it is an
// internal engine type shared by native C++ components. If it is ever exposed
// to external clients, it should receive explicit field-by-field serialization.
// ============================================================================
    class Serializer
    {
    public:
        // ---- Writing (Big-Endian) ----

        static void writeU8(std::vector<std::uint8_t> &buf, std::uint8_t v)
        {
            buf.push_back(v);
        }

        static void writeU32(std::vector<std::uint8_t> &buf, std::uint32_t v)
        {
            buf.push_back(static_cast<std::uint8_t>(v >> 24));
            buf.push_back(static_cast<std::uint8_t>(v >> 16));
            buf.push_back(static_cast<std::uint8_t>(v >> 8));
            buf.push_back(static_cast<std::uint8_t>(v));
        }

        static void writeI32(std::vector<std::uint8_t> &buf, std::int32_t v)
        {
            writeU32(buf, static_cast<std::uint32_t>(v));
        }

        static void writeU64(std::vector<std::uint8_t> &buf, std::uint64_t v)
        {
            for (int shift = 56; shift >= 0; shift -= 8)
            {
                buf.push_back(static_cast<std::uint8_t>(v >> shift));
            }
        }

        static void writeString(std::vector<std::uint8_t> &buf,
                                std::string_view str)
        {
            writeU32(buf, static_cast<std::uint32_t>(str.size()));
            buf.insert(buf.end(), str.begin(), str.end());
        }

        // ---- Reading (Big-Endian) ----
        // Every read function advances offset and returns false if there aren't
        // enough bytes remaining.

        static bool readU8(const std::vector<std::uint8_t> &buf, std::size_t &offset, std::uint8_t &out)
        {
            if (offset + 1 > buf.size())
                return false;
            out = buf[offset];
            offset += 1;
            return true;
        }

        static bool readU32(const std::vector<std::uint8_t> &buf, std::size_t &offset, std::uint32_t &out)
        {
            if (offset + 4 > buf.size())
                return false;
            out = (static_cast<std::uint32_t>(buf[offset]) << 24) |
                  (static_cast<std::uint32_t>(buf[offset + 1]) << 16) |
                  (static_cast<std::uint32_t>(buf[offset + 2]) << 8) |
                  static_cast<std::uint32_t>(buf[offset + 3]);
            offset += 4;
            return true;
        }

        static bool readI32(const std::vector<std::uint8_t> &buf, std::size_t &offset, std::int32_t &out)
        {
            std::uint32_t raw = 0;
            if (!readU32(buf, offset, raw))
                return false;
            out = static_cast<std::int32_t>(raw);
            return true;
        }

        static bool readU64(const std::vector<std::uint8_t> &buf, std::size_t &offset, std::uint64_t &out)
        {
            if (offset + 8 > buf.size())
                return false;
            out = 0;
            for (int i = 0; i < 8; ++i)
            {
                out = (out << 8) | static_cast<std::uint64_t>(buf[offset + i]);
            }
            offset += 8;
            return true;
        }

        static bool readString(const std::vector<std::uint8_t> &buf,
                               std::size_t &offset,
                               std::string &out)
        {
            std::uint32_t length = 0;
            if (!readU32(buf, offset, length) ||
                offset + length > buf.size())
            {
                return false;
            }
            out.assign(buf.begin() + offset,
                       buf.begin() + offset + length);
            offset += length;
            return true;
        }
    };

        // ---- Header + payload -> full frame ready to send ----

        static std::vector<std::uint8_t> buildFrame(NetworkMessageType type,
                                                    const std::vector<std::uint8_t> &payload)
        {
            std::vector<std::uint8_t> frame;
            frame.reserve(kHeaderSize + payload.size());

            writeU8(frame, static_cast<std::uint8_t>(type));
            writeU32(frame, static_cast<std::uint32_t>(payload.size()));
            frame.insert(frame.end(), payload.begin(), payload.end());

            return frame;
        }

        // ---- NetworkMovePacket ----

        static std::vector<std::uint8_t> serializeMovePacket(const NetworkMovePacket &packet)
        {
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
        static std::optional<NetworkMovePacket> deserializeMovePacket(const std::vector<std::uint8_t> &buf)
        {
            if (buf.size() != kMovePacketWireSize)
            {
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

            if (!ok)
                return std::nullopt;
            return packet;
        }

        // Translates a network packet into an engine action request.
        // Note: in the Position constructor, the first parameter is row (y) and
        // the second is col (x).
        static ActionRequest deserializeToRequest(const NetworkMovePacket &packet)
        {
            Position from(packet.from.y, packet.from.x);
            Position to(packet.to.y, packet.to.x);

            PlayerAction action(from, to);

            return ActionRequest(
                packet.requestId,
                static_cast<PlayerColor>(packet.playerColor),
                action);
        }

        // ---- ActionResult ----
        // See the note at the top of this file regarding the shared-ABI assumption.

        static std::vector<std::uint8_t> serializeActionResult(const ActionResult &result)
        {
            std::vector<std::uint8_t> buf(sizeof(ActionResult));
            std::memcpy(buf.data(), &result, sizeof(ActionResult));
            return buf;
        }

        static std::optional<ActionResult> deserializeToResult(const std::vector<std::uint8_t> &buffer)
        {
            if (buffer.size() < sizeof(ActionResult))
            {
                return std::nullopt;
            }
            ActionResult result(0, ActionStatus::Rejected);
            std::memcpy(&result, buffer.data(), sizeof(ActionResult));
            return result;
        }

        static std::vector<std::uint8_t> serializeAuthRequest(const std::string &username, const std::string &password)
        {
            std::vector<std::uint8_t> buf;
            writeString(buf, username);
            writeString(buf, password);
            return buf;
        }

        static bool deserializeAuthRequest(const std::vector<std::uint8_t> &buf, std::string &outUsername, std::string &outPassword)
        {
            std::size_t offset = 0;
            return readString(buf, offset, outUsername) &&
                   readString(buf, offset, outPassword);
        }

} // namespace kungfu