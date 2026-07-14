#pragma once
#include "../../ui/InputEvents.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
#include <mutex>

class ImgInputTranslator {
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

public:
    ImgInputTranslator() = default;

    // רישום ה-Callback לחלון של OpenCV
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
     * אוסף את כל אירועי הקלט שהצטברו מהפריים האחרון (עכבר ומקלדת).
     * @param keyResult תוצאת הקריאה ל-cv::waitKey בפריים הנוכחי (או -1 אם לא נלחץ מקש).
     * @return וקטור המכיל את כל אירועי הקלט המתורגמים.
     */
    std::vector<InputEvent> pollEvents(int keyResult) {
        std::vector<InputEvent> processedEvents;

        // 1. שליפת אירועי העכבר שהצטברו בצורה בטוחה מרובוטיקת ה-Thread של ה-Callback
        {
            std::lock_guard<std::mutex> lock(s_eventsMutex);
            processedEvents = std::move(s_pendingMouseEvents);
            s_pendingMouseEvents.clear();
        }

        // 2. תרגום אירוע מקלדת (במידה ונלחץ מקש ב-OpenCV)
        if (keyResult != -1) {
            KeyEvent keyEvent;
            keyEvent.rawCode = keyResult;
            
            // מיפוי בסיסי של מקשים שימושיים מתוך קודי ה-ASCII ש-OpenCV מחזיר
            switch (keyResult & 0xFF) { // סינון מסכות קוד מקש בסיסי
                case 27: // Escape key
                    keyEvent.key = Key::Escape;
                    break;
                case 32: // Space
                    keyEvent.key = Key::Space;
                    break;
                case 'w': case 'W':
                    keyEvent.key = Key::W;
                    break;
                case 's': case 'S':
                    keyEvent.key = Key::S;
                    break;
                case 'a': case 'A':
                    keyEvent.key = Key::A;
                    break;
                case 'd': case 'D':
                    keyEvent.key = Key::D;
                    break;
                case 81: // קוד מקש שמאלה במערכות מסוימות
                    keyEvent.key = Key::Left;
                    break;
                case 83: // קוד מקש ימינה במערכות מסוימות
                    keyEvent.key = Key::Right;
                    break;
                default:
                    keyEvent.key = Key::Unknown;
                    break;
            }

            InputEvent ie;
            ie.type = InputEvent::Type::Keyboard;
            ie.key = keyEvent;
            processedEvents.push_back(ie);
        }

        return processedEvents;
    }
};

// אתחול המשתנים הסטטיים של המחלקה
inline std::vector<InputEvent> ImgInputTranslator::s_pendingMouseEvents;
inline std::mutex ImgInputTranslator::s_eventsMutex;