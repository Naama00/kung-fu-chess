#pragma once
#include "ui/framework/IInputTranslator.hpp"
#include "ui/framework/InputEvents.hpp"
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <vector>
#include <mutex>
#include <string>

class ImgInputTranslator : public IInputTranslator {
private:
    Vector2D m_logicalRange{1000.0f, 1000.0f};
    std::string m_windowName;
    Vector2D m_contentSize{1000.0f, 1000.0f};

    // התור הפך להיות משתנה חבר מופע (instance member) רגיל ולא סטטי!
    std::vector<InputEvent> m_pendingMouseEvents;
    std::mutex m_eventsMutex;

    // ה-Callback הסטטי של OpenCV מקבל את ה-this כ-userdata ומנתב ישירות למופע המתאים
    static void onMouse(int event, int x, int y, int flags, void* userdata) {
        (void)flags;
        auto* translator = static_cast<ImgInputTranslator*>(userdata);
        if (!translator) return;

        translator->handleMouseCallback(event, x, y);
    }

    void handleMouseCallback(int event, int x, int y) {
        MouseEvent mouseEvent;

        cv::Rect winRect = cv::getWindowImageRect(m_windowName);
        int winW = winRect.width;
        int winH = winRect.height;
        int contentW = static_cast<int>(m_contentSize.x);
        int contentH = static_cast<int>(m_contentSize.y);

        if (winW > 0 && winH > 0 && contentW > 0 && contentH > 0) {
            float scale = std::min(static_cast<float>(winW) / contentW,
                                   static_cast<float>(winH) / contentH);
            float padX = (winW - contentW * scale) / 2.0f;
            float padY = (winH - contentH * scale) / 2.0f;

            mouseEvent.logicalX = (x - padX) / scale;
            mouseEvent.logicalY = (y - padY) / scale;
        } else {
            mouseEvent.logicalX = static_cast<float>(x);
            mouseEvent.logicalY = static_cast<float>(y);
        }

        bool relevantEvent = false;

        switch (event) {
            case cv::EVENT_MOUSEMOVE:
                mouseEvent.action = MouseEvent::Action::Move;
                mouseEvent.button = MouseButton::None;
                relevantEvent = true;
                break;
            case cv::EVENT_LBUTTONDOWN:
                mouseEvent.action = MouseEvent::Action::Press;
                mouseEvent.button = MouseButton::Left;
                relevantEvent = true;
                break;
            case cv::EVENT_LBUTTONUP:
                mouseEvent.action = MouseEvent::Action::Release;
                mouseEvent.button = MouseButton::Left;
                relevantEvent = true;
                break;
            case cv::EVENT_RBUTTONDOWN:
                mouseEvent.action = MouseEvent::Action::Press;
                mouseEvent.button = MouseButton::Right;
                relevantEvent = true;
                break;
            case cv::EVENT_RBUTTONUP:
                mouseEvent.action = MouseEvent::Action::Release;
                mouseEvent.button = MouseButton::Right;
                relevantEvent = true;
                break;
            default:
                break;
        }

        if (relevantEvent) {
            std::lock_guard<std::mutex> lock(m_eventsMutex);
            InputEvent ie;
            ie.type = InputEvent::Type::Mouse;
            ie.mouse = mouseEvent;
            m_pendingMouseEvents.push_back(ie);
        }
    }

    Vector2D m_windowSize{800.0f, 800.0f};

    static Key translateKey(int rawCode) {
        switch (rawCode & 0xFF) {
            case 27: return Key::Escape;
            case 32: return Key::Space;
            case 'w': case 'W': return Key::W;
            case 's': case 'S': return Key::S;
            case 'a': case 'A': return Key::A;
            case 'd': case 'D': return Key::D;
            default: return Key::Unknown;
        }
    }

public:
    ImgInputTranslator() = default;

    void registerWindow(const std::string& windowName, Vector2D windowSize, Vector2D contentSize = {1000.0f, 1000.0f}) {
        m_windowName = windowName;
        m_windowSize = windowSize;
        m_contentSize = contentSize;
        // העברת ה-this מצביע כארגומנט האחרון
        cv::setMouseCallback(windowName, onMouse, this);
    }

    Vector2D getPhysicalWindowSize() const {
        return m_windowSize;
    }

    void setPhysicalWindowSize(Vector2D size) {
        m_windowSize = size;
    }

    void pollEvents(std::vector<InputEvent>& outEvents) override {
        {
            std::lock_guard<std::mutex> lock(m_eventsMutex);
            outEvents.insert(outEvents.end(), m_pendingMouseEvents.begin(), m_pendingMouseEvents.end());
            m_pendingMouseEvents.clear();
        }

        int keyResult = cv::waitKey(1);
        if (keyResult != -1) {
            KeyEvent keyEvent;
            keyEvent.rawCode = keyResult;
            keyEvent.key = translateKey(keyResult);

            InputEvent ie;
            ie.type = InputEvent::Type::Keyboard;
            ie.key = keyEvent;
            outEvents.push_back(ie);
        }
    }
};