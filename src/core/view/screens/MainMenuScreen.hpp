#pragma once

#include "ui/framework/IScreen.hpp"
#include "ui/framework/ScreenManager.hpp"
#include "ui/components/Button.hpp"
#include "core/view/screens/ChessGameScreen.hpp"
#include "core/common/GameConfig.hpp"
#include <memory>
#include <iostream>

class MainMenuScreen : public IScreen {
private:
    Button m_playRealtimeButton;
    Button m_playTurnButton;
    Button m_exitButton;
    
    float m_logicalRangeX = 1000.0f;
    float m_logicalRangeY = 1000.0f;

public:
    explicit MainMenuScreen(ScreenManager& manager) 
        : IScreen(manager)
        , m_playRealtimeButton({300.0f, 430.0f}, {400.0f, 65.0f}, "Play Kung-Fu Chess (Real-Time)", [this]() {
            // פתיחת משחק קונג-פו בזמן אמת (צינון ופרה-מובס פעילים)
            kungfu::GameConfig config;
            config.allowSimultaneousMovement = true;
            config.enablePremoves = true;
            m_screenManager.pushScreen(std::make_unique<ChessGameScreen>(m_screenManager, config));
          })
        , m_playTurnButton({300.0f, 520.0f}, {400.0f, 65.0f}, "Play Classic Chess (Turn-Based)", [this]() {
            // פתיחת משחק קלאסי מבוסס תורות (צינון, תנועה איטית וקפיצות כבויים לחלוטין)
            kungfu::GameConfig config;
            config.allowSimultaneousMovement = false;
            config.enablePremoves = false;
            config.cooldownDurationMs = 0;   // ללא זמני צינון
            config.msPerCellSpeed = 0;       // תנועת החלקה מיידית על הלוח
            config.jumpDurationMs = 0;       
            config.allowJumping = false;     // ביטול מוחלט של מנגנון הקפיצות
            m_screenManager.pushScreen(std::make_unique<ChessGameScreen>(m_screenManager, config));
          })
        , m_exitButton({300.0f, 610.0f}, {400.0f, 65.0f}, "Exit Game", [this]() {
            // הסרת מסך הפתיחה תרוקן את המחסנית ותסגור את המשחק בצורה נקייה
            m_screenManager.popScreen();
          })
    {
        // התאמת סכמת צבעים איכותית לכל כפתור (נורמלי, ריחוף, טקסט)
        m_playRealtimeButton.setColors({40, 110, 75, 255}, {55, 140, 95, 255}, {255, 255, 255, 255}); // ירוק
        m_playTurnButton.setColors({45, 85, 130, 255}, {60, 110, 165, 255}, {255, 255, 255, 255});    // כחול
        m_exitButton.setColors({140, 50, 50, 255}, {175, 65, 65, 255}, {255, 255, 255, 255});          // אדום
    }

    void onEnter() override {
        std::cout << "Main Menu Entered!" << std::endl;
    }

    void onExit() override {
        std::cout << "Main Menu Exited!" << std::endl;
    }

    void update(float deltaTime) override {
        m_playRealtimeButton.update(deltaTime);
        m_playTurnButton.update(deltaTime);
        m_exitButton.update(deltaTime);
    }

    void draw(IRenderer& renderer) override {
        // צביעת הרקע בצבע כהה נעים המותאם למשחקים
        renderer.clear({20, 20, 25, 255});

        // כיוונון קואורדינטות המירכוז של הטקסטים
        renderer.drawText("KUNG-FU CHESS", {210.0f, 220.0f}, 64, {240, 200, 80, 255});
        renderer.drawText("Real-Time Battle Engine", {315.0f, 300.0f}, 22, {180, 180, 190, 255});

        // רינדור הכפתורים
        m_playRealtimeButton.draw(renderer);
        m_playTurnButton.draw(renderer);
        m_exitButton.draw(renderer);
    }

    void handleInput(const std::vector<InputEvent>& events) override {
        for (const auto& event : events) {
            if (event.type == InputEvent::Type::Mouse) {
                m_playRealtimeButton.handleInput(event.mouse);
                m_playTurnButton.handleInput(event.mouse);
                m_exitButton.handleInput(event.mouse);
            }
            else if (event.type == InputEvent::Type::Keyboard) {
                if (event.key.key == Key::Escape) {
                    m_screenManager.popScreen();
                }
            }
        }
    }
};