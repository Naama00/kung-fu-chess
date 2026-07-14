#pragma once

#include <cstdint>
#include <unordered_map>

namespace kungfu {

class CooldownTracker {
public:
    void setCooldown(std::uint64_t pieceId, int expiresAtMs) noexcept {
        cooldowns_[pieceId] = expiresAtMs;
    }

    void clear(std::uint64_t pieceId) noexcept {
        cooldowns_.erase(pieceId);
    }

    bool isOnCooldown(std::uint64_t pieceId, int currentTimeMs) const noexcept {
        auto it = cooldowns_.find(pieceId);
        if (it == cooldowns_.end()) {
            return false;
        }
        return currentTimeMs < it->second;
    }

    // מתודה חדשה לצורך שליפת זמן סיום הצינון
    int getExpiration(std::uint64_t pieceId) const noexcept {
        auto it = cooldowns_.find(pieceId);
        if (it == cooldowns_.end()) {
            return 0;
        }
        return it->second;
    }

    size_t entryCount() const noexcept {
        return cooldowns_.size();
    }

private:
    std::unordered_map<std::uint64_t, int> cooldowns_;
};

}  // namespace kungfu