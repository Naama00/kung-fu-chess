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

        static void writeString(std::vector<std::uint8_t> &buf, std::string_view str)
        {
            writeU32(buf, static_cast<std::uint32_t>(str.size()));
            if (!str.empty())
            {
                const auto *bytes = reinterpret_cast<const std::uint8_t *>(str.data());
                buf.insert(buf.end(), bytes, bytes + str.size());
            }
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
            if (!readU32(buf, offset, length))
                return false;

            if (length == 0)
            {
                out.clear();
                return true;
            }

            if (offset + length > buf.size())
                return false;

            out.assign(reinterpret_cast<const char *>(buf.data() + offset), length);
            offset += length;
            return true;
        }

        // ---- Header + payload -> full frame ready to send ----

        static std::vector<std::uint8_t> buildFrame(NetworkMessageType type,
                                                    const std::vector<std::uint8_t> &payload)
        {
            std::vector<std::uint8_t> frame;
            frame.reserve(kHeaderSize + payload.size());

            writeU8(frame, static_cast<std::uint8_t>(type));
            writeU32(frame, static_cast<std::uint32_t>(payload.size()));
            if (!payload.empty())
            {
                frame.insert(frame.end(), payload.data(), payload.data() + payload.size());
            }

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

        // ---- MATCH_FOUND Payload Serialization ----
        // Encodes match ID, assigned color, opponent's username, and opponent's Elo rating.
        static std::vector<std::uint8_t> serializeMatchFound(std::uint64_t matchId, std::uint8_t color, std::string_view opponentUser, std::uint32_t opponentElo)
        {
            std::vector<std::uint8_t> buf;
            writeU64(buf, matchId);
            writeU8(buf, color);
            writeString(buf, opponentUser);
            writeU32(buf, opponentElo);
            return buf;
        }

        // ---- LOGIN_RESPONSE Payload Serialization ----
        // success flag (1 byte), followed by [ELO rating (4 bytes) + one-time
        // sessionToken (8 bytes)] only when authentication succeeded. The
        // sessionToken is what the client presents in SESSION_BIND to attach
        // its UDP realtime channel to this TCP-authenticated session.
        static std::vector<std::uint8_t> serializeLoginResponse(bool success, int rating, std::uint64_t sessionToken)
        {
            std::vector<std::uint8_t> buf;
            writeU8(buf, success ? 1 : 0);
            if (success)
            {
                writeU32(buf, static_cast<std::uint32_t>(rating));
                writeU64(buf, sessionToken);
            }
            return buf;
        }

        static bool deserializeLoginResponse(const std::vector<std::uint8_t> &buf, bool &outSuccess, int &outRating, std::uint64_t &outSessionToken)
        {
            std::size_t offset = 0;
            std::uint8_t successByte = 0;
            if (!readU8(buf, offset, successByte))
                return false;

            outSuccess = successByte != 0;
            if (outSuccess)
            {
                std::uint32_t rating = 0;
                if (!readU32(buf, offset, rating) || !readU64(buf, offset, outSessionToken))
                    return false;
                outRating = static_cast<int>(rating);
            }
            return true;
        }

        // ---- SESSION_BIND Payload Serialization ----
        // Carries only the sessionToken issued by LOGIN_RESPONSE, proving to
        // the UDP server that this endpoint belongs to an already
        // TCP-authenticated client.
        static std::vector<std::uint8_t> serializeSessionBind(std::uint64_t sessionToken)
        {
            std::vector<std::uint8_t> buf;
            writeU64(buf, sessionToken);
            return buf;
        }

        static bool deserializeSessionBind(const std::vector<std::uint8_t> &buf, std::uint64_t &outSessionToken)
        {
            std::size_t offset = 0;
            return readU64(buf, offset, outSessionToken);
        }
    };

} // namespace kungfu
