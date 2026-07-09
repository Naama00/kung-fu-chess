#pragma once

#include "engine/GameSnapshot.hpp"

namespace kungfu {

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // פונקציית הרישום הראשית המקבלת את הנתונים לקריאה בלבד ומציירת אותם למסך
    virtual void render(const GameSnapshot& snapshot) = 0;
};

}  // namespace kungfu