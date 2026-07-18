// engine/actions/PlayerAction.hpp
#pragma once

#include "engine/common/Position.hpp"

namespace kungfu {

// מייצג כוונת שחקן בלבד, אינו משנה מצב משחק.
// PlayerAction מתאר בקשה להזיז כלי ממיקום מקור למיקום יעד.
// האימות (חוקיות, cooldown, סמכות) מתבצע אך ורק ב-GameEngine
// בעת קבלת ה-ActionRequest שעוטף את הפעולה הזו.
struct PlayerAction {
    Position from; // מיקום הכלי בעת הגשת הבקשה
    Position to;   // מיקום היעד המבוקש

    PlayerAction(Position from, Position to)
        : from(from), to(to) {}
};

} // namespace kungfu
