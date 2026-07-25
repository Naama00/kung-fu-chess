// server/network/IConnection.hpp
#pragma once

#include <cstdint>
#include <vector>
#include "NetworkMessages.hpp"

namespace kungfu {

// Abstract send-endpoint for a single transport channel.
//
// PlayerSession talks to this interface only - it never touches a raw
// tcp::socket or udp::socket directly. That is what lets the same business
// logic (PlayerSession, MatchManager, LiveMatch) work unchanged whether a
// given message physically goes out over TCP or UDP; see channelFor() in
// NetworkMessages.hpp for how that routing decision is made.
class IConnection {
public:
    virtual ~IConnection() = default;

    // Frames and sends 'payload' as a message of the given type. Delivery is
    // best-effort for realtime (UDP) connections and reliable/ordered for
    // control (TCP) connections - callers should not assume either.
    virtual void send(NetworkMessageType type, const std::vector<std::uint8_t>& payload) = 0;
};

} // namespace kungfu
