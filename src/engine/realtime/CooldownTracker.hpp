// engine/realtime/CooldownTracker.hpp
#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include "engine/snapshot/MatchStateSnapshot.hpp"

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

    // Exports active cooldowns using relative remaining milliseconds for server-clock independence
    std::vector<CooldownSnapshot> exportSnapshot(int currentTimeMs) const {
        std::vector<CooldownSnapshot> snapshots;
        snapshots.reserve(cooldowns_.size());

        for (const auto& [pieceId, expiresAtMs] : cooldowns_) {
            int remaining = expiresAtMs - currentTimeMs;
            if (remaining > 0) {
                snapshots.push_back({pieceId, remaining});
            }
        }
        return snapshots;
    }

    // Restores active cooldowns relative to the new server clock
    void restoreSnapshot(const std::vector<CooldownSnapshot>& snapshots, int currentTimeMs) noexcept {
        cooldowns_.clear();
        for (const auto& snap : snapshots) {
            if (snap.remainingMs > 0) {
                cooldowns_[snap.pieceId] = currentTimeMs + snap.remainingMs;
            }
        }
    }

private:
    std::unordered_map<std::uint64_t, int> cooldowns_;
};

}  // namespace kungfu