// core/view/screens/ChessGameScreen.hpp

#pragma once

#include "ui/framework/IScreen.hpp"
#include "ui/framework/ScreenManager.hpp"
#include "ui/components/CooldownBar.hpp"
#include "ui/components/Button.hpp" 
#include "core/engine/GameEngine.hpp"
#include "core/input/Controller.hpp"
#include "core/view/SnapshotBuilder.hpp"
#include "core/io/BoardParser.hpp"
#include "core/common/PieceTokenCodec.hpp"
#include "core/engine/MoveHistoryTracker.hpp" 
#include <memory>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>

class ChessGameScreen : public IScreen {
private:
    std::shared_ptr<kungfu::GameEngine> m_gameEngine;
    std::shared_ptr<kungfu::Controller> m_controller;
    std::shared_ptr<kungfu::MoveHistoryTracker> m_historyTracker;
    CooldownBar m_cooldownBar;
    kungfu::GameConfig m_config; 
    std::unique_ptr<Button> m_rematchButton;
    std::unique_ptr<Button> m_menuButton;
    
    float m_logicalRangeX = 1000.0f;
    float m_logicalRangeY = 1000.0f;

    // הגדרות פריסה חדשות ללוח השחמט
    const float m_boardOffsetX = 240.0f;
    const float m_boardOffsetY = 240.0f;
    const float m_boardWidth = 520.0f;
    const float m_boardHeight = 520.0f;

    static int positionKey(const kungfu::Position& pos, int cols) {
        return pos.row() * cols + pos.col();
    }

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
        m_historyTracker = std::make_shared<kungfu::MoveHistoryTracker>();
        m_gameEngine->addObserver(m_historyTracker);
        int cols = board->cols();
        // גודל התא מחושב כעת לפי הגודל הלוגי של הלוח החדש
        int logicalCellSize = static_cast<int>(m_boardWidth / cols);
        m_controller->setCellSize(logicalCellSize);
    }

    // פונקציית עזר לרינדור גנרי של פאנל שחקן וטבלת המהלכים
    void drawPlayerPanel(IRenderer& renderer, 
                         const std::string& username, 
                         float startX, 
                         float startY, 
                         float width, 
                         float height, 
                         const std::vector<kungfu::RecordedMove>& moves, 
                         kungfu::PlayerColor filterColor) {
        
        // 1. ציור קוביית שם השחקן
        Vector2D nameBoxPos{startX, startY};
        Vector2D nameBoxSize{width, 40.0f};
        renderer.drawRectangle(nameBoxPos, nameBoxSize, {60, 60, 65, 255}, true);
        renderer.drawRectangle(nameBoxPos, nameBoxSize, {100, 100, 105, 255}, false);
        
        // כתיבת השם ממורכז אנכית ואופקית בצורה מקורבת
        float textOffset = (width - (static_cast<float>(username.length()) * 10.0f)) / 2.0f;
        renderer.drawText(username, {startX + textOffset, startY + 26.0f}, 18, {255, 255, 255, 255});

        // 2. ציור כותרות הטבלה (Time | Move)
        float tableY = startY + 50.0f;
        Vector2D headerPos{startX, tableY};
        Vector2D headerSize{width, 30.0f};
        renderer.drawRectangle(headerPos, headerSize, {45, 45, 50, 255}, true);
        renderer.drawRectangle(headerPos, headerSize, {80, 80, 85, 255}, false);

        renderer.drawText("Time", {startX + 15.0f, tableY + 21.0f}, 14, {200, 200, 200, 255});
        renderer.drawText("Move", {startX + width / 2.0f + 15.0f, tableY + 21.0f}, 14, {200, 200, 200, 255});

        // 3. ציור הרקע של גוף הטבלה
        float rowY = tableY + 30.0f;
        float rowHeight = 25.0f;
        float bodyHeight = height - 80.0f;
        
        renderer.drawRectangle({startX, rowY}, {width, bodyHeight}, {35, 35, 40, 255}, true);
        renderer.drawRectangle({startX, rowY}, {width, bodyHeight}, {80, 80, 85, 255}, false);

        // קו הפרדה אנכי בין זמן למהלך
        renderer.drawLine({startX + width / 2.0f, rowY}, {startX + width / 2.0f, rowY + bodyHeight}, {80, 80, 85, 255}, 1.0f);

        // סינון המהלכים השייכים לשחקן הספציפי
        std::vector<kungfu::RecordedMove> playerMoves;
        for (const auto& m : moves) {
            if (m.color == filterColor) {
                playerMoves.push_back(m);
            }
        }

        // חישוב מספר השורות המקסימלי שנכנסות בגוף הטבלה
        int maxRowsToFit = static_cast<int>(bodyHeight / rowHeight);
        int startIndex = 0;
        if (playerMoves.size() > static_cast<size_t>(maxRowsToFit)) {
            // גלילה אוטומטית למהלכים האחרונים ביותר
            startIndex = static_cast<int>(playerMoves.size()) - maxRowsToFit;
        }

        int displayRow = 0;
        for (size_t i = startIndex; i < playerMoves.size() && displayRow < maxRowsToFit; ++i, ++displayRow) {
            float currentY = rowY + displayRow * rowHeight;
            
            // שורות בצבעים מתחלפים (Alternating Row Colors)
            if (displayRow % 2 == 1) {
                renderer.drawRectangle({startX + 1.0f, currentY}, {width - 2.0f, rowHeight}, {42, 42, 48, 255}, true);
            }

            // ציור ערכי העמודות
            renderer.drawText(playerMoves[i].timeStr, {startX + 10.0f, currentY + 18.0f}, 12, {240, 240, 240, 255});
            renderer.drawText(playerMoves[i].moveStr, {startX + width / 2.0f + 15.0f, currentY + 18.0f}, 12, {240, 240, 240, 255});

            // קו הפרדה אופקי עדין בין השורות
            renderer.drawLine({startX, currentY + rowHeight}, {startX + width, currentY + rowHeight}, {55, 55, 60, 255}, 1.0f);
        }
    }

