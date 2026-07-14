// קובץ זה מגדיר את אירועי המקלדת והעכבר הלוגיים בצורה מנותקת לחלוטין מהספרייה הגרפית
#pragma once

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

// הגדרת מקשים גנריים התואמת את השימוש ב-Translator וב-main
enum class Key {
    Unknown,
    Escape,
    Space,
    W, S, A, D,
    Left, Right
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
    Key key;
    int rawCode;
};

// נתונים עבור אירועי עכבר - משתמשים בקואורדינטות לוגיות בלבד
struct MouseEvent {
    enum class Action { Press, Release, Move };
    Action action;
    MouseButton button;
    float logicalX;
    float logicalY;
};

// אירוע קלט מאוחד המאפשר גישה נוחה וישירה לשדות
struct InputEvent {
    enum class Type { Mouse, Keyboard };
    Type type;
    MouseEvent mouse;
    KeyEvent key;
};