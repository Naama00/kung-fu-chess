#pragma once
#include "ui/framework/IInputTranslator.hpp"
#include "ui/framework/InputEvents.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
#include <mutex>

class ImgInputTranslator : public IInputTranslator {
private:
    Vector2D m_logicalRange{1000.0f, 1000.0f};

    // מפתח לאחסון זמני של אירועי עכבר שמגיעים מה-Callback של OpenCV (המשתמש ב-Thread נפרד)
    static std::vector<InputEvent> s_pendingMouseEvents;
    static std::mutex s_eventsMutex;

    // פונקציית ה-Callback הסטטית שמתקבלת מ-OpenCV
    static void onMouse(int event, int x, int y, int flags, void* userdata) {
        (void)flags;
        auto* translator = static_cast<ImgInputTranslator*>(userdata);
        if (!translator) return;

        MouseEvent mouseEvent;
        // תרגום קואורדינטות פיזיות לקואורדינטות לוגיות
        Vector2D physicalSize = translator->getPhysicalWindowSize();

        mouseEvent.logicalX = (static_cast<float>(x) / physicalSize.x) * translator->m_logicalRange.x;
        mouseEvent.logicalY = (static_cast<float>(y) / physicalSize.y) * translator->m_logicalRange.y;

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
            std::lock_guard<std::mutex> lock(s_eventsMutex);
            InputEvent ie;
            ie.type = InputEvent::Type::Mouse;
            ie.mouse = mouseEvent;
            s_pendingMouseEvents.push_back(ie);
        }
    }

    Vector2D m_windowSize{800.0f, 800.0f}; // גודל פיזי ברירת מחדל של החלון

    // תרגום קוד מקש גולמי מ-OpenCV למקש לוגי
    static Key translateKey(int rawCode) {
        switch (rawCode & 0xFF) { // סינון מסכות קוד מקש בסיסי
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

    // רישום ה-Callback לחלון של OpenCV. זו פעולת אתחול חד-פעמית וספציפית
    // ל-backend, ולכן אינה חלק מ-IInputTranslator - main.cpp קורא לה על
    // הטיפוס הקונקרטי לפני תחילת הלולאה, בדיוק כמו יצירת החלון עצמו.
    void registerWindow(const std::string& windowName, Vector2D windowSize) {
        m_windowSize = windowSize;
        cv::setMouseCallback(windowName, onMouse, this);
    }

    Vector2D getPhysicalWindowSize() const {
        return m_windowSize;
    }

    void setPhysicalWindowSize(Vector2D size) {
        m_windowSize = size;
    }

    /**
     * מימוש בפועל של IInputTranslator::pollEvents.
     * אוסף את כל אירועי הקלט שהצטברו מהפריים האחרון (עכבר ומקלדת), כולל
     * הפעלת cv::waitKey הפנימית - פרט מימוש שמוסתר לגמרי מהקורא.
     */
    void pollEvents(std::vector<InputEvent>& outEvents) override {
        // 1. שליפת אירועי העכבר שהצטברו בצורה בטוחה מהתור המשותף עם ה-Callback
        {
            std::lock_guard<std::mutex> lock(s_eventsMutex);
            outEvents.insert(outEvents.end(), s_pendingMouseEvents.begin(), s_pendingMouseEvents.end());
            s_pendingMouseEvents.clear();
        }

        // 2. דגימת מקלדת - קריאת cv::waitKey הועברה לכאן, פנימה ל-Translator
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

// אתחול המשתנים הסטטיים של המחלקה
inline std::vector<InputEvent> ImgInputTranslator::s_pendingMouseEvents;
inline std::mutex ImgInputTranslator::s_eventsMutex;