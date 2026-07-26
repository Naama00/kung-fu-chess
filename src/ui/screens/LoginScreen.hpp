// ui/screens/LoginScreen.hpp
#pragma once

#include "ui/screens/BaseScreen.hpp"
#include "ui/framework/ISoundPlayer.hpp"
#include "players/network/NetworkSession.hpp"
#include <future>
#include <memory>
#include <string>
#include <vector>

class LoginScreen : public BaseScreen {
public:
    explicit LoginScreen(ScreenManager& manager,
                         std::shared_ptr<ISoundPlayer> soundPlayer = nullptr,
                         bool isSfml = false);
    ~LoginScreen() override = default;

    void onEnter() override;
    void onExit() override;
    void update(float deltaTime) override;
    void handleInput(const std::vector<InputEvent>& events) override;

protected:
    void drawContent(IRenderer& renderer) override;

private:
    enum class ActiveField { Username, Password };

    struct AuthResult {
        bool success;
        std::string message;
        int rating;
    };

    static AuthResult performNetworkAuth(const std::string& username, const std::string& password, bool isRegister);
    char translateKeyToChar(const KeyEvent& keyEvent) const;
    void startAuth(bool isRegister);

    std::string m_usernameText;
    std::string m_passwordText;
    ActiveField m_activeField = ActiveField::Username;

    float m_cursorTimer = 0.0f;
    std::shared_ptr<ISoundPlayer> m_soundPlayer;
    bool m_isSfml = false;

    std::future<AuthResult> m_authFuture;
    bool m_authPending = false;
    std::shared_ptr<kungfu::NetworkSession> m_authSession;
    bool m_loginConnecting = false;

    std::string m_statusMessage;
    Color m_statusColor{210, 215, 225, 255};

    const Vector2D m_panelPos{270.0f, 240.0f};
    const Vector2D m_panelSize{460.0f, 510.0f};

    const Vector2D m_inputSize{340.0f, 45.0f};
    const Vector2D m_usernamePos{330.0f, 370.0f};
    const Vector2D m_passwordPos{330.0f, 470.0f};

    const Vector2D m_btnSize{160.0f, 50.0f};
    const Vector2D m_loginBtnPos{330.0f, 560.0f};
    const Vector2D m_registerBtnPos{510.0f, 560.0f};

    const Vector2D m_offlineBtnSize{340.0f, 50.0f};
    const Vector2D m_offlineBtnPos{330.0f, 630.0f};

    Vector2D m_mousePos{0.0f, 0.0f};
};