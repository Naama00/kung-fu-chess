#pragma once

#include <cstdint>
#include <unordered_map>

namespace kungfu {

// אחראית בלעדית על מעקב אחר זמני צינון (cooldown) של כלים, לפי מזהה יציב (piece->id()).
// אינה יודעת דבר על תנועה, לוח, או כללי משחק - רק "מתי כל כלי חופשי לפעול שוב".
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

    size_t entryCount() const noexcept {
        return cooldowns_.size();
    }

private:
    std::unordered_map<std::uint64_t, int> cooldowns_;
};

}  // namespace kungfu