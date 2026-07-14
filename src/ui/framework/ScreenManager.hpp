// מנהל המסכים המחזיק את המחסנית ומבצע את פעולות המעבר בצורה בטוחה בסוף מחזור העדכון
// (באמצעות מנגנון Pending Actions) כדי למנוע קריסות זיכרון
#pragma once
#include "IScreen.hpp"
#include <vector>
#include <memory>

class IRenderer;
struct InputEvent;

class ScreenManager {
private:
    std::vector<std::unique_ptr<IScreen>> m_screens;

    enum class PendingActionType { Push, Pop, Change };
    struct PendingAction {
        PendingActionType type;
        std::unique_ptr<IScreen> screen;
    };
    std::vector<PendingAction> m_pendingActions;

    void applyPendingActions() {
        for (auto& action : m_pendingActions) {
            switch (action.type) {
                case PendingActionType::Push:
                    m_screens.push_back(std::move(action.screen));
                    m_screens.back()->onEnter();
                    break;

                case PendingActionType::Pop:
                    if (!m_screens.empty()) {
                        m_screens.back()->onExit();
                        m_screens.pop_back();
                    }
                    break;

                case PendingActionType::Change:
                    while (!m_screens.empty()) {
                        m_screens.back()->onExit();
                        m_screens.pop_back();
                    }
                    m_screens.push_back(std::move(action.screen));
                    m_screens.back()->onEnter();
                    break;
            }
        }
        m_pendingActions.clear();
    }

public:
    ScreenManager() = default;
    ~ScreenManager() {
        while (!m_screens.empty()) {
            m_screens.back()->onExit();
            m_screens.pop_back();
        }
    }

    // מניעת העתקה של מנהל המסכים
    ScreenManager(const ScreenManager&) = delete;
    ScreenManager& operator=(const ScreenManager&) = delete;

    // מעבר למסך חדש וניקוי כל המחסנית
    void changeScreen(std::unique_ptr<IScreen> newScreen) {
        m_pendingActions.push_back({PendingActionType::Change, std::move(newScreen)});
    }

    // דחיפת מסך חדש מעל המסך הנוכחי (למשל תפריט השהיה)
    void pushScreen(std::unique_ptr<IScreen> newScreen) {
        m_pendingActions.push_back({PendingActionType::Push, std::move(newScreen)});
    }

    // הסרת המסך העליון וחזרה למסך הקודם
    void popScreen() {
        m_pendingActions.push_back({PendingActionType::Pop, nullptr});
    }

    // עדכון המסך הפעיל
    void update(float deltaTime) {
        applyPendingActions();
        if (!m_screens.empty()) {
            m_screens.back()->update(deltaTime);
        }
    }

    // רינדור המסך הפעיל
    void draw(IRenderer& renderer) {
        if (!m_screens.empty()) {
            m_screens.back()->draw(renderer);
        }
    }

    // ניתוב קלט למסך הפעיל
    void handleInput(const std::vector<InputEvent>& events) {
        if (!m_screens.empty()) {
            m_screens.back()->handleInput(events);
        }
    }

    bool isEmpty() const {
        return m_screens.empty();
    }
};