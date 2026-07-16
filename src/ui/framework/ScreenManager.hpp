// מנהל המסכים המחזיק את מחסנית המסכים ומבצע את פעולות המעבר בצורה בטוחה
// בסוף מחזור העדכון באמצעות מנגנון Pending Actions.
// כך נמנעים ממחיקת מסך בזמן שהוא עדיין מריץ update()/handleInput().

#pragma once

#include "IScreen.hpp"
#include <memory>
#include <vector>

class IRenderer;
struct InputEvent;

class ScreenManager {
private:
    std::vector<std::unique_ptr<IScreen>> m_screens;

    enum class PendingActionType {
        Push,
        Pop,
        Change
    };

    struct PendingAction {
        PendingActionType type;
        std::unique_ptr<IScreen> screen;
    };

    std::vector<PendingAction> m_pendingActions;

private:

    // מסיר את כל המסכים מהמחסנית.
    // משמש בעת החלפת כל המסכים במסך חדש.
    void clearScreens() {
        while (!m_screens.empty()) {
            m_screens.back()->onExit();
            m_screens.pop_back();
        }
    }

    void applyPush(std::unique_ptr<IScreen> screen) {
        m_screens.push_back(std::move(screen));
        m_screens.back()->onEnter();
    }

    void applyPop() {
        if (m_screens.empty()) {
            return;
        }

        m_screens.back()->onExit();
        m_screens.pop_back();
    }

    void applyChange(std::unique_ptr<IScreen> screen) {
        clearScreens();
        applyPush(std::move(screen));
    }

    void processAction(PendingAction& action) {
        switch (action.type) {
        case PendingActionType::Push:
            applyPush(std::move(action.screen));
            break;

        case PendingActionType::Pop:
            applyPop();
            break;

        case PendingActionType::Change:
            applyChange(std::move(action.screen));
            break;
        }
    }

    // מבצע את כל הפעולות שנדחו.
    // onEnter()/onExit() יכולים בעצמם לבצע push/pop/change.
    // לכן אי אפשר לעבור ישירות על m_pendingActions,
    // אחרת vector עלול להשתנות בזמן האיטרציה.
    // הפתרון הוא להעביר (move) את כל הפעולות לרשימה זמנית,
    // לבצע אותן, ואז לבדוק האם נוספו פעולות חדשות.
    void applyPendingActions() {
        while (!m_pendingActions.empty()) {

            auto actionsToApply = std::move(m_pendingActions);
            m_pendingActions.clear();

            for (auto& action : actionsToApply) {
                processAction(action);
            }
        }
    }

public:

    ScreenManager() = default;

    // אין צורך לקרוא ל-onExit().
    // ה-unique_ptr ידאג להשמיד את כל המסכים בצורה אוטומטית.
    ~ScreenManager() = default;

    ScreenManager(const ScreenManager&) = delete;
    ScreenManager& operator=(const ScreenManager&) = delete;

    // מחליף את כל מחסנית המסכים במסך חדש.
    void changeScreen(std::unique_ptr<IScreen> newScreen) {
        m_pendingActions.push_back(
            { PendingActionType::Change, std::move(newScreen) });
    }

    // מוסיף מסך חדש מעל המסך הפעיל.
    void pushScreen(std::unique_ptr<IScreen> newScreen) {
        m_pendingActions.push_back(
            { PendingActionType::Push, std::move(newScreen) });
    }

    // מסיר את המסך העליון.
    void popScreen() {
        m_pendingActions.push_back(
            { PendingActionType::Pop, nullptr });
    }

    // מעדכן את המסך הפעיל.
    void update(float deltaTime) {
        applyPendingActions();

        if (!m_screens.empty()) {
            m_screens.back()->update(deltaTime);
        }
    }

    // מצייר את המסך הפעיל.
    void draw(IRenderer& renderer) {
        if (!m_screens.empty()) {
            m_screens.back()->draw(renderer);
        }
    }

    // מעביר את אירועי הקלט למסך הפעיל בלבד.
    void handleInput(const std::vector<InputEvent>& events) {
        if (!m_screens.empty()) {
            m_screens.back()->handleInput(events);
        }
    }

    bool isEmpty() const {
        return m_screens.empty() && m_pendingActions.empty();
    }
};