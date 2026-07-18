// תפקיד: המימוש הקונקרטי של מנתח האיומים. הוא מספק שתי יכולות מרכזיות:
// isThreatened: בדיקה מהירה האם משבצת יעד מסוימת מאוימת כרגע על ידי לפחות כלי אחד של היריב.
// getThreateningPieces: החזרת רשימה מפורטת של כל הכלים הספציפיים של היריב המאיימים על משבצת נתונה.
// כיצד הוא פועל: הוא עושה שימוש חוזר ב-MoveGenerator
// כדי להפיק את כל המהלכים הפוטנציאליים של היריב ולבדוק האם אחד מהם מסתיים במשבצת היעד המבוקשת.
#pragma once

#include "engine/analysis/IThreatAnalyzer.hpp"
#include "engine/analysis/MoveGenerator.hpp"

namespace kungfu {

class ThreatAnalyzer : public IThreatAnalyzer {
public:
    ThreatAnalyzer() = default;
    ~ThreatAnalyzer() override = default;

    std::vector<view::PieceSnapshot> getThreateningPieces(
        const view::GameSnapshot& snapshot,
        const Position& targetPosition,
        PlayerColor defenderColor) const override;

    bool isThreatened(
        const view::GameSnapshot& snapshot,
        const Position& targetPosition,
        PlayerColor defenderColor) const override;
};

} // namespace kungfu