// ui/screens/LoginScreen.cpp
#include "ui/screens/LoginScreen.hpp"
#include "ui/screens/StartScreen.hpp"
#include "ui/framework/ScreenManager.hpp"
#include "server/network/Serializer.hpp"
#include "players/network/ClientAuth.hpp"
#include <boost/asio.hpp>
#include <iostream>

LoginScreen::LoginScreen(ScreenManager& manager, std::shared_ptr<ISoundPlayer> soundPlayer, bool isSfml)
    : BaseScreen(manager, ""), m_soundPlayer(std::move(soundPlayer)), m_isSfml(isSfml) {
    m_theme.background = Color{12, 13, 17, 255};
    m_theme.titleText = Color{240, 200, 80, 255};
    m_theme.buttonNormal = Color{35, 37, 45, 255};
    m_theme.buttonHover = Color{48, 120, 192, 255};
    m_theme.border = Color{55, 58, 70, 255};
    m_theme.bodyText = Color{210, 215, 225, 255};
}

LoginScreen::AuthResult LoginScreen::performNetworkAuth(const std::string& username, const std::string& password, bool isRegister) {
    AuthResult res{false, "Connection error", 1200};
    try {
        boost::asio::io_context ioContext;
        boost::asio::ip::tcp::socket socket(ioContext);
        boost::asio::ip::tcp::resolver resolver(ioContext);

        boost::system::error_code ec;
        auto endpoints = resolver.resolve("127.0.0.1", "8080", ec);
        if (ec) {
            res.message = "Could not resolve server address";
            return res;
        }

        boost::asio::connect(socket, endpoints, ec);
        if (ec) {
            res.message = "Server is offline";
            return res;
        }

        auto payload = kungfu::Serializer::serializeAuthRequest(username, password);
        auto type = isRegister ? kungfu::NetworkMessageType::REGISTER_REQUEST : kungfu::NetworkMessageType::LOGIN_REQUEST;
        auto frame = kungfu::Serializer::buildFrame(type, payload);

        boost::asio::write(socket, boost::asio::buffer(frame), ec);
        if (ec) {
            res.message = "Failed to send credentials";
            return res;
        }

        std::vector<std::uint8_t> headerBuf(kungfu::kHeaderSize);
        boost::asio::read(socket, boost::asio::buffer(headerBuf), ec);
        if (ec) {
            res.message = "No response from server";
            return res;
        }

        std::size_t offset = 0;
        std::uint8_t resType = 0;
        std::uint32_t payloadSize = 0;
        kungfu::Serializer::readU8(headerBuf, offset, resType);
        kungfu::Serializer::readU32(headerBuf, offset, payloadSize);

        std::vector<std::uint8_t> responsePayload(payloadSize);
        if (payloadSize > 0) {
            boost::asio::read(socket, boost::asio::buffer(responsePayload), ec);
            if (ec) {
                res.message = "Incomplete response from server";
                return res;
            }
        }

        if (resType == static_cast<std::uint8_t>(kungfu::NetworkMessageType::REGISTER_RESPONSE)) {
            if (!responsePayload.empty() && responsePayload[0] == 1) {
                res.success = true;
                res.message = "Registration successful!";
            } else {
                res.message = "Registration failed. Username taken.";
            }
        }

        boost::system::error_code ignored;
        socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
        socket.close(ignored);
    } catch (const std::exception& e) {
        res.message = e.what();
    }
    return res;
}

char LoginScreen::translateKeyToChar(const KeyEvent& keyEvent) const {
    int code = keyEvent.rawCode;
    if ((code >= 'a' && code <= 'z') || 
        (code >= 'A' && code <= 'Z') || 
        (code >= '0' && code <= '9') || 
        code == ' ' || code == '.' || code == '_' || code == '-') {
        return static_cast<char>(code);
    }
    return '\0';
}

