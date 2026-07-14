// #include <iostream>
// #include <string>
// #include <vector>
// #include <sstream>
// #include <algorithm>
// #include <memory>
// #include <cctype>

// #include "board/Board.hpp"
// #include "io/BoardParser.hpp"
// #include "io/BoardPrinter.hpp"
// #include "rules/RuleEngine.hpp"
// #include "engine/GameEngine.hpp"
// #include "input/Controller.hpp"

// namespace {

// // פונקציית עזר להסרת רווחים מקצוות שורה
// std::string trim(const std::string& str) {
//     auto first = std::find_if_not(str.begin(), str.end(), [](unsigned char ch) {
//         return std::isspace(ch);
//     });
//     if (first == str.end()) {
//         return "";
//     }
//     auto last = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char ch) {
//         return std::isspace(ch);
//     }).base();
//     return std::string(first, last);
// }

// } // namespace

// int main() {
//     // הגדרת תמיכה באופטימיזציה של מהירות קלט/פלט במידת הצורך
//     std::ios_base::sync_with_stdio(false);
//     std::cin.tie(nullptr);

//     std::vector<std::string> allLines;
//     std::string line;
//     bool hasSections = false;

//     // קריאת קלט מלא משורת הפקודה / קובץ בדיקה
//     while (std::getline(std::cin, line)) {
//         std::string trimmed = trim(line);
//         if (trimmed == "Board:" || trimmed == "Commands:") {
//             hasSections = true;
//         }
//         allLines.push_back(trimmed);
//     }

//     // --- תרחיש א': שלב א' בלבד (קלט לוח ישיר ללא הגדרת פקודות) ---
//     if (!hasSections) {
//         std::string boardRaw;
//         for (const auto& l : allLines) {
//             if (!l.empty()) {
//                 boardRaw += l + "\n";
//             }
//         }

//         auto board = kungfu::BoardParser::parse(boardRaw);
//         if (!board) {
//             std::cout << "Invalid board\n";
//             return 1;
//         }

//         // הדפסת הלוח הלוגי כפי שהתפרסר
//         std::cout << kungfu::BoardPrinter::print(*board);
//         return 0;
//     }

//     // --- תרחיש ב': משחק מלא עם כותרות "Board:" ו-"Commands:" ---
//     std::string boardSection;
//     std::vector<std::string> commandLines;
//     bool readingBoard = false;
//     bool readingCommands = false;

//     for (const auto& l : allLines) {
//         if (l.empty()) {
//             continue;
//         }
//         if (l == "Board:") {
//             readingBoard = true;
//             readingCommands = false;
//             continue;
//         }
//         if (l == "Commands:") {
//             readingBoard = false;
//             readingCommands = true;
//             continue;
//         }

//         if (readingBoard) {
//             boardSection += l + "\n";
//         } else if (readingCommands) {
//             commandLines.push_back(l);
//         }
//     }

//     // 1. יצירת הלוח
//     auto board = kungfu::BoardParser::parse(boardSection);
//     if (!board) {
//         std::cout << "Invalid board\n";
//         return 1;
//     }

//     // 2. אתחול מנוע החוקים, מנוע המשחק והבקר (Controller)
//     auto ruleEngine = std::make_shared<kungfu::RuleEngine>(board);
//     auto gameEngine = std::make_shared<kungfu::GameEngine>(board, ruleEngine);
//     kungfu::Controller controller(gameEngine); 
//     // 3. עיבוד שורות פקודה
//     for (const auto& cmd : commandLines) {
//         std::istringstream stream(cmd);
//         std::string action;
//         stream >> action;

//         if (action == "click") {
//             int x = 0;
//             int y = 0;
//             if (stream >> x >> y) {
//                 controller.click(x, y);
//             }
//         } 
//         else if (action == "wait") {
//             int ms = 0;
//             if (stream >> ms) {
//                 gameEngine->wait(ms);
//             }
//         } 
//         else if (action == "print") {
//             std::string target;
//             if (stream >> target && target == "board") {
//                 std::cout << kungfu::BoardPrinter::print(*board);
//             }
//         }
//     }

//     return 0;
// }

// ------------------------------
// קובץ הMAIN שיוצר עבור הUI
// צריך לסנכרן אותו עם הלוגיקה- מעין איחוד של הMAIN -הקוד בקודם איתו
#include "graphics_impl/img_backend/ImgRenderer.hpp"
#include "graphics_impl/img_backend/ImgInputTranslator.hpp"
#include "ui/ScreenManager.hpp"
#include "img.hpp" // אובייקט ה-Img שלכם
#include <chrono>
#include <memory>
#include <iostream>

// מסך הדגמה בסיסי כדי לבדוק שהמערכת עובדת
class GameDemoScreen : public IScreen {
private:
    float m_rotation = 0.0f;
    Vector2D m_boxPos{500.0f, 500.0f};

public:
    explicit GameDemoScreen(ScreenManager& manager) : IScreen(manager) {}

