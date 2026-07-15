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
#include <memory>
#include <iostream>
#include <unordered_map>
#include <string>
#include <vector>

class ChessGameScreen : public IScreen {
private:
    std::shared_ptr<kungfu::GameEngine> m_gameEngine;
    std::shared_ptr<kungfu::Controller> m_controller;
    CooldownBar m_cooldownBar;
    kungfu::GameConfig m_config; 
    
    bool m_isPaused = false;

    // היסטוריית מהלכים מופרדת לשני השחקנים
    std::vector<std::string> m_whiteHistory;
    std::vector<std::string> m_blackHistory;

    // כפתורי בקרה (מעתה ממוקמים ב-Header העליון ליד הכותרת)
    std::unique_ptr<Button> m_pauseButton;
    std::unique_ptr<Button> m_sidebarRestartButton;
    std::unique_ptr<Button> m_sidebarMenuButton;

    // כפתורי סוף המשחק (ממורכזים מעל הלוח בסיום)
    std::unique_ptr<Button> m_rematchButton;
    std::unique_ptr<Button> m_menuButton;
    
    // הגדרות פריסת קואורדינטות קבועות בתוך מרחב 1000x1000 הלוגי
    float m_boardStartX = 0.0f;
    float m_boardStartY = 100.0f; // הלוח ממוקם בין y=100 ל-y=900
    float m_boardRangeX = 800.0f;
    float m_boardRangeY = 800.0f;
    float m_logicalRangeX = 1000.0f;
    float m_logicalRangeY = 1000.0f;

    // חישוב ערך חומרי (נקודות) של כלי שחמט סטנדרטי [1]
    int getPieceValue(kungfu::PieceType type) const {
        switch (type) {
            case kungfu::PieceType::Queen:  return 9;
            case kungfu::PieceType::Rook:   return 5;
            case kungfu::PieceType::Bishop: return 3;
            case kungfu::PieceType::Knight: return 3;
            case kungfu::PieceType::Pawn:   return 1;
            default:                        return 0;
        }
    }

    // חישוב סך הנקודות הנוכחי עבור כל צבע
    int calculatePlayerScore(kungfu::PlayerColor color) const {
        int total = 0;
        if (!m_gameEngine || !m_gameEngine->getBoard()) {
            return 0;
        }

        // 1. חישוב נקודות מהכלים שעל הלוח
        for (const auto& piece : m_gameEngine->getBoard()->pieces()) {
            if (piece && piece->color() == color && piece->state() != kungfu::PieceState::Captured) {
                total += getPieceValue(piece->type());
            }
        }
        // 2. חישוב נקודות מכלים שנמצאים כרגע בתנועה או קפיצה באוויר
        for (const auto& motion : m_gameEngine->getArbiter().activeMotions()) {
            auto piece = motion.piece();
            if (piece && piece->color() == color && piece->state() == kungfu::PieceState::Airborne) {
                total += getPieceValue(piece->type());
            }
        }
        return total;
    }

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
        
        // הגדרת משבצת וירטואלית יציבה של 100 עבור ה-Controller
        m_controller->setCellSize(100);

        m_isPaused = false;
        
        m_whiteHistory.clear();
        m_blackHistory.clear();
        m_whiteHistory.push_back("Connected");
        m_blackHistory.push_back("Connected");

        // אתחול כפתור ההשהיה ב-Header העליון (מיושר לצד ימין של פס הכותרת)
        m_pauseButton = std::make_unique<Button>(
            Vector2D{500.0f, 25.0f}, Vector2D{140.0f, 50.0f}, "Pause", 
            [this]() { togglePause(); }
        );
        m_pauseButton->setColors({65, 65, 80, 255}, {85, 85, 105, 255}, {255, 255, 255, 255});
    }

    void togglePause() {
        m_isPaused = !m_isPaused;
        
        if (m_isPaused) {
            m_pauseButton = std::make_unique<Button>(
                Vector2D{500.0f, 25.0f}, Vector2D{140.0f, 50.0f}, "Resume", 
                [this]() { togglePause(); }
            );
            m_pauseButton->setColors({40, 110, 75, 255}, {55, 140, 95, 255}, {255, 255, 255, 255}); 
        } else {
            m_pauseButton = std::make_unique<Button>(
                Vector2D{500.0f, 25.0f}, Vector2D{140.0f, 50.0f}, "Pause", 
                [this]() { togglePause(); }
            );
            m_pauseButton->setColors({65, 65, 80, 255}, {85, 85, 105, 255}, {255, 255, 255, 255}); 
        }
    }