void LoginScreen::startAuth(bool isRegister) {
    if (m_usernameText.empty() || m_passwordText.empty()) {
        m_statusMessage = "Please enter both username and password.";
        m_statusColor = {240, 100, 100, 255}; 
        return;
    }

    kungfu::ClientAuth::reset();

    if (isRegister) {
        m_statusMessage = "Registering account...";
        m_statusColor = {240, 200, 80, 255}; 
        m_authPending = true;

        m_authFuture = std::async(std::launch::async, [this]() {
            return performNetworkAuth(m_usernameText, m_passwordText, true);
        });
        return;
    }

    m_statusMessage = "Connecting to server...";
    m_statusColor = {240, 200, 80, 255};
    m_authPending = true;
    m_loginConnecting = true;

    kungfu::ClientAuth::username = m_usernameText;
    kungfu::ClientAuth::password = m_passwordText;
    kungfu::ClientAuth::isAuthenticated = true;

    m_authSession = std::make_shared<kungfu::NetworkSession>();
    m_authSession->player = std::make_shared<kungfu::NetworkPlayer>(
        m_authSession->ioContext, "127.0.0.1", "8080",
        false, 0, 0, true);
    m_authSession->player->connectAndJoin();

    auto session = m_authSession; 
    m_authSession->thread = std::thread([session]() {
        boost::asio::io_context::work work(session->ioContext);
        session->ioContext.run();
    });
}

void LoginScreen::drawContent(IRenderer& renderer) {
    renderer.drawText("KUNG-FU CHESS", {280.0f, 130.0f}, 42, m_theme.titleText);
    renderer.drawText("The Real-Time Chess Experience", {340.0f, 190.0f}, 14, m_theme.bodyText);

    drawGlassPanel(renderer, m_panelPos, m_panelSize);

    renderer.drawText("Account Authentication", {m_panelPos.x + 30.0f, m_panelPos.y + 40.0f}, 20, m_theme.titleText);
    renderer.drawLine({m_panelPos.x + 30.0f, m_panelPos.y + 60.0f}, {m_panelPos.x + 430.0f, m_panelPos.y + 60.0f}, {65, 68, 85, 120}, 1.0f);

    Color activeBorder{240, 200, 80, 255};
    Color inactiveBorder{50, 52, 65, 255};

    renderer.drawText("Username:", {m_usernamePos.x, m_usernamePos.y - 25.0f}, 13, m_theme.bodyText);
    Color userBorder = (m_activeField == ActiveField::Username) ? activeBorder : inactiveBorder;
    renderer.drawRectangle(m_usernamePos, m_inputSize, {18, 19, 23, 255}, true);
    renderer.drawRectangle(m_usernamePos, m_inputSize, userBorder, false);
    
    std::string userDisplay = m_usernameText;
    if (m_activeField == ActiveField::Username && (static_cast<int>(m_cursorTimer * 2.0f) % 2 == 0)) {
        userDisplay += "|";
    }
    renderer.drawText(userDisplay, {m_usernamePos.x + 12.0f, m_usernamePos.y + 28.0f}, 15, {255, 255, 255, 255});

    renderer.drawText("Password:", {m_passwordPos.x, m_passwordPos.y - 25.0f}, 13, m_theme.bodyText);
    Color passBorder = (m_activeField == ActiveField::Password) ? activeBorder : inactiveBorder;
    renderer.drawRectangle(m_passwordPos, m_inputSize, {18, 19, 23, 255}, true);
    renderer.drawRectangle(m_passwordPos, m_inputSize, passBorder, false);

    std::string passDisplay(m_passwordText.length(), '*');
    if (m_activeField == ActiveField::Password && (static_cast<int>(m_cursorTimer * 2.0f) % 2 == 0)) {
        passDisplay += "|";
    }
    renderer.drawText(passDisplay, {m_passwordPos.x + 12.0f, m_passwordPos.y + 28.0f}, 15, {255, 255, 255, 255});

    bool loginHovered = isPointInRect(m_mousePos, m_loginBtnPos, m_btnSize);
    bool registerHovered = isPointInRect(m_mousePos, m_registerBtnPos, m_btnSize);
    bool offlineHovered = isPointInRect(m_mousePos, m_offlineBtnPos, m_offlineBtnSize);

    drawButton(renderer, "   Login", m_loginBtnPos, m_btnSize, loginHovered);
    drawButton(renderer, "  Register", m_registerBtnPos, m_btnSize, registerHovered);

    Color offlineColor = offlineHovered ? Color{48, 120, 192, 180} : Color{45, 48, 56, 255};
    renderer.drawRectangle(m_offlineBtnPos, m_offlineBtnSize, offlineColor, true);
    renderer.drawRectangle(m_offlineBtnPos, m_offlineBtnSize, m_theme.border, false);
    renderer.drawText("Play Offline", {m_offlineBtnPos.x + 120.0f, m_offlineBtnPos.y + 31.0f}, 15, m_theme.bodyText);

    if (!m_statusMessage.empty()) {
        renderer.drawText(m_statusMessage, {m_panelPos.x + 30.0f, m_panelPos.y + 490.0f}, 12, m_statusColor);
    }
}

