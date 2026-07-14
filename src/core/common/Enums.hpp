#pragma once

namespace kungfu {

enum class PieceType {
    King,
    Queen,
    Rook,
    Bishop,
    Knight,
    Pawn
};

enum class PlayerColor {
    White,
    Black
};

enum class PieceState {
    Idle,       // על הלוח, אינו בתנועה
    Moving,     // נמצא בדרך ליעד
    Captured,   // הוסר מהמשחק
    Airborne    // כלי שנמצא כרגע בעיצומה של קפיצה באוויר
};

}  // namespace kungfu