// ui/screens/LoginScreen.hpp
#pragma once
#include "ui/screens/BaseScreen.hpp"
#include "ui/framework/ISoundPlayer.hpp"
#include "ui/screens/StartScreen.hpp"
#include "ui/framework/ScreenManager.hpp"
#include "server/Serializer.hpp"
#include "server/ClientAuth.hpp"
#include <future>
#include <string>
#include <vector>
#include <chrono>
#include <boost/asio.hpp>

class LoginScreen : public BaseScreen {
private:
    std::string m_usernameText = "";
    std::string m_passwordText = "";
    
    enum class ActiveField { Username, Password };
    ActiveField m_activeField = ActiveField::Username;

    float m_cursorTimer = 0.0f;
    std::shared_ptr<ISoundPlayer> m_soundPlayer;
    bool m_isSfml = false;

    struct AuthResult {
        bool success;
        std::string message;
        int rating;
    };

    std::future<AuthResult> m_authFuture;
    bool m_authPending = false;
    std::string m_statusMessage = "";
    Color m_statusColor{210, 215, 225, 255}; 

    // הגדרות גיאומטריות של ממשק המשתמש
    const Vector2D m_inputSize{300.0f, 45.0f};
    const Vector2D m_usernamePos{350.0f, 350.0f};
    const Vector2D m_passwordPos{350.0f, 450.0f};

    const Vector2D m_btnSize{140.0f, 50.0f};
    const Vector2D m_loginBtnPos{350.0f, 540.0f};
    const Vector2D m_registerBtnPos{510.0f, 540.0f};
    
    const Vector2D m_offlineBtnSize{300.0f, 50.0f};
    const Vector2D m_offlineBtnPos{350.0f, 610.0f};

    Vector2D m_mousePos{0.0f, 0.0f};

    // ביצוע קריאת הרשת בצורה אסינכרונית כדי לא לעצור את לולאת המשחק הראשית
    static AuthResult performNetworkAuth(const std::string& username, const std::string& password, bool isRegister) {
        AuthResult res{false, "Connection error", 1200};
        try {
            boost::asio::io_context ioContext;
            boost::asio::ip::udp::socket socket(ioContext);
            boost::asio::ip::udp::resolver resolver(ioContext);
            
            boost::system::error_code ec;
            auto endpoints = resolver.resolve("127.0.0.1", "8080", ec);
            if (ec) {
                res.message = "Could not resolve server address";
                return res;
            }

            socket.open(boost::asio::ip::udp::v4(), ec);
            if (ec) {
                res.message = "Could not open UDP socket";
                return res;
            }

            // חיבור לוגי של סוקט ה-UDP לנקודת הקצה של השרת
            socket.connect(*endpoints.begin(), ec);
            if (ec) {
                res.message = "Server is offline";
                return res;
            }

            auto payload = kungfu::Serializer::serializeAuthRequest(username, password);
            auto type = isRegister ? kungfu::NetworkMessageType::REGISTER_REQUEST : kungfu::NetworkMessageType::LOGIN_REQUEST;
            auto frame = kungfu::Serializer::buildFrame(type, payload);

            // שליחת חבילת UDP
            socket.send(boost::asio::buffer(frame), 0, ec);
            if (ec) {
                res.message = "Failed to send credentials";
                return res;
            }

            // קבלת תשובה ב-UDP (מכיוון שזה קורה על Thread נפרד, קריאה חוסמת לא תפריע לתצוגה)
            std::vector<std::uint8_t> recvBuf(kungfu::kMaxPayloadSize);
            std::size_t bytesRecvd = socket.receive(boost::asio::buffer(recvBuf), 0, ec);
            if (ec || bytesRecvd < kungfu::kHeaderSize) {
                res.message = "No response from server";
                return res;
            }

            std::size_t offset = 0;
            std::uint8_t resType = 0;
            std::uint32_t payloadSize = 0;
            kungfu::Serializer::readU8(recvBuf, offset, resType);
            kungfu::Serializer::readU32(recvBuf, offset, payloadSize);

            std::vector<std::uint8_t> responsePayload(
                recvBuf.begin() + offset, 
                recvBuf.begin() + offset + payloadSize
            );

            if (resType == static_cast<std::uint8_t>(kungfu::NetworkMessageType::REGISTER_RESPONSE)) {
                if (!responsePayload.empty() && responsePayload[0] == 1) {
                    res.success = true;
                    res.message = "Registration successful! You can now login.";
                } else {
                    res.message = "Registration failed. Username taken.";
                }
            }
            else if (resType == static_cast<std::uint8_t>(kungfu::NetworkMessageType::LOGIN_RESPONSE)) {
                if (!responsePayload.empty() && responsePayload[0] == 1) {
                    std::size_t readOffset = 1;
                    std::uint32_t rating = 1200;
                    kungfu::Serializer::readU32(responsePayload, readOffset, rating);
                    
                    res.success = true;
                    res.rating = static_cast<int>(rating);
                    res.message = "Success! Access granted.";
                } else {
                    res.message = "Login failed. Invalid password.";
                }
            }
        } catch (const std::exception& e) {
            res.message = e.what();
        }
        return res;
    }

