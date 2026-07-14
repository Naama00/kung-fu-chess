#pragma once

#include "ui/framework/IScreen.hpp"
#include "ui/framework/ScreenManager.hpp"
#include "ui/components/CooldownBar.hpp"
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
    
    // מרחב הקואורדינטות הלוגי (1000x1000)
    float m_logicalRangeX = 1000.0f;
    float m_logicalRangeY = 1000.0f;

public:
    explicit ChessGameScreen(ScreenManager& manager) : IScreen(manager) {
        // 1. הגדרת לוח שחמט סטנדרטי התחלתי
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
        
        // 2. אתחול מנוע המשחק והבקר (Controller)
        m_gameEngine = std::make_shared<kungfu::GameEngine>(board, ruleEngine);
        m_controller = std::make_shared<kungfu::Controller>(m_gameEngine);
        
        // 3. סנכרון גודל התא של הבקר עם מערכת הצירים הלוגית (1000 / 8 עמודות = 125)
        int cols = board->cols();
        int logicalCellSize = static_cast<int>(m_logicalRangeX / cols);
        m_controller->setCellSize(logicalCellSize);
    }

    void onEnter() override {
        std::cout << "Chess Game Screen Activated!" << std::endl;
    }

    void onExit() override {
        std::cout << "Chess Game Screen Deactivated!" << std::endl;
    }

    void update(float deltaTime) override {
        // המרת deltaTime (בשניות) למילישניות ועדכון מנוע המשחק בזמן אמת
        int ms = static_cast<int>(deltaTime * 1000.0f);
        m_gameEngine->wait(ms);
    }

    void draw(IRenderer& renderer) override {
        // 1. ניקוי המסך לרקע כהה נעים
        renderer.clear({30, 30, 40, 255});

        auto board = m_gameEngine->getBoard();
        int rows = board->rows();
        int cols = board->cols();
        
        float cellWidth = m_logicalRangeX / cols;
        float cellHeight = m_logicalRangeY / rows;

        // 2. ציור לוח השחמט (משבצות בהירות וכהות)
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                Vector2D pos{c * cellWidth, r * cellHeight};
                Vector2D size{cellWidth, cellHeight};
                
                Color cellColor = ((r + c) % 2 == 0) 
                    ? Color{240, 217, 181, 255}   // קרם בהיר
                    : Color{181, 136, 99, 255};    // חום עץ
                
                renderer.drawRectangle(pos, size, cellColor, true);
            }
        }

        // 3. סימון והדגשת המשבצת שנבחרה כרגע על ידי השחקן
        auto selectedOpt = m_controller->selectedPosition();
        if (selectedOpt.has_value()) {
            Vector2D pos{selectedOpt->col() * cellWidth, selectedOpt->row() * cellHeight};
            Vector2D size{cellWidth, cellHeight};
            renderer.drawRectangle(pos, size, {255, 255, 0, 80}, true); // צהוב חצי שקוף
        }

        // 4. בניית תמונת המצב (Snapshot) הכוללת את מיקומי הכלים (כולל תנועה חלקה)
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
            // חישוב מרכז ורדיוס הציור של הכלי
            Vector2D center{pieceSnap.pixelX + cellWidth / 2.0f, pieceSnap.pixelY + cellHeight / 2.0f};
            float radius = std::min(cellWidth, cellHeight) * 0.35f;

            // התאמת צבעים לפי צבע השחקן
            Color pieceBg = (pieceSnap.color == kungfu::PlayerColor::White) ? Color{255, 255, 255, 255} : Color{45, 45, 45, 255};
            Color pieceBorder = (pieceSnap.color == kungfu::PlayerColor::White) ? Color{0, 0, 0, 255} : Color{220, 220, 220, 255};
            Color textCol = (pieceSnap.color == kungfu::PlayerColor::White) ? Color{0, 0, 0, 255} : Color{255, 255, 255, 255};

            // א. ציור גוף הכלי (עיגול עם מסגרת)
            renderer.drawCircle(center, radius, pieceBg, true);
            renderer.drawCircle(center, radius, pieceBorder, false);

            // ב. כתיבת תו הטוקן המייצג את סוג הכלי
            char tokenChar = kungfu::PieceTokenCodec::toChar(pieceSnap.type);
            std::string tokenStr(1, tokenChar);
            renderer.drawText(tokenStr, {center.x - 10.0f, center.y + 10.0f}, 24, textCol);

            // ג. זיהוי וציור מד הצינון (Cooldown) במידה והכלי בטעינה
            auto boardPieces = board->pieces();
            for (const auto& p : boardPieces) {
                if (p && p->position() == pieceSnap.logicalPosition) {
                    int currentMs = m_gameEngine->getCurrentTimeMs();
                    if (m_gameEngine->getArbiter().isOnCooldown(std::const_pointer_cast<kungfu::Piece>(p), currentMs)) {
                        // מדגם פשוט המראה חיווי ויזואלי של טעינה
                        m_cooldownBar.draw(renderer, center, 0.6f); 
                    }
                }
            }
        }

        // 6. חיווי מסך סוף משחק (Game Over)
        if (snapshot.isGameOver) {
            renderer.drawRectangle({0.0f, 400.0f}, {1000.0f, 200.0f}, {0, 0, 0, 200}, true);
            renderer.drawText("GAME OVER", {330.0f, 515.0f}, 48, {255, 60, 60, 255});
        }
    }

    void handleInput(const std::vector<InputEvent>& events) override {
        for (const auto& event : events) {
            if (event.type == InputEvent::Type::Mouse) {
                const auto& mouse = event.mouse;
                if (mouse.action == MouseEvent::Action::Press && mouse.button == MouseButton::Left) {
                    // שליחת קואורדינטות לוגיות (0..1000) ישירות ל-Controller
                    m_controller->click(static_cast<int>(mouse.logicalX), static_cast<int>(mouse.logicalY));
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