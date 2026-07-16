// ממשק זה מגדיר כיצד קלט פיזי מתורגם לאירועים הגנריים של ה-UI במערכת הצירים הלוגית
#pragma once
#include "ui/framework/InputEvents.hpp"
#include <vector>

class IInputTranslator {
public:
    virtual ~IInputTranslator() = default;

    // דגימה ועדכון של אירועי הקלט מהחלון הפיזי באותו פריים
    virtual void pollEvents(std::vector<InputEvent>& outEvents) = 0;

    // פונקציית עזר סטטית לתרגום קואורדינטות פיזיות ללוגיות
    static Vector2D translateToLogical(Vector2D physicalPos, Vector2D windowSize, Vector2D logicalRange) {
        Vector2D logicalPos{0.0f, 0.0f};
        if (windowSize.x > 0.0f && windowSize.y > 0.0f) {
            logicalPos.x = (physicalPos.x / windowSize.x) * logicalRange.x;
            logicalPos.y = (physicalPos.y / windowSize.y) * logicalRange.y;
        }
        return logicalPos;
    }
};