void LoginScreen::onEnter() {}
void LoginScreen::onExit() {}

void LoginScreen::update(float deltaTime) {
    tickBackground(deltaTime); 
    m_cursorTimer += deltaTime;

    if (!m_authPending) return;

    if (m_loginConnecting) {
        auto status = m_authSession->player->loginStatus();
        if (status == kungfu::NetworkPlayer::LoginStatus::Success) {
            m_authPending = false;
            m_loginConnecting = false;
            m_statusMessage = m_authSession->player->loginMessage();
            m_statusColor = {100, 210, 130, 255};
            m_screenManager.changeScreen(std::make_unique<StartScreen>(m_screenManager, m_soundPlayer, m_authSession));
        } else if (status == kungfu::NetworkPlayer::LoginStatus::Failed) {
            m_authPending = false;
            m_loginConnecting = false;
            m_statusMessage = m_authSession->player->loginMessage();
            m_statusColor = {240, 100, 100, 255};
            kungfu::ClientAuth::reset();
            m_authSession.reset(); 
        }
    } else if (m_authFuture.valid() && m_authFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        auto result = m_authFuture.get();
        m_authPending = false;
        m_statusMessage = result.success ? (result.message + " You can now log in.") : result.message;
        m_statusColor = result.success ? Color{100, 210, 130, 255} : Color{240, 100, 100, 255};
    }
}

void LoginScreen::handleInput(const std::vector<InputEvent>& events) {
    if (m_authPending) return; 

    for (const auto& event : events) {
        if (event.type == InputEvent::Type::Mouse) {
            m_mousePos = {event.mouse.logicalX, event.mouse.logicalY};

            if (event.mouse.action == MouseEvent::Action::Press && event.mouse.button == MouseButton::Left) {
                if (isPointInRect(m_mousePos, m_usernamePos, m_inputSize)) {
                    m_activeField = ActiveField::Username;
                } else if (isPointInRect(m_mousePos, m_passwordPos, m_inputSize)) {
                    m_activeField = ActiveField::Password;
                }

                if (isPointInRect(m_mousePos, m_loginBtnPos, m_btnSize)) {
                    startAuth(false);
                } else if (isPointInRect(m_mousePos, m_registerBtnPos, m_btnSize)) {
                    startAuth(true);
                } else if (isPointInRect(m_mousePos, m_offlineBtnPos, m_offlineBtnSize)) {
                    kungfu::ClientAuth::reset();
                    m_screenManager.changeScreen(std::make_unique<StartScreen>(m_screenManager, m_soundPlayer));
                }
            }
        } 
        else if (event.type == InputEvent::Type::Keyboard) {
            if (event.key.key == Key::Tab) {
                m_activeField = (m_activeField == ActiveField::Username) ? ActiveField::Password : ActiveField::Username;
            } 
            else if (event.key.key == Key::Enter) {
                startAuth(false);
            } 
            else if (event.key.key == Key::Backspace) {
                std::string& activeStr = (m_activeField == ActiveField::Username) ? m_usernameText : m_passwordText;
                if (!activeStr.empty()) {
                    activeStr.pop_back();
                }
            } 
            else {
                char c = translateKeyToChar(event.key);
                if (c != '\0') {
                    std::string& activeStr = (m_activeField == ActiveField::Username) ? m_usernameText : m_passwordText;
                    if (activeStr.length() < 16) {
                        activeStr += c;
                    }
                }
            }
        }
    }
}