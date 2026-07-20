// ui/framework/InputEvents.hpp
#pragma once
#include <cstdint>

// קואורדינטות דו-מימדיות גנריות
struct Vector2D {
    float x = 0.0f;
    float y = 0.0f;
};

// צבע גנרי בפורמט RGBA
struct Color {
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;
};

// הגדרת מקשים גנריים מורחבת לתמיכה בהקלדה
enum class Key {
    Unknown,
    Escape,
    Space,
    W, S, A, D,
    Left, Right,
    Backspace,
    Enter,
    Tab
};

// הגדרת כפתורי עכבר
enum class MouseButton {
    None,
    Left,
    Right,
    Middle
};

// נתונים עבור אירועי מקלדת
struct KeyEvent {
    Key key = Key::Unknown;
    int rawCode = 0; 
};

// נתונים עבור אירועי עכבר
struct MouseEvent {
    enum class Action { Press, Release, Move };
    Action action;
    MouseButton button;
    float logicalX;
    float logicalY;
};

// אירוע קלט מאוחד
struct InputEvent {
    enum class Type { Mouse, Keyboard };
    Type type;
    MouseEvent mouse;
    KeyEvent key;
};