public:
    explicit ChessGameScreen(ScreenManager& manager, kungfu::GameConfig config = kungfu::GameConfig{}) 
        : IScreen(manager), m_config(config) 
    {
        resetGame();

        m_rematchButton = std::make_unique<Button>(
            Vector2D{260.0f, 520.0f}, Vector2D{220.0f, 60.0f}, "Rematch", 
            [this]() { resetGame(); }
        );
        m_menuButton = std::make_unique<Button>(
            Vector2D{520.0f, 520.0f}, Vector2D{220.0f, 60.0f}, "Main Menu", 
            [this]() { m_screenManager.popScreen(); } 
        );

        m_rematchButton->setColors({40, 110, 75, 255}, {55, 140, 95, 255}, {255, 255, 255, 255}); 
        m_menuButton->setColors({50, 50, 60, 255}, {70, 70, 85, 255}, {255, 255, 255, 255});      
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
        
        float cellWidth = m_boardWidth / cols;
        float cellHeight = m_boardHeight / rows;

        // 1. ציור קואורדינטות השורות (1-8) והעמודות (a-h)
        for (int c = 0; c < cols; ++c) {
            char letter = 'a' + c;
            std::string label(1, letter);
            float xPos = m_boardOffsetX + c * cellWidth + (cellWidth / 2.0f) - 6.0f;
            renderer.drawText(label, {xPos, m_boardOffsetY - 12.0f}, 16, {180, 180, 180, 255}); // למעלה
            renderer.drawText(label, {xPos, m_boardOffsetY + m_boardHeight + 25.0f}, 16, {180, 180, 180, 255}); // למטה
        }

        for (int r = 0; r < rows; ++r) {
            char number = '1' + r;
            std::string label(1, number);
            float yPos = m_boardOffsetY + r * cellHeight + (cellHeight / 2.0f) + 6.0f;
            renderer.drawText(label, {m_boardOffsetX - 25.0f, yPos}, 16, {180, 180, 180, 255}); // משמאל
            renderer.drawText(label, {m_boardOffsetX + m_boardWidth + 15.0f, yPos}, 16, {180, 180, 180, 255}); // מימין
        }

        // 2. ציור הלוח עצמו
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                Vector2D pos{c * cellWidth + m_boardOffsetX, r * cellHeight + m_boardOffsetY};
                Vector2D size{cellWidth, cellHeight};
                
                Color cellColor = ((r + c) % 2 == 0) 
                    ? Color{240, 217, 181, 255}
                    : Color{181, 136, 99, 255};
                
                renderer.drawRectangle(pos, size, cellColor, true);
            }
        }

        // 3. סימון משבצת שנבחרה
        auto selectedOpt = m_controller->selectedPosition();
        if (selectedOpt.has_value()) {
            Vector2D pos{selectedOpt->col() * cellWidth + m_boardOffsetX, selectedOpt->row() * cellHeight + m_boardOffsetY};
            Vector2D size{cellWidth, cellHeight};
            renderer.drawRectangle(pos, size, {255, 255, 0, 80}, true);
        }

        // 4. ציור פאנלים צדדיים מטופל בהמשך לאחר בניית positionToAnimatedPixel

        // 5. ציור הניקוד (Score) בראש המסך
        int score = m_gameEngine->getScore();
        std::string scoreStr = "Score: " + std::string(score >= 0 ? "+" : "") + std::to_string(score);
        renderer.drawText(scoreStr, {440.0f, 80.0f}, 24, {240, 200, 80, 255});

        // 6. בניית snapshot וציור כל הכלים ומדי הצינון (Cooldowns)
        auto snapshot = kungfu::view::SnapshotBuilder::build(
            *board,
            m_gameEngine->getArbiter(),
            m_gameEngine->getCurrentTimeMs(),
            m_gameEngine->isGameOver(),
            selectedOpt,
            cellWidth
        );

        std::unordered_map<int, Vector2D> positionToAnimatedPixel;
        positionToAnimatedPixel.reserve(snapshot.pieces.size());

        for (const auto& pieceSnap : snapshot.pieces) {
            std::string assetId = (pieceSnap.color == kungfu::PlayerColor::White) ? "w" : "b";
            assetId += kungfu::PieceTokenCodec::toChar(pieceSnap.type);

            // תוספת ה-offset של הלוח לציור הפיזיקלי
            Vector2D spritePos{pieceSnap.pixelX + m_boardOffsetX, pieceSnap.pixelY + m_boardOffsetY};
            Vector2D spriteSize{cellWidth, cellHeight};

            renderer.drawSprite(assetId, spritePos, spriteSize);

            if (pieceSnap.cooldownProgress > 0.0f) {
                Vector2D center{
                    pieceSnap.pixelX + m_boardOffsetX + cellWidth / 2.0f, 
                    pieceSnap.pixelY + m_boardOffsetY + cellHeight / 2.0f
                };
                m_cooldownBar.draw(renderer, center, pieceSnap.cooldownProgress);
            }

            positionToAnimatedPixel[positionKey(pieceSnap.logicalPosition, cols)] =
                Vector2D{pieceSnap.pixelX, pieceSnap.pixelY};
        }

        // 7. ציור קווי Premoves מתוכננים
        const auto& premoveQueue = m_gameEngine->getPremoveQueue();
        for (const auto& entry : premoveQueue.entries()) {
            auto piece = entry.first;
            auto to = entry.second; 
            if (!piece) continue;

            float destX = to.col() * cellWidth + m_boardOffsetX;
            float destY = to.row() * cellHeight + m_boardOffsetY;

            renderer.drawRectangle({destX, destY}, {cellWidth, cellHeight}, {0, 120, 255, 45}, true);   
            renderer.drawRectangle({destX, destY}, {cellWidth, cellHeight}, {0, 120, 255, 180}, false);  

            Vector2D startPt{0.0f, 0.0f};
            auto it = positionToAnimatedPixel.find(positionKey(piece->position(), cols));
            if (it != positionToAnimatedPixel.end()) {
                startPt.x = it->second.x + m_boardOffsetX + cellWidth / 2.0f;
                startPt.y = it->second.y + m_boardOffsetY + cellHeight / 2.0f;
            } else {
                startPt.x = piece->position().col() * cellWidth + m_boardOffsetX + cellWidth / 2.0f;
                startPt.y = piece->position().row() * cellHeight + m_boardOffsetY + cellHeight / 2.0f;
            }

            Vector2D endPt{destX + cellWidth / 2.0f, destY + cellHeight / 2.0f};
            renderer.drawLine(startPt, endPt, {0, 120, 255, 180}, 3.0f);
        }
        // 8. שליפת המידע מהטרקר לצורך הציור של הפנאלים
        const auto& history = m_historyTracker->getMoves();
        drawPlayerPanel(renderer, "Black", 20.0f, 160.0f, 200.0f, 600.0f, history, kungfu::PlayerColor::Black);
        drawPlayerPanel(renderer, "White", 780.0f, 160.0f, 200.0f, 600.0f, history, kungfu::PlayerColor::White);

        // 9. מסך סוף המשחק
        if (snapshot.isGameOver) {
            renderer.drawRectangle({0.0f, 320.0f}, {1000.0f, 320.0f}, {15, 15, 20, 220}, true);
            renderer.drawRectangle({0.0f, 320.0f}, {1000.0f, 320.0f}, {120, 40, 40, 255}, false); 
            
            renderer.drawText("GAME OVER", {330.0f, 420.0f}, 54, {255, 60, 60, 255});
            
            m_rematchButton->draw(renderer);
            m_menuButton->draw(renderer);
        }
    }

    void handleInput(const std::vector<InputEvent>& events) override {
        for (const auto& event : events) {
            if (event.type == InputEvent::Type::Mouse) {
                const auto& mouse = event.mouse;
                
                if (m_gameEngine->isGameOver()) {
                    m_rematchButton->handleInput(mouse);
                    m_menuButton->handleInput(mouse);
                } else {
                    if (mouse.action == MouseEvent::Action::Press && mouse.button == MouseButton::Left) {
                        // העברת קואורדינטות קליק מתורגמות ומותאמות לאזור הלוח הפיזי בלבד
                        int boardRelX = static_cast<int>(mouse.logicalX - m_boardOffsetX);
                        int boardRelY = static_cast<int>(mouse.logicalY - m_boardOffsetY);
                        m_controller->click(boardRelX, boardRelY);
                    }
                }
            }
            else if (event.type == InputEvent::Type::Keyboard) {
                if (event.key.key == Key::Escape) {
                    m_screenManager.popScreen(); 
                }
            }
        }
    }
};