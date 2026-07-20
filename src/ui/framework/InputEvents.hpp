// ui/framework/InputEvents.hpp
#pragma once
#include <cstdint>

// Generic two-dimensional coordinates
struct Vector2D {
    float x = 0.0f;
    float y = 0.0f;
};

// Generic color in RGBA format
struct Color {
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;
};

// Definition of generic keys extended to support typing
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

// Definition of mouse buttons
enum class MouseButton {
    None,
    Left,
    Right,
    Middle
};

// Data for keyboard events
struct KeyEvent {
    Key key = Key::Unknown;
    int rawCode = 0; 
};

// Data for mouse events
struct MouseEvent {
    enum class Action { Press, Release, Move };
    Action action;
    MouseButton button;
    float logicalX;
    float logicalY;
};

// Unified input event
struct InputEvent {
    enum class Type { Mouse, Keyboard };
    Type type;
    MouseEvent mouse;
    KeyEvent key;
};