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

    static std::vector<std::uint8_t> serializeMatchSnapshot(const MatchStateSnapshot &snap)
        {
            std::vector<std::uint8_t> buf;
            buf.reserve(256 + snap.pieces.size() * 18 + snap.activeMotions.size() * 24);

            writeU64(buf, snap.matchId);
            writeI32(buf, snap.currentTimeMs);
            writeU8(buf, snap.gameOver ? 1 : 0);
            writeU8(buf, static_cast<std::uint8_t>(snap.currentTurn));

            writeString(buf, snap.whiteUsername);
            writeString(buf, snap.blackUsername);
            writeU8(buf, snap.isWhiteDisconnected ? 1 : 0);
            writeU8(buf, snap.isBlackDisconnected ? 1 : 0);
            writeI32(buf, snap.reconnectSecondsLeft);

            // 1. Pieces
            writeU32(buf, static_cast<std::uint32_t>(snap.pieces.size()));
            for (const auto &p : snap.pieces) {
                writeU64(buf, p.id);
                writeU8(buf, static_cast<std::uint8_t>(p.type));
                writeU8(buf, static_cast<std::uint8_t>(p.color));
                writeI32(buf, p.position.row());
                writeI32(buf, p.position.col());
                writeU8(buf, static_cast<std::uint8_t>(p.state));
                writeU8(buf, p.hasMoved ? 1 : 0);
            }

            // 2. Active Motions
            writeU32(buf, static_cast<std::uint32_t>(snap.activeMotions.size()));
            for (const auto &m : snap.activeMotions) {
                writeU64(buf, m.pieceId);
                writeI32(buf, m.from.row());
                writeI32(buf, m.from.col());
                writeI32(buf, m.to.row());
                writeI32(buf, m.to.col());
                writeI32(buf, m.elapsedMs);
                writeI32(buf, m.durationMs);
            }

            // 3. Active Cooldowns
            writeU32(buf, static_cast<std::uint32_t>(snap.activeCooldowns.size()));
            for (const auto &c : snap.activeCooldowns) {
                writeU64(buf, c.pieceId);
                writeI32(buf, c.remainingMs);
            }

            // 4. Pending Premoves
            writeU32(buf, static_cast<std::uint32_t>(snap.pendingPremoves.size()));
            for (const auto &pm : snap.pendingPremoves) {
                writeU64(buf, pm.pieceId);
                writeI32(buf, pm.to.row());
                writeI32(buf, pm.to.col());
            }

            return buf;
        }

        static std::optional<MatchStateSnapshot> deserializeMatchSnapshot(const std::vector<std::uint8_t> &buf)
        {
            std::size_t offset = 0;
            MatchStateSnapshot snap;

            std::uint8_t gameOverByte = 0, turnByte = 0;
            std::uint8_t whiteDiscByte = 0, blackDiscByte = 0;

            bool ok = true;
            ok &= readU64(buf, offset, snap.matchId);
            ok &= readI32(buf, offset, snap.currentTimeMs);
            ok &= readU8(buf, offset, gameOverByte);
            ok &= readU8(buf, offset, turnByte);
            snap.gameOver = (gameOverByte != 0);
            snap.currentTurn = static_cast<PlayerColor>(turnByte);

            ok &= readString(buf, offset, snap.whiteUsername);
            ok &= readString(buf, offset, snap.blackUsername);
            ok &= readU8(buf, offset, whiteDiscByte);
            ok &= readU8(buf, offset, blackDiscByte);
            ok &= readI32(buf, offset, snap.reconnectSecondsLeft);
            snap.isWhiteDisconnected = (whiteDiscByte != 0);
            snap.isBlackDisconnected = (blackDiscByte != 0);

            // 1. Pieces
            std::uint32_t pieceCount = 0;
            ok &= readU32(buf, offset, pieceCount);
            if (!ok) return std::nullopt;

            snap.pieces.reserve(pieceCount);
            for (std::uint32_t i = 0; i < pieceCount; ++i) {
                PieceStateSnapshot p{};
                std::uint8_t typeByte = 0, colorByte = 0, stateByte = 0, movedByte = 0;
                std::int32_t row = 0, col = 0;

                ok &= readU64(buf, offset, p.id);
                ok &= readU8(buf, offset, typeByte);
                ok &= readU8(buf, offset, colorByte);
                ok &= readI32(buf, offset, row);
                ok &= readI32(buf, offset, col);
                ok &= readU8(buf, offset, stateByte);
                ok &= readU8(buf, offset, movedByte);

                p.type = static_cast<PieceType>(typeByte);
                p.color = static_cast<PlayerColor>(colorByte);
                p.position = Position(row, col);
                p.state = static_cast<PieceState>(stateByte);
                p.hasMoved = (movedByte != 0);

                snap.pieces.push_back(p);
            }

            // 2. Active Motions
            std::uint32_t motionCount = 0;
            ok &= readU32(buf, offset, motionCount);
            if (!ok) return std::nullopt;

            snap.activeMotions.reserve(motionCount);
            for (std::uint32_t i = 0; i < motionCount; ++i) {
                MotionSnapshot m{};
                std::int32_t fromRow = 0, fromCol = 0, toRow = 0, toCol = 0;

                ok &= readU64(buf, offset, m.pieceId);
                ok &= readI32(buf, offset, fromRow);
                ok &= readI32(buf, offset, fromCol);
                ok &= readI32(buf, offset, toRow);
                ok &= readI32(buf, offset, toCol);
                ok &= readI32(buf, offset, m.elapsedMs);
                ok &= readI32(buf, offset, m.durationMs);

                m.from = Position(fromRow, fromCol);
                m.to = Position(toRow, toCol);

                snap.activeMotions.push_back(m);
            }

            // 3. Active Cooldowns
            std::uint32_t cooldownCount = 0;
            ok &= readU32(buf, offset, cooldownCount);
            if (!ok) return std::nullopt;

            snap.activeCooldowns.reserve(cooldownCount);
            for (std::uint32_t i = 0; i < cooldownCount; ++i) {
                CooldownSnapshot c{};
                ok &= readU64(buf, offset, c.pieceId);
                ok &= readI32(buf, offset, c.remainingMs);
                snap.activeCooldowns.push_back(c);
            }

            // 4. Pending Premoves
            std::uint32_t premoveCount = 0;
            ok &= readU32(buf, offset, premoveCount);
            if (!ok) return std::nullopt;

            snap.pendingPremoves.reserve(premoveCount);
            for (std::uint32_t i = 0; i < premoveCount; ++i) {
                PremoveSnapshot pm{};
                std::int32_t toRow = 0, toCol = 0;

                ok &= readU64(buf, offset, pm.pieceId);
                ok &= readI32(buf, offset, toRow);
                ok &= readI32(buf, offset, toCol);
                pm.to = Position(toRow, toCol);

                snap.pendingPremoves.push_back(pm);
            }

            if (!ok) return std::nullopt;
            return snap;
        }
        
} // namespace kungfu
