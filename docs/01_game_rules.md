# Game rules and piece movement

This file documents the basic chess rules implemented in the project. These rules form the static logical foundation that does not depend on real time, and they are enforced by the `RuleEngine` component.

## 1. Victory and game-end conditions
* **Physical capture of the king:** in this game there is no concept of "check" or "checkmate" (because the game is simultaneous and a player can abandon their king or move it into danger).
* **Game end:** the game ends immediately once one player reaches the opponent king's square physically and captures it (`capturedKing = true`).

## 2. Piece movement rules
Standard geometric movements are implemented using classes inheriting from `IPieceRule`:

* **Rook:** moves in a straight line (horizontally or vertically) any distance, as long as the path is clear. It is blocked by the first piece encountered (if it is an enemy piece, it can capture it).
* **Bishop:** moves diagonally any distance, as long as the path is clear. It is blocked by the first piece encountered.
* **Queen:** combines the movement rules of the rook and bishop.
* **King:** moves one square in any direction (vertical, horizontal, or diagonal).
* **Pawn:**
   * One step forward (depending on color: white moves upward through the ranks, black moves downward).
  * Two steps forward only as the first move, provided both squares along the way are empty.
  * Captures one square diagonally forward-right or forward-left, but only if an enemy piece is there.
* **Knight:** moves in an "L" shape (two squares in one direction and one square sideways). 
  * *Special feature:* the knight jumps over other pieces on its path and is not blocked by them during movement.

## 3. Pawn promotion
Implemented using the `ChessPromotionRule` class:
* When a pawn reaches the last rank of the board (the highest rank for white, or rank 0 for black), it is automatically replaced by a queen at the same target coordinate.
* תהליך זה מתבצע באופן סינכרוני עם נחיתת הכלי, והכלי החדש (המלכה) מקבל את מזהה הצינון של המהלך.