    void onEnter() override {
        std::cout << "Demo Screen Entered!" << std::endl;
    }

    void onExit() override {
        std::cout << "Demo Screen Exited!" << std::endl;
    }

    void update(float deltaTime) override {
        // סיבוב ואנימציה פשוטה מבוססת זמן
        m_rotation += 45.0f * deltaTime; // 45 מעלות בשנייה
        if (m_rotation >= 360.0f) {
            m_rotation -= 360.0f;
        }
    }

    void draw(IRenderer& renderer) override {
        // 1. ציור רקע כהה
        renderer.clear({20, 20, 30, 255});

        // 2. ציור כותרת טקסט
        renderer.drawText("Chess System Engine", {350.0f, 100.0f}, 32, {255, 255, 255, 255});

        // 3. ציור מלבן מסתובב/נע במרכז
        renderer.drawRectangle({m_boxPos.x - 50.0f, m_boxPos.y - 50.0f}, {100.0f, 100.0f}, {0, 150, 255, 255}, true);
        
        // 4. ציור מעגל ייצוגי
        renderer.drawCircle({500.0f, 500.0f}, 150.0f, {0, 255, 150, 100}, false);
    }

    void handleInput(const std::vector<InputEvent>& events) override {
        for (const auto& event : events) {
            if (event.type == InputEvent::Type::Mouse) {
                const auto& mouse = event.mouse;
                // עדכון מיקום המלבן לפי מיקום העכבר הלוגי בעת תזוזה
                if (mouse.action == MouseEvent::Action::Move) {
                    m_boxPos = {mouse.logicalX, mouse.logicalY};
                }
            }
            else if (event.type == InputEvent::Type::Keyboard) {
                // לחיצה על Escape תסגור את המסך (ותצא מהתוכנית במידה וזה המסך האחרון)
                if (event.key.key == Key::Escape) {
                    m_screenManager.popScreen();
                }
            }
        }
    }
};

int main() {
    try {
        const std::string windowName = "Chess Game Window";
        const int windowWidth = 800;
        const int windowHeight = 800;

        // 1. יצירת קנבס הציור הראשי ואתחול המטריצה הפנימית שלו לגודל הרצוי
        Img screenCanvas;
        // עוקפים את הקריאה מהקובץ ומאתחלים ישירות מטריצה ריקה (BGRA)
        cv::Mat& rawMat = const_cast<cv::Mat&>(screenCanvas.get_mat());
        rawMat = cv::Mat::zeros(windowHeight, windowWidth, CV_8UC4);

        // 2. אתחול מנהלי התשתית
        ImgRenderer renderer(screenCanvas);
        ImgInputTranslator inputTranslator;
        ScreenManager screenManager;

        // רישום החלון לצורך קבלת אירועי עכבר מ-OpenCV
        cv::namedWindow(windowName, cv::WINDOW_AUTOSIZE);
        inputTranslator.registerWindow(windowName, {static_cast<float>(windowWidth), static_cast<float>(windowHeight)});

        // 3. טעינת מסך ברירת המחדל הראשון למערכת
        screenManager.changeScreen(std::make_unique<GameDemoScreen>(screenManager));

        // 4. משתני עזר לחישוב Delta Time
        auto previousTime = std::chrono::high_resolution_clock::now();
        bool running = true;

        std::cout << "Starting main loop..." << std::endl;

        while (running && !screenManager.isEmpty()) {
            // חישוב הזמן שחלף מהפריים הקודם (Delta Time)
            auto currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> elapsed = currentTime - previousTime;
            previousTime = currentTime;
            float deltaTime = elapsed.count();

            // אבטחת גבול עליון ל-Delta Time למניעת קפיצות פיזיקליות חדות במקרה של תקיעה זמנית
            if (deltaTime > 0.1f) deltaTime = 0.1f;

            // א. איסוף וניתוב אירועי קלט
            // waitKey(1) ממתין מילישנייה אחת לקלט מקלדת ומחזיר את הקוד שנלחץ
            int keyResult = cv::waitKey(1);
            std::vector<InputEvent> events = inputTranslator.pollEvents(keyResult);
            screenManager.handleInput(events);

            // ב. עדכון הלוגיקה
            screenManager.update(deltaTime);

            // ג. ציור הפריים הנוכחי
            renderer.beginFrame();
            screenManager.draw(renderer);
            renderer.endFrame();

            // ד. הצגת התמונה הסופית על המסך בעזרת OpenCV
            cv::imshow(windowName, screenCanvas.get_mat());

            // ה. בדיקה האם המשתמש סגר את החלון ישירות דרך כפתור ה-X של מערכת ההפעלה
            if (cv::getWindowProperty(windowName, cv::WND_PROP_VISIBLE) < 1.0) {
                running = false;
            }
        }

        cv::destroyAllWindows();
        std::cout << "Engine shut down cleanly." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Critical Error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
