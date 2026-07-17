#include "players/human/Controller.hpp"
#include "engine/core/IGameEngine.hpp"

namespace kungfu {

Controller::Controller(std::shared_ptr<IGameEngine> engine, int cellSize)
    : engine_(std::move(engine)), mapper_(cellSize), selectedPosition_(std::nullopt) {}

ControllerResult Controller::click(int x, int y) {
    ControllerResult result{false, "", std::nullopt, std::nullopt, std::nullopt};

    if (!engine_) {
        result.description = "Engine reference is null";
        return result;
    }

    int rows = engine_->getBoardRows();
    int cols = engine_->getBoardCols();
    auto cellOpt = mapper_.pixelToCell(x, y, rows, cols);

    // 1. קליק מחוץ לגבולות הלוח - התעלמות מוחלטת (אינו מבטל סימון קיים)
    if (!cellOpt.has_value()) {
        result.description = "Click outside board ignored";
        return result;
    }

    Position targetCell = cellOpt.value();

    // 2. קליק בתוך הלוח כאשר כבר קיים כלי מסומן
    if (selectedPosition_.has_value()) {
        Position from = selectedPosition_.value();

        // תיקון: selectedPosition_ הוא מיקום בלבד, לא זהות כלי. בזמן-אמת,
        // בין הקליק שבחר את הכלי לבין הקליק הזה, מישהו אחר (כמעט תמיד כלי
        // יריב שתפס את הכלי שנבחר - "נחת" בדיוק על אותה משבצת) יכול לתפוס
        // את from. בלי הבדיקה הזו, requestMove(from, targetCell) היה מזהה
        // מחדש איזה כלי נמצא שם *עכשיו* ומפעיל תנועה עליו - ולא בהכרח על
        // הכלי שהמשתמש התכוון אליו.
        //
        // הבדיקה: הצבע של הכלי שעדיין יושב ב-from חייב להיות תואם לצבע
        // שנשמר בזמן הבחירה. אם לא (או שאין שם כלי בכלל) - הבחירה מיושנת.
        auto currentColorAtFrom = engine_->getPieceColorAt(from);
        bool selectionStillValid = currentColorAtFrom.has_value() &&
                                    selectedColor_.has_value() &&
                                    currentColorAtFrom.value() == selectedColor_.value();

        if (!selectionStillValid) {
            clearSelection();

            // מטפלים בקליק הנוכחי כאילו הוא קליק ראשון - ייתכן שהוא בחירה
            // חדשה ולגיטימית של כלי אחר, ואין סיבה "לבלוע" אותו.
            if (engine_->hasPieceAt(targetCell)) {
                selectedPosition_ = targetCell;
                selectedColor_ = engine_->getPieceColorAt(targetCell);
                result.actionTaken = true;
                result.description = "Stale selection cleared; new piece selected";
                return result;
            }
            result.description = "Stale selection cleared";
            return result;
        }

        // אם נלחץ על אותה משבצת שבה כבר יש כלי מסומן, ביטול הבחירה מונע
        // שליחה אוטומטית של בקשת jump/קפיצה ללא כוונה של המשתמש.
        if (from == targetCell) {
            clearSelection();
            result.actionTaken = true;
            result.description = "Selection cleared";
            return result;
        }

        // מחליפים את הסימון רק אם לחצנו על כלי ידידותי אחר
        if (engine_->hasPieceAt(targetCell)) {
            auto targetColor = engine_->getPieceColorAt(targetCell);
            if (targetColor.has_value() && targetColor.value() == selectedColor_.value()) {
                selectedPosition_ = targetCell;
                selectedColor_ = targetColor;
                result.actionTaken = true;
                result.description = "Piece selection replaced";
                return result;
            }
        }

        // שליחת בקשת התנועה למנוע המשחק; האימות/ביצוע יבוצעו מאוחר יותר דרך GameEngine
        auto moveResult = engine_->requestMove(from, targetCell);
        auto selectedColor = selectedColor_;
        clearSelection();

        result.actionTaken = true;
        result.from = from;
        result.to = targetCell;
        result.playerColor = selectedColor;

        if (moveResult.isAccepted) {
            result.description = "Move requested: " + moveResult.reason;
        } else {
            result.description = "Move rejected: " + moveResult.reason;
        }
        return result;
    }

    // 3. קליק בתוך הלוח ללא כלי מסומן כרגע (קליק ראשון)
    if (engine_->hasPieceAt(targetCell)) {
        selectedPosition_ = targetCell;
        selectedColor_ = engine_->getPieceColorAt(targetCell);
        result.actionTaken = true;
        result.description = "Piece selected";
        return result;
    }

    // התעלמות מקליק ראשון על משבצת ריקה
    result.description = "Click on empty cell ignored";
    return result;
}

std::optional<Position> Controller::selectedPosition() const noexcept {
    return selectedPosition_;
}

void Controller::clearSelection() noexcept {
    selectedPosition_ = std::nullopt;
    selectedColor_ = std::nullopt;
}

}  // namespace kungfu