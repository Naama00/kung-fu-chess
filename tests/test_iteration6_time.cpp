#include "../core/include/GameEngine.hpp"
#include "../core/include/Board.hpp"
#include <cassert>
#include <iostream>

void test_movement_over_time() {
    // 1. אתחול לוח בדיקה בסיסי עם צריח לבן ב-(0,0)
    Board board;
    board.addRow({ "wR", ".", "." });
    board.addRow({ ".", ".", "." });
    
    GameEngine engine;
    engine.initBoard(board);

    // 2. ביצוע מהלך: בחירת הצריח ב-(0,0) ותנועה ל-(0,2)
    // נניח שכל משבצת היא 100x100 פיקסלים כפי שמוגדר ב-handleClick
    engine.handleClick(50, 50);   // לחיצה על (0,0) -> בחירה
    engine.handleClick(250, 50);  // לחיצה על (0,2) -> פקודת תנועה

    // 3. בדיקה מיידית (זמן 0ms): הכלי חייב להישאר במקור
    Board boardAtZero = engine.getRenderedBoard();
    assert(boardAtZero.getTokenAt(0, 0) == "wR");
    assert(boardAtZero.getTokenAt(0, 2) == ".");

    // 4. התקדמות זמן קצרה (500ms): הזמן (1000ms) עדיין לא עבר, הכלי עדיין במקור
    engine.advanceTime(500);
    Board boardAtHalfTime = engine.getRenderedBoard();
    assert(boardAtHalfTime.getTokenAt(0, 0) == "wR");
    assert(boardAtHalfTime.getTokenAt(0, 2) == ".");

    // 5. התקדמות זמן מלאה (עוד 600ms, סה"כ 1100ms): הכלי חייב להגיע ליעד
    engine.advanceTime(600);
    Board boardAtFullTime = engine.getRenderedBoard();
    assert(boardAtFullTime.getTokenAt(0, 0) == ".");
    assert(boardAtFullTime.getTokenAt(0, 2) == "wR");
}

void run_time_tests() {
    test_movement_over_time();
    std::cout << "[✓] All movement-over-time tests passed successfully.\n";
}