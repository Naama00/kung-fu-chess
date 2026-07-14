#pragma once

#include "ui/framework/IScreen.hpp"
#include "ui/framework/ScreenManager.hpp"
#include "ui/components/CooldownBar.hpp"
#include "ui/components/Button.hpp" // הוספת ייבוא של Button
#include "core/engine/GameEngine.hpp"
#include "core/input/Controller.hpp"
#include "core/view/SnapshotBuilder.hpp"
#include "core/io/BoardParser.hpp"
#include "core/common/PieceTokenCodec.hpp"
#include <memory>
#include <iostream>
#include <algorithm>

class ChessGameScreen : public IScreen {
private:
    std::shared_ptr<kungfu::GameEngine> m_gameEngine;
    std::shared_ptr<kungfu::Controller> m_controller;
    CooldownBar m_cooldownBar;
    kungfu::GameConfig m_config; // שמירת הקונפיגורציה המקורית לצורך Reset
    
    // כפתורי ה-Overlay למסך סוף משחק
    std::unique_ptr<Button> m_rematchButton;
    std::unique_ptr<Button> m_menuButton;
    
    float m_logicalRangeX = 1000.0f;
    float m_logicalRangeY = 1000.0f;

    // מתודת עזר לאתחול או איפוס מלא של לוח השחמט
    void resetGame() {
        std::string startBoard = 
            "wR wN wB wQ wK wB wN wR\n"
            "wP wP wP wP wP wP wP wP\n"
            ". . . . . . . .\n"
            ". . . . . . . .\n"
            ". . . . . . . .\n"
            ". . . . . . . .\n"
            "bP bP bP bP bP bP bP bP\n"
            "bR bN bB bQ bK bB bN bR\n";

        auto board = kungfu::BoardParser::parse(startBoard);
        auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
        
        m_gameEngine = std::make_shared<kungfu::GameEngine>(board, ruleEngine, m_config);
        m_controller = std::make_shared<kungfu::Controller>(m_gameEngine);
        
        int cols = board->cols();
        int logicalCellSize = static_cast<int>(m_logicalRangeX / cols);
        m_controller->setCellSize(logicalCellSize);
    }

public:
    explicit ChessGameScreen(ScreenManager& manager, kungfu::GameConfig config = kungfu::GameConfig{}) 
        : IScreen(manager), m_config(config) 
    {
        // 1. טעינת לוח ואתחול המשחק
        resetGame();

        // 2. אתחול כפתורי פאנל סוף המשחק
        m_rematchButton = std::make_unique<Button>(
            Vector2D{260.0f, 520.0f}, Vector2D{220.0f, 60.0f}, "Rematch", 
            [this]() { resetGame(); }
        );
        m_menuButton = std::make_unique<Button>(
            Vector2D{520.0f, 520.0f}, Vector2D{220.0f, 60.0f}, "Main Menu", 
            [this]() { m_screenManager.popScreen(); } // חוזר למסך הפתיחה
        );

        // הגדרת עיצוב ייחודי לכפתורי סוף המשחק
        m_rematchButton->setColors({40, 110, 75, 255}, {55, 140, 95, 255}, {255, 255, 255, 255}); // ירוק
        m_menuButton->setColors({50, 50, 60, 255}, {70, 70, 85, 255}, {255, 255, 255, 255});      // אפור כהה
    }

    void onEnter() override {
        std::cout << "Chess Game Screen Activated!" << std::endl;
    }

    void onExit() override {
        std::cout << "Chess Game Screen Deactivated!" << std::endl;
    }

    void update(float deltaTime) override {
        int ms = static_cast<int>(deltaTime * 1000.0f);
        m_gameEngine->wait(ms);

        // עדכון מצב הכפתורים של מסך סוף המשחק
        if (m_gameEngine->isGameOver()) {
            m_rematchButton->update(deltaTime);
            m_menuButton->update(deltaTime);
        }
    }

