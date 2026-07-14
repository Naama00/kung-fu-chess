// קובץ זה מגדיר את אירועי המקלדת והעכבר הלוגיים בצורה מנותקת לחלוטין מהספרייה הגרפית
#pragma once
#include <variant>

// קואורדינטות דו-מימדיות גנריות
struct Vector2D {
    float x = 0.0f;
    float y = 0.0f;
};

// צבע גנרי בפורמט RGBA
struct Color {
    unsigned char r = 255;
    unsigned char g = 255;
    unsigned char b = 255;
    unsigned char a = 255;
};

// הגדרת מקשים נפוצים לשימוש המשחק
enum class KeyCode {
    Unknown,
    Escape,
    Space,
    Enter,
    Left, Right, Up, Down,
    A, B, C, D, E, F, G, H,
    Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8
};

// הגדרת כפתורי עכבר
enum class MouseButton {
    Left,
    Right,
    Middle
};

// נתונים עבור אירועי מקלדת
struct KeyEvent {
    KeyCode code;
    bool pressed; // true = לחיצה, false = שחרור
};

// נתונים עבור אירועי עכבר - משתמשים בקואורדינטות לוגיות בלבד
struct MouseEvent {
    enum class Action { Press, Release, Move };
    Action action;
    MouseButton button;
    float logicalX;
    float logicalY;
};

// איחוד המייצג אירוע קלט כלשהו
using InputEvent = std::variant<KeyEvent, MouseEvent>;