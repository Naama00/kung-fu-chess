// This interface defines how physical input is translated into generic UI events in the logical coordinate system
#pragma once
#include "ui/framework/InputEvents.hpp"
#include <vector>

class IInputTranslator {
public:
    virtual ~IInputTranslator() = default;

    // Sample and update input events from the physical window in the same frame
    virtual void pollEvents(std::vector<InputEvent>& outEvents) = 0;

    // Static helper function for translating physical coordinates to logical ones
    static Vector2D translateToLogical(Vector2D physicalPos, Vector2D windowSize, Vector2D logicalRange) {
        Vector2D logicalPos{0.0f, 0.0f};
        if (windowSize.x > 0.0f && windowSize.y > 0.0f) {
            logicalPos.x = (physicalPos.x / windowSize.x) * logicalRange.x;
            logicalPos.y = (physicalPos.y / windowSize.y) * logicalRange.y;
        }
        return logicalPos;
    }
};