// Screen manager that holds the screen stack and performs transitions safely
// At the end of the update cycle using the Pending Actions mechanism.
// This prevents removing a screen while it is still running update()/handleInput().

#pragma once

#include "ui/framework/IScreen.hpp"
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

    // Remove all screens from the stack.
    // Used when replacing all screens with a new screen.
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

    // Execute all deferred actions.
    // onEnter()/onExit() may themselves perform push/pop/change.
    // Therefore, one cannot iterate directly over m_pendingActions,
    // otherwise the vector may change during iteration.
    // The solution is to move all actions into a temporary list,
    // execute them, and then check whether new actions were added.
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

    // There is no need to call onExit().
    // The unique_ptr will automatically destroy all screens.
    ~ScreenManager() = default;

    ScreenManager(const ScreenManager&) = delete;
    ScreenManager& operator=(const ScreenManager&) = delete;

    // Replaces the entire screen stack with a new screen.
    void changeScreen(std::unique_ptr<IScreen> newScreen) {
        m_pendingActions.push_back(
            { PendingActionType::Change, std::move(newScreen) });
    }

    // Adds a new screen on top of the active screen.
    void pushScreen(std::unique_ptr<IScreen> newScreen) {
        m_pendingActions.push_back(
            { PendingActionType::Push, std::move(newScreen) });
    }

    // Removes the top screen.
    void popScreen() {
        m_pendingActions.push_back(
            { PendingActionType::Pop, nullptr });
    }

    // Updates the active screen.
    void update(float deltaTime) {
        applyPendingActions();

        if (!m_screens.empty()) {
            m_screens.back()->update(deltaTime);
        }
    }

    // Draws the active screen.
    void draw(IRenderer& renderer) {
        if (!m_screens.empty()) {
            m_screens.back()->draw(renderer);
        }
    }

    // Routes input events to the active screen only.
    void handleInput(const std::vector<InputEvent>& events) {
        if (!m_screens.empty()) {
            m_screens.back()->handleInput(events);
        }
    }

    bool isEmpty() const {
        return m_screens.empty() && m_pendingActions.empty();
    }
};