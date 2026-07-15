#include "core/input/Controller.hpp"
#include "core/engine/IGameEngine.hpp"

namespace kungfu {

Controller::Controller(std::shared_ptr<IGameEngine> engine, int cellSize)
    : engine_(std::move(engine)), mapper_(cellSize), selectedPosition_(std::nullopt) {}

ControllerResult Controller::click(int x, int y) {
    if (!engine_) {
        return {false, "Engine reference is null"};
    }

    int rows = engine_->getBoardRows();
    int cols = engine_->getBoardCols();
    auto cellOpt = mapper_.pixelToCell(x, y, rows, cols);

    // 1. קליק מחוץ לגבולות הלוח - התעלמות מוחלטת (אינו מבטל סימון קיים)
    if (!cellOpt.has_value()) {
        return {false, "Click outside board ignored"};
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
                return {true, "Stale selection cleared; new piece selected"};
            }
            return {false, "Stale selection cleared"};
        }

        // מחליפים את הסימון רק אם לחצנו על כלי ידידותי אחר
        if (from != targetCell && engine_->hasPieceAt(targetCell)) {
            auto targetColor = engine_->getPieceColorAt(targetCell);
            if (targetColor.has_value() && targetColor.value() == selectedColor_.value()) {
                selectedPosition_ = targetCell;
                selectedColor_ = targetColor;
                return {true, "Piece selection replaced"};
            }
        }

        // שליחת בקשת התנועה למנוע המשחק (כעת, לחיצה שנייה על אותו כלי תגיע לכאן ותפעיל קפיצה!)
        auto moveResult = engine_->requestMove(from, targetCell);
        
        // פינוי הסימון באופן מיידי לאחר הניסיון
        clearSelection();

        if (moveResult.isAccepted) {
            return {true, "Move requested: " + moveResult.reason};
        } else {
            return {true, "Move rejected: " + moveResult.reason};
        }
    }

    // 3. קליק בתוך הלוח ללא כלי מסומן כרגע (קליק ראשון)
    if (engine_->hasPieceAt(targetCell)) {
        selectedPosition_ = targetCell;
        selectedColor_ = engine_->getPieceColorAt(targetCell);
        return {true, "Piece selected"};
    }

    // התעלמות מקליק ראשון על משבצת ריקה
    return {false, "Click on empty cell ignored"};
}

std::optional<Position> Controller::selectedPosition() const noexcept {
    return selectedPosition_;
}

void Controller::clearSelection() noexcept {
    selectedPosition_ = std::nullopt;
    selectedColor_ = std::nullopt;
}

}  // namespace kungfu