public:
    explicit ChessGameScreen(ScreenManager& manager, kungfu::GameConfig config = kungfu::GameConfig{}) 
        : IScreen(manager), m_config(config) 
    {
        resetGame();

        // אתחול כפתורי ה-Header העליון
        m_sidebarRestartButton = std::make_unique<Button>(
            Vector2D{660.0f, 25.0f}, Vector2D{140.0f, 50.0f}, "Restart", 
            [this]() { resetGame(); }
        );
        m_sidebarRestartButton->setColors({55, 85, 115, 255}, {70, 105, 140, 255}, {255, 255, 255, 255});

        m_sidebarMenuButton = std::make_unique<Button>(
            Vector2D{820.0f, 25.0f}, Vector2D{140.0f, 50.0f}, "Quit Menu", 
            [this]() { m_screenManager.popScreen(); }
        );
        m_sidebarMenuButton->setColors({110, 50, 50, 255}, {140, 65, 65, 255}, {255, 255, 255, 255});

        // כפתורי סוף המשחק (ממורכזים מעל הלוח בסיום)
        m_rematchButton = std::make_unique<Button>(
            Vector2D{220.0f, 540.0f}, Vector2D{160.0f, 50.0f}, "Rematch", 
            [this]() { resetGame(); }
        );
        m_menuButton = std::make_unique<Button>(
            Vector2D{420.0f, 540.0f}, Vector2D{160.0f, 50.0f}, "Main Menu", 
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
        m_pauseButton->update(deltaTime);
        m_sidebarRestartButton->update(deltaTime);
        m_sidebarMenuButton->update(deltaTime);

        if (m_isPaused) {
            return;
        }

        int ms = static_cast<int>(deltaTime * 1000.0f);
        m_gameEngine->wait(ms);

        if (m_gameEngine->isGameOver()) {
            m_rematchButton->update(deltaTime);
            m_menuButton->update(deltaTime);
        }
    }

    void draw(IRenderer& renderer) override {
        // צבע רקע כהה ונקי על כל שטח המרחב הלוגי
        renderer.clear({20, 20, 25, 255});

        auto board = m_gameEngine->getBoard();
        int rows = board->rows();
        int cols = board->cols();
        
        float cellWidth = m_boardRangeX / cols;   // 100.0f
        float cellHeight = m_boardRangeY / rows; // 100.0f

        // --- 1. ציור לוח המשחק (ממורכז אנכית y=100) ---
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                Vector2D pos{m_boardStartX + c * cellWidth, m_boardStartY + r * cellHeight};
                Vector2D size{cellWidth, cellHeight};
                
                Color cellColor = ((r + c) % 2 == 0) 
                    ? Color{240, 217, 181, 255}
                    : Color{181, 136, 99, 255};
                
                renderer.drawRectangle(pos, size, cellColor, true);
            }
        }

        // --- 2. סימון משבצת נבחרת ---
        auto selectedOpt = m_controller->selectedPosition();
        if (selectedOpt.has_value() && !m_isPaused && !m_gameEngine->isGameOver()) {
            Vector2D pos{selectedOpt->col() * cellWidth, m_boardStartY + selectedOpt->row() * cellHeight};
            Vector2D size{cellWidth, cellHeight};
            renderer.drawRectangle(pos, size, {255, 255, 0, 80}, true);
        }

        // --- 3. בניית Snapshot וציור כל הכלים ---
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

            float animatedX = pieceSnap.pixelX;
            float animatedY = m_boardStartY + pieceSnap.pixelY;
            
            Vector2D spritePos{animatedX, animatedY};
            Vector2D spriteSize{cellWidth, cellHeight};

            renderer.drawSprite(assetId, spritePos, spriteSize);

            if (pieceSnap.cooldownProgress > 0.0f) {
                Vector2D center{animatedX + cellWidth / 2.0f, animatedY + cellHeight / 2.0f};
                m_cooldownBar.draw(renderer, center, pieceSnap.cooldownProgress);
            }

            positionToAnimatedPixel[positionKey(pieceSnap.logicalPosition, cols)] = spritePos;
        }

        // --- 4. ציור חיווי ויזואלי של מהלכים מתוכננים מראש (Premoves) ---
        if (!m_isPaused && !m_gameEngine->isGameOver()) {
            const auto& premoveQueue = m_gameEngine->getPremoveQueue();
            for (const auto& entry : premoveQueue.entries()) {
                auto piece = entry.first;
                auto to = entry.second; 
                if (!piece) continue;

                float destX = to.col() * cellWidth;
                float destY = m_boardStartY + to.row() * cellHeight;

                renderer.drawRectangle({destX, destY}, {cellWidth, cellHeight}, {0, 120, 255, 45}, true);   
                renderer.drawRectangle({destX, destY}, {cellWidth, cellHeight}, {0, 120, 255, 180}, false);  

                Vector2D startPt{0.0f, 0.0f};
                auto it = positionToAnimatedPixel.find(positionKey(piece->position(), cols));
                if (it != positionToAnimatedPixel.end()) {
                    startPt.x = it->second.x + cellWidth / 2.0f;
                    startPt.y = it->second.y + cellHeight / 2.0f;
                } else {
                    startPt.x = piece->position().col() * cellWidth + cellWidth / 2.0f;
                    startPt.y = m_boardStartY + piece->position().row() * cellHeight + cellHeight / 2.0f;
                }

                Vector2D endPt{destX + cellWidth / 2.0f, destY + cellHeight / 2.0f};
                renderer.drawLine(startPt, endPt, {0, 120, 255, 180}, 3.0f);
            }
        }

        // --- 5. סרגל עליון (Top Header Bar): כותרת משחק ולחצני בקרה קבועים ---
        renderer.drawRectangle({0.0f, 0.0f}, {1000.0f, 95.0f}, {25, 25, 30, 255}, true);
        renderer.drawLine({0.0f, 95.0f}, {1000.0f, 95.0f}, {60, 60, 75, 255}, 2.0f);

        renderer.drawText("KUNG-FU CHESS", {30.0f, 60.0f}, 24, {240, 200, 80, 255});

        // רינדור כפתורי השליטה ב-Header (תמיד נגישים ללחיצה)
        m_pauseButton->draw(renderer);
        m_sidebarRestartButton->draw(renderer);
        m_sidebarMenuButton->draw(renderer);

        // --- 6. סרגל ימני: שתי טבלאות מופרדות להיסטוריית המהלכים של כל שחקן ---
        renderer.drawRectangle({800.0f, 100.0f}, {200.0f, 800.0f}, {25, 25, 35, 255}, true);
        renderer.drawLine({800.0f, 100.0f}, {800.0f, 900.0f}, {60, 60, 75, 255}, 2.0f);

        // א. טבלת מהלכי שחקן לבן (White Player Moves)
        renderer.drawRectangle({810.0f, 110.0f}, {180.0f, 360.0f}, {30, 30, 40, 255}, true);
        renderer.drawRectangle({810.0f, 110.0f}, {180.0f, 360.0f}, {70, 70, 85, 255}, false);
        renderer.drawText("WHITE MOVES", {825.0f, 145.0f}, 13, {255, 255, 255, 255});
        renderer.drawLine({815.0f, 160.0f}, {985.0f, 160.0f}, {50, 50, 65, 255}, 1.0f);

        float whiteLogY = 195.0f;
        for (const auto& logEntry : m_whiteHistory) {
            Color textColor = {200, 200, 210, 255};
            if (logEntry.find("rejected") != std::string::npos || logEntry.find("failed") != std::string::npos) {
                textColor = {240, 100, 100, 255};
            } else if (logEntry.find("ok") != std::string::npos || logEntry.find("Connected") != std::string::npos) {
                textColor = {100, 210, 130, 255};
            }
            renderer.drawText(logEntry, {825.0f, whiteLogY}, 10, textColor);
            whiteLogY += 35.0f;
        }

        // ב. טבלת מהלכי שחקן שחור (Black Player Moves)
        renderer.drawRectangle({810.0f, 490.0f}, {180.0f, 360.0f}, {30, 30, 40, 255}, true);
        renderer.drawRectangle({810.0f, 490.0f}, {180.0f, 360.0f}, {70, 70, 85, 255}, false);
        renderer.drawText("BLACK MOVES", {825.0f, 525.0f}, 13, {160, 160, 175, 255});
        renderer.drawLine({815.0f, 540.0f}, {985.0f, 540.0f}, {50, 50, 65, 255}, 1.0f);

        float blackLogY = 575.0f;
        for (const auto& logEntry : m_blackHistory) {
            Color textColor = {170, 170, 185, 255};
            if (logEntry.find("rejected") != std::string::npos || logEntry.find("failed") != std::string::npos) {
                textColor = {240, 100, 100, 255};
            } else if (logEntry.find("ok") != std::string::npos || logEntry.find("Connected") != std::string::npos) {
                textColor = {100, 210, 130, 255};
            }
            renderer.drawText(logEntry, {825.0f, blackLogY}, 10, textColor);
            blackLogY += 35.0f;
        }

        // --- 7. סרגל תחתון (Bottom Status Bar): ניקוד מפורט וחיווי תורות ---
        renderer.drawRectangle({0.0f, 905.0f}, {1000.0f, 95.0f}, {25, 25, 30, 255}, true);
        renderer.drawLine({0.0f, 905.0f}, {1000.0f, 905.0f}, {60, 60, 75, 255}, 2.0f);

        int whiteScore = calculatePlayerScore(kungfu::PlayerColor::White);
        int blackScore = calculatePlayerScore(kungfu::PlayerColor::Black);
        renderer.drawText("WHITE Score: " + std::to_string(whiteScore) + " / 39", {40.0f, 960.0f}, 15, {255, 255, 255, 255});
        renderer.drawText("BLACK Score: " + std::to_string(blackScore) + " / 39", {340.0f, 960.0f}, 15, {170, 170, 180, 255});

        renderer.drawText("GAME STATUS: ", {640.0f, 960.0f}, 13, {150, 150, 170, 255});
        if (m_gameEngine->isGameOver()) {
            renderer.drawText("Game Over!", {750.0f, 960.0f}, 14, {240, 80, 80, 255});
        } else if (m_isPaused) {
            renderer.drawText("Paused", {750.0f, 960.0f}, 14, {240, 200, 80, 255});
        } else {
            if (!m_config.allowSimultaneousMovement) {
                if (m_gameEngine->currentTurn() == kungfu::PlayerColor::White) {
                    renderer.drawText("White's Turn", {750.0f, 960.0f}, 14, {240, 240, 250, 255});
                } else {
                    renderer.drawText("Black's Turn", {750.0f, 960.0f}, 14, {150, 150, 160, 255});
                }
            } else {
                renderer.drawText("Real-Time Battle!", {750.0f, 960.0f}, 14, {80, 180, 240, 255});
            }
        }

        // --- 8. מודל מסך השהיה (Pause Modal) ---
        if (m_isPaused && !snapshot.isGameOver) {
            renderer.drawRectangle({m_boardStartX, m_boardStartY}, {m_boardRangeX, m_boardRangeY}, {15, 15, 20, 180}, true);
            
            renderer.drawRectangle({350.0f, 300.0f}, {300.0f, 150.0f}, {25, 25, 35, 240}, true);
            renderer.drawRectangle({350.0f, 300.0f}, {300.0f, 150.0f}, {80, 80, 100, 255}, false);
            
            renderer.drawText("PAUSED", {445.0f, 370.0f}, 38, {240, 200, 80, 255});
            renderer.drawText("Press SPACE or Resume", {390.0f, 415.0f}, 14, {180, 180, 190, 255});
        }

        // --- 9. מודל סיום משחק (Game Over Modal) ---
        if (snapshot.isGameOver) {
            renderer.drawRectangle({m_boardStartX, m_boardStartY}, {m_boardRangeX, m_boardRangeY}, {10, 10, 15, 150}, true);

            renderer.drawRectangle({150.0f, 350.0f}, {500.0f, 300.0f}, {20, 20, 25, 240}, true);
            renderer.drawRectangle({150.0f, 350.0f}, {500.0f, 300.0f}, {200, 60, 60, 255}, false); 
            
            renderer.drawText("GAME OVER", {250.0f, 440.0f}, 50, {255, 60, 60, 255});
            
            m_rematchButton->draw(renderer);
            m_menuButton->draw(renderer);
        }
    }

    void handleInput(const std::vector<InputEvent>& events) override {
        for (const auto& event : events) {
            if (event.type == InputEvent::Type::Mouse) {
                const auto& mouse = event.mouse;
                
                m_pauseButton->handleInput(mouse);
                m_sidebarRestartButton->handleInput(mouse);
                m_sidebarMenuButton->handleInput(mouse);

                if (m_gameEngine->isGameOver()) {
                    m_rematchButton->handleInput(mouse);
                    m_menuButton->handleInput(mouse);
                } else if (!m_isPaused) {
                    // ביצוע תרגום קלט ישיר ומסונכרן לחלוטין מתוך הטווח הלוגי הקבוע 1000x1000
                    if (mouse.logicalX >= m_boardStartX && mouse.logicalX < m_boardStartX + m_boardRangeX &&
                        mouse.logicalY >= m_boardStartY && mouse.logicalY < m_boardStartY + m_boardRangeY) {
                        
                        if (mouse.action == MouseEvent::Action::Press && mouse.button == MouseButton::Left) {
                            float boardClickX = mouse.logicalX;
                            float boardClickY = mouse.logicalY - m_boardStartY;

                            int col = static_cast<int>(boardClickX / 100.0f);
                            int row = static_cast<int>(boardClickY / 100.0f);

                            if (col >= 0 && col < 8 && row >= 0 && row < 8) {
                                // המרה וירטואלית יציבה מול Controller המוגדר עם Cell Size של 100
                                int virtualX = col * 100 + 50;
                                int virtualY = row * 100 + 50;
                                
                                // שמירת השחקן הנוכחי לפני הלחיצה כדי להחליט לאיזה לוג לשייך את המהלך
                                auto activeColor = m_gameEngine->currentTurn();
                                
                                auto result = m_controller->click(virtualX, virtualY);
                                
                                if (result.actionTaken && !result.description.empty()) {
                                    std::string logText = result.description;
                                    // ניקוי כיתובים חוזרים כדי לחסוך מקום בטבלאות
                                    if (logText.rfind("Move requested: ", 0) == 0) {
                                        logText = logText.substr(16);
                                    }

                                    if (activeColor == kungfu::PlayerColor::White) {
                                        m_whiteHistory.push_back(logText);
                                        if (m_whiteHistory.size() > 8) { // שמירה על 8 מהלכים אחרונים בטבלה
                                            m_whiteHistory.erase(m_whiteHistory.begin());
                                        }
                                    } else {
                                        m_blackHistory.push_back(logText);
                                        if (m_blackHistory.size() > 8) {
                                            m_blackHistory.erase(m_blackHistory.begin());
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else if (event.type == InputEvent::Type::Keyboard) {
                if (event.key.key == Key::Escape) {
                    if (m_gameEngine->isGameOver()) {
                        m_screenManager.popScreen();
                    } else {
                        togglePause();
                    }
                }
                else if (event.key.key == Key::Space) {
                    if (!m_gameEngine->isGameOver()) {
                        togglePause();
                    }
                }
            }
        }
    }
};