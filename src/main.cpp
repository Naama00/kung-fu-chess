// src/main.cpp
#include "graphics_impl/img_backend/ImgRenderer.hpp"
#include "graphics_impl/img_backend/ImgInputTranslator.hpp"
#include "ui/framework/ScreenManager.hpp"
#include "core/view/screens/ChessGameScreen.hpp" 
#include "graphics_impl/img.hpp"
#include <chrono>
#include <memory>
#include <iostream>

int main() {
    try {
        const std::string windowName = "Kung-Fu Chess Game";
        const int windowWidth = 800;
        const int windowHeight = 800;

        // 1. אתחול קנבס הציור הראשי של חלון ה-OpenCV
        Img screenCanvas;
        screenCanvas.mat() = cv::Mat::zeros(windowHeight, windowWidth, CV_8UC4);

        // 2. אתחול מנהלי התשתית
        ImgRenderer renderer(screenCanvas);
        ImgInputTranslator inputTranslator;
        ScreenManager screenManager;

        // 3. רישום החלון לקבלת אירועי עכבר ומקלדת מ-OpenCV
        cv::namedWindow(windowName, cv::WINDOW_AUTOSIZE);
        inputTranslator.registerWindow(windowName, {static_cast<float>(windowWidth), static_cast<float>(windowHeight)});

        // 4. טעינת מסך השחמט האינטראקטיבי כמסך הראשי
        screenManager.changeScreen(std::make_unique<ChessGameScreen>(screenManager));

        // הצגת פריים ריק ראשוני כדי שהחלון יהיה גלוי לפני הכניסה ללולאה.
        // בלי זה, WND_PROP_VISIBLE מחזיר 0 בפריים הראשון ויוצא מיד.
        cv::imshow(windowName, screenCanvas.get_mat());
        cv::waitKey(1); // נותן ל-OS להציג את החלון לפני שנבדוק visibility

        // 5. לולאת המשחק הראשית (Main Game Loop) בזמן אמת
        auto previousTime = std::chrono::high_resolution_clock::now();
        bool running = true;

        std::cout << "Starting main loop..." << std::endl;

        while (running && !screenManager.isEmpty()) {
            // חישוב Delta Time
            auto currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> elapsed = currentTime - previousTime;
            previousTime = currentTime;
            float deltaTime = elapsed.count();
            if (deltaTime > 0.1f) deltaTime = 0.1f;

            // א. איסוף וניתוב אירועי קלט
            int keyResult = cv::waitKey(1);
            std::vector<InputEvent> events = inputTranslator.pollEvents(keyResult);
            screenManager.handleInput(events);

            // ב. עדכון לוגיקה פיזיקלית וצינון של מנוע המשחק
            screenManager.update(deltaTime);

            // ג. רינדור הפריים הנוכחי אל ה-Canvas
            renderer.beginFrame();
            screenManager.draw(renderer);
            renderer.endFrame();

            // ד. הצגת התמונה הסופית על גבי החלון בעזרת OpenCV
            cv::imshow(windowName, screenCanvas.get_mat());

            // ה. בדיקת סגירת החלון על ידי מערכת ההפעלה
            if (cv::getWindowProperty(windowName, cv::WND_PROP_VISIBLE) < 1.0) {
                running = false;
            }
        }
        std::cout << "Loop exited. running=" << running << " isEmpty=" << screenManager.isEmpty() << std::endl;

        cv::destroyAllWindows();
        std::cout << "Game Engine shut down cleanly." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Critical Error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}