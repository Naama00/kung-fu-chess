#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "NetworkMessages.hpp"

namespace kungfu {

class NetworkSession;

std::string networkSessionUsername(const std::shared_ptr<NetworkSession>& session);
void networkSessionSetMatchId(const std::shared_ptr<NetworkSession>& session, std::uint64_t id);
void networkSessionSetColor(const std::shared_ptr<NetworkSession>& session, PlayerColor color);
void networkSessionSendPacket(const std::shared_ptr<NetworkSession>& session,
                              NetworkMessageType type,
                              const std::vector<std::uint8_t>& payload);

} // namespace kungfu
