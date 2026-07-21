// ui/animations/HoverGroup.hpp
#pragma once
#include <string>
#include <unordered_map>
#include <algorithm>
#include "ui/animations/Animation.hpp"

namespace ui {
namespace animation {

/**
 * @brief Tracks a smoothed 0..1 "hover progress" value per named widget id.
 *
 * Screens that draw many buttons/toggles manually (instead of using the
 * Button component, which already tracks its own hover state) can use this
 * to get the same animated hover transition instead of an instant on/off
 * switch, without needing a dedicated member variable per widget.
 *
 * Usage: call update() once per widget per frame (typically from the
 * screen's update()), then read eased() from draw().
 */
class HoverGroup {
public:
    explicit HoverGroup(float speed = 6.0f) noexcept : m_speed(speed) {}

    // Advances the animated progress for 'id' toward 1.0 (hovered) or 0.0 (not).
    void update(const std::string& id, bool isHovered, float deltaTime) {
        float& progress = m_progress[id]; // default-inserts 0.0f if new
        float target = isHovered ? 1.0f : 0.0f;
        float step = m_speed * deltaTime;

        if (progress < target) {
            progress = std::min(target, progress + step);
        } else if (progress > target) {
            progress = std::max(target, progress - step);
        }
    }

    // Returns the eased (smoothstep) progress for a widget; 0 if never updated.
    float eased(const std::string& id) const {
        auto it = m_progress.find(id);
        return Easing::smoothstep(it != m_progress.end() ? it->second : 0.0f);
    }

private:
    float m_speed;
    std::unordered_map<std::string, float> m_progress;
};

} // namespace animation
} // namespace ui