    char translateKeyToChar(const KeyEvent& keyEvent) const {
        int code = keyEvent.rawCode;
        if (m_isSfml) {
            if (code >= 0 && code <= 25) {
                return 'a' + code;
            }
            if (code >= 26 && code <= 35) {
                return '0' + (code - 26);
            }
            if (code == 57) {
                return ' ';
            }
            return '\0';
        } else {
            char c = static_cast<char>(code & 0xFF);
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || 
                c == ' ' || c == '.' || c == '_' || c == '-') {
                return c;
            }
            return '\0';
        }
    }

    void startAuth(bool isRegister) {
        if (m_usernameText.empty() || m_passwordText.empty()) {
            m_statusMessage = "Please enter both username and password.";
            m_statusColor = {240, 100, 100, 255}; 
            return;
        }

        m_statusMessage = isRegister ? "Registering account..." : "Connecting to server...";
        m_statusColor = {240, 200, 80, 255}; 
        m_authPending = true;

        m_authFuture = std::async(std::launch::async, [this, isRegister]() {
            return performNetworkAuth(m_usernameText, m_passwordText, isRegister);
        });
    }

protected:
    void drawContent(IRenderer& renderer) override {
        renderer.drawText("KUNG-FU CHESS", {280.0f, 200.0f}, 42, m_theme.titleText);
        renderer.drawText("Account Authentication Screen", {330.0f, 260.0f}, 16, m_theme.bodyText);

        Color activeBorder{240, 200, 80, 255};
        Color inactiveBorder{80, 80, 90, 255};

        // תיבת שם משתמש
        renderer.drawText("Username:", {350.0f, 335.0f}, 14, m_theme.bodyText);
        Color userBorder = (m_activeField == ActiveField::Username) ? activeBorder : inactiveBorder;
        renderer.drawRectangle(m_usernamePos, m_inputSize, {30, 31, 38, 255}, true);
        renderer.drawRectangle(m_usernamePos, m_inputSize, userBorder, false);
        
        std::string userDisplay = m_usernameText;
        if (m_activeField == ActiveField::Username && (static_cast<int>(m_cursorTimer * 2.0f) % 2 == 0)) {
            userDisplay += "|";
        }
        renderer.drawText(userDisplay, {m_usernamePos.x + 12.0f, m_usernamePos.y + 28.0f}, 15, {255, 255, 255, 255});

        // תיבת סיסמה
        renderer.drawText("Password:", {350.0f, 435.0f}, 14, m_theme.bodyText);
        Color passBorder = (m_activeField == ActiveField::Password) ? activeBorder : inactiveBorder;
        renderer.drawRectangle(m_passwordPos, m_inputSize, {30, 31, 38, 255}, true);
        renderer.drawRectangle(m_passwordPos, m_inputSize, passBorder, false);

        std::string passDisplay(m_passwordText.length(), '*');
        if (m_activeField == ActiveField::Password && (static_cast<int>(m_cursorTimer * 2.0f) % 2 == 0)) {
            passDisplay += "|";
        }
        renderer.drawText(passDisplay, {m_passwordPos.x + 12.0f, m_passwordPos.y + 28.0f}, 15, {255, 255, 255, 255});

        bool loginHovered = isPointInRect(m_mousePos, m_loginBtnPos, m_btnSize);
        bool registerHovered = isPointInRect(m_mousePos, m_registerBtnPos, m_btnSize);
        bool offlineHovered = isPointInRect(m_mousePos, m_offlineBtnPos, m_offlineBtnSize);

        // ציור הכפתורים
        drawButton(renderer, "Login", m_loginBtnPos, m_btnSize, loginHovered);
        drawButton(renderer, "Register", m_registerBtnPos, m_btnSize, registerHovered);

        Color offlineColor = offlineHovered ? Color{70, 75, 85, 255} : Color{45, 48, 56, 255};
        renderer.drawRectangle(m_offlineBtnPos, m_offlineBtnSize, offlineColor, true);
        renderer.drawRectangle(m_offlineBtnPos, m_offlineBtnSize, m_theme.border, false);
        renderer.drawText("Play Offline", {m_offlineBtnPos.x + 95.0f, m_offlineBtnPos.y + 31.0f}, 16, m_theme.bodyText);

        if (!m_statusMessage.empty()) {
            renderer.drawText(m_statusMessage, {350.0f, 720.0f}, 13, m_statusColor);
        }
    }

public:
    LoginScreen(ScreenManager& manager, std::shared_ptr<ISoundPlayer> soundPlayer = nullptr, bool isSfml = false)
        : BaseScreen(manager, "Secure Login"), m_soundPlayer(soundPlayer), m_isSfml(isSfml) {
        m_theme.background = Color{18, 19, 23, 255};
        m_theme.titleText = Color{240, 200, 80, 255};
        m_theme.buttonNormal = Color{35, 37, 45, 255};
        m_theme.buttonHover = Color{48, 120, 192, 255};
        m_theme.border = Color{55, 58, 70, 255};
        m_theme.bodyText = Color{210, 215, 225, 255};
    }

    void onEnter() override {}
    void onExit() override {}

    void update(float deltaTime) override {
        m_cursorTimer += deltaTime;

        if (m_authPending && m_authFuture.valid()) {
            if (m_authFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                auto result = m_authFuture.get();
                m_authPending = false;
                m_statusMessage = result.message;

                if (result.success) {
                    m_statusColor = {100, 210, 130, 255}; // Green
                    
                    kungfu::ClientAuth::username = m_usernameText;
                    kungfu::ClientAuth::password = m_passwordText;
                    kungfu::ClientAuth::rating = result.rating;
                    kungfu::ClientAuth::isAuthenticated = true;

                    m_screenManager.changeScreen(std::make_unique<StartScreen>(m_screenManager, m_soundPlayer));
                } else {
                    m_statusColor = {240, 100, 100, 255}; // Red
                }
            }
        }
    }

    void handleInput(const std::vector<InputEvent>& events) override {
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
                        kungfu::ClientAuth::isAuthenticated = false;
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
};