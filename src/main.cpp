// src/main.cpp
#include "graphics_impl/img_backend/ImgRenderer.hpp"
#include "graphics_impl/img_backend/ImgInputTranslator.hpp"
#include "ui/framework/ScreenManager.hpp"
#include "core/view/screens/MainMenuScreen.hpp" // ייבוא מסך הפתיחה החדש במקום מסך המשחק הישיר
#include "graphics_impl/img.hpp"
#include <chrono>
#include <memory>
#include <iostream>

int main() {
    try {
        const std::string windowName = "Kung-Fu Chess Game";
        const int windowWidth = 650;  // גודל מותאם אישית יציב למניעת חריגות
        const int windowHeight = 650;

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

       // 4. טעינת מסך הפתיחה כמסך הראשי
        screenManager.changeScreen(std::make_unique<MainMenuScreen>(screenManager));

        // טעינת תמונות הכלים לתוך ה-AssetManager של ה-Renderer
        renderer.getAssetManager().loadAsset<ImgTextureAsset>("wK", "assets/wK.png");
        renderer.getAssetManager().loadAsset<ImgTextureAsset>("wQ", "assets/wQ.png");
        renderer.getAssetManager().loadAsset<ImgTextureAsset>("wR", "assets/wR.png");
        renderer.getAssetManager().loadAsset<ImgTextureAsset>("wB", "assets/wB.png");
        renderer.getAssetManager().loadAsset<ImgTextureAsset>("wN", "assets/wN.png");
        renderer.getAssetManager().loadAsset<ImgTextureAsset>("wP", "assets/wP.png");

        renderer.getAssetManager().loadAsset<ImgTextureAsset>("bK", "assets/bK.png");
        renderer.getAssetManager().loadAsset<ImgTextureAsset>("bQ", "assets/bQ.png");
        renderer.getAssetManager().loadAsset<ImgTextureAsset>("bR", "assets/bR.png");
        renderer.getAssetManager().loadAsset<ImgTextureAsset>("bB", "assets/bB.png");
        renderer.getAssetManager().loadAsset<ImgTextureAsset>("bN", "assets/bN.png");
        renderer.getAssetManager().loadAsset<ImgTextureAsset>("bP", "assets/bP.png");

        // הצגת פריים ריק ראשוני כדי שהחלון יהיה גלוי לפני הכניסה ללולאה.
        cv::imshow(windowName, screenCanvas.get_mat());
        cv::waitKey(1); // מעבד אירועים ראשוניים ומציג את החלון

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

            // ב. בדיקת סגירת החלון על ידי מערכת ההפעלה (מנגנון הגנה try-catch)
            bool windowExists = true;
            try {
                double prop = cv::getWindowProperty(windowName, cv::WND_PROP_AUTOSIZE);
                if (prop < 0) {
                    windowExists = false;
                }
            } catch (const cv::Exception&) {
                windowExists = false;
            }

            if (!windowExists) {
                running = false;
                break;
            }

            // ג. עדכון לוגיקה פיזיקלית וצינון של מנוע המשחק
            screenManager.update(deltaTime);

            // ד. רינדור הפריים הנוכחי אל ה-Canvas
            renderer.beginFrame();
            screenManager.draw(renderer);
            renderer.endFrame();

            // ה. הצגת התמונה הסופית על גבי החלון בעזרת OpenCV
            cv::imshow(windowName, screenCanvas.get_mat());
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