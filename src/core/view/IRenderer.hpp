#pragma once

#include "core/view/GameSnapshot.hpp"

namespace kungfu {
namespace view {

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // מקבל תמונת מצב ויזואלית ומצייר אותה למסך
    virtual void render(const GameSnapshot& snapshot) = 0;
};

}  // namespace view
}  // namespace kungfu