    void draw(IRenderer& renderer) override {
        renderer.clear({30, 30, 40, 255});

        auto board = m_gameEngine->getBoard();
        int rows = board->rows();
        int cols = board->cols();
        
        float cellWidth = m_logicalRangeX / cols;
        float cellHeight = m_logicalRangeY / rows;

        // ציור הלוח
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                Vector2D pos{c * cellWidth, r * cellHeight};
                Vector2D size{cellWidth, cellHeight};
                
                Color cellColor = ((r + c) % 2 == 0) 
                    ? Color{240, 217, 181, 255}
                    : Color{181, 136, 99, 255};
                
                renderer.drawRectangle(pos, size, cellColor, true);
            }
        }

        // סימון משבצת נבחרת
        auto selectedOpt = m_controller->selectedPosition();
        if (selectedOpt.has_value()) {
            Vector2D pos{selectedOpt->col() * cellWidth, selectedOpt->row() * cellHeight};
            Vector2D size{cellWidth, cellHeight};
            renderer.drawRectangle(pos, size, {255, 255, 0, 80}, true);
        }

        // בניית Snapshot וציור כל הכלים
        auto snapshot = kungfu::view::SnapshotBuilder::build(
            *board,
            m_gameEngine->getArbiter(),
            m_gameEngine->getCurrentTimeMs(),
            m_gameEngine->isGameOver(),
            selectedOpt,
            cellWidth
        );

        // 5. ציור הכלים ומדי הצינון (Cooldowns)
        for (const auto& pieceSnap : snapshot.pieces) {
            // א. זיהוי ה-AssetId של הכלי (למשל "wP" עבור רגלי לבן, "bK" עבור מלך שחור)
            std::string assetId = (pieceSnap.color == kungfu::PlayerColor::White) ? "w" : "b";
            assetId += kungfu::PieceTokenCodec::toChar(pieceSnap.type);

            // ב. חישוב מיקום וגודל ה-Sprite של הכלי
            Vector2D spritePos{pieceSnap.pixelX, pieceSnap.pixelY};
            Vector2D spriteSize{cellWidth, cellHeight};

            // ג. ציור ה-Sprite של הכלי באמצעות ה-Renderer האבסטרקטי
            renderer.drawSprite(assetId, spritePos, spriteSize);

            // ד. זיהוי וציור מד הצינון (Cooldown) במידה והכלי בטעינה
            auto boardPieces = board->pieces();
            for (const auto& p : boardPieces) {
                if (p && p->position() == pieceSnap.logicalPosition) {
                    int currentMs = m_gameEngine->getCurrentTimeMs();
                    float progress = m_gameEngine->getArbiter().getCooldownProgress(
                        std::const_pointer_cast<kungfu::Piece>(p), 
                        currentMs
                    );
                    if (progress > 0.0f) {
                        // חישוב המרכז לצורך מירכוז מד הצינון מעט מתחת לכלי
                        Vector2D center{pieceSnap.pixelX + cellWidth / 2.0f, pieceSnap.pixelY + cellHeight / 2.0f};
                        m_cooldownBar.draw(renderer, center, progress); 
                    }
                }
            }
        }
        // 5.5. ציור חיווי ויזואלי של מהלכים מתוכננים מראש (Premoves)
        const auto& premoveQueue = m_gameEngine->getPremoveQueue();
        for (const auto& entry : premoveQueue.entries()) {
            auto piece = entry.first;
            auto to = entry.second; 
            if (!piece) continue;

            // א. חישוב מיקום פיזיקלי של משבצת היעד המתוכננת
            float destX = to.col() * cellWidth;
            float destY = to.row() * cellHeight;

            // ב. ציור ריבוע הדגשה כחול שקוף על משבצת היעד
            renderer.drawRectangle({destX, destY}, {cellWidth, cellHeight}, {0, 120, 255, 45}, true);   // מילוי כחול שקוף
            renderer.drawRectangle({destX, destY}, {cellWidth, cellHeight}, {0, 120, 255, 180}, false);  // מסגרת כחולה ברורה

            // ג. חישוב נקודת ההתחלה של הקו המקשר (מתוך המרכז הפיזי החלק של הכלי)
            Vector2D startPt{0.0f, 0.0f};
            bool foundSnap = false;

            // נחפש את המיקום הפיזיקלי המונפש של הכלי מתוך ה-Snapshot
            for (const auto& pieceSnap : snapshot.pieces) {
                if (pieceSnap.logicalPosition == piece->position() && 
                    pieceSnap.type == piece->type() && 
                    pieceSnap.color == piece->color()) {
                    startPt.x = pieceSnap.pixelX + cellWidth / 2.0f;
                    startPt.y = pieceSnap.pixelY + cellHeight / 2.0f;
                    foundSnap = true;
                    break;
                }
            }

            // במקרה גיבוי (Fallback) אם הכלי לא ב-Snapshot, נשתמש במיקום הלוגי הסטטי
            if (!foundSnap) {
                startPt.x = piece->position().col() * cellWidth + cellWidth / 2.0f;
                startPt.y = piece->position().row() * cellHeight + cellHeight / 2.0f;
            }

            // ד. חישוב נקודת הסוף של הקו (מרכז משבצת היעד)
            Vector2D endPt{destX + cellWidth / 2.0f, destY + cellHeight / 2.0f};

            // ה. מתיחת קו מקשר כחול ומרשים בעובי 3 פיקסלים
            renderer.drawLine(startPt, endPt, {0, 120, 255, 180}, 3.0f);
        }
        // 6. חיווי ופאנל סוף משחק (Game Over Modal overlay)
        if (snapshot.isGameOver) {
            // ציור פאנל רקע מרובע כהה וחצי שקוף במרכז המסך
            renderer.drawRectangle({0.0f, 320.0f}, {1000.0f, 320.0f}, {15, 15, 20, 220}, true);
            renderer.drawRectangle({0.0f, 320.0f}, {1000.0f, 320.0f}, {120, 40, 40, 255}, false); // מסגרת אדומה דקה
            
            renderer.drawText("GAME OVER", {330.0f, 420.0f}, 54, {255, 60, 60, 255});
            
            // ציור כפתורי ה-Overlay האינטראקטיביים
            m_rematchButton->draw(renderer);
            m_menuButton->draw(renderer);
        }
    }

    void handleInput(const std::vector<InputEvent>& events) override {
        for (const auto& event : events) {
            if (event.type == InputEvent::Type::Mouse) {
                const auto& mouse = event.mouse;
                
                // ניתוב קלט לפי מצב המשחק
                if (m_gameEngine->isGameOver()) {
                    // בסיום משחק, העכבר שולט אך ורק על כפתורי ה-Overlay
                    m_rematchButton->handleInput(mouse);
                    m_menuButton->handleInput(mouse);
                } else {
                    if (mouse.action == MouseEvent::Action::Press && mouse.button == MouseButton::Left) {
                        m_controller->click(static_cast<int>(mouse.logicalX), static_cast<int>(mouse.logicalY));
                    }
                }
            }
            else if (event.type == InputEvent::Type::Keyboard) {
                if (event.key.key == Key::Escape) {
                    m_screenManager.popScreen(); // חזרה לתפריט
                }
            }
        }
    }
};