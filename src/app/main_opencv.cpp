// src/main.cpp
#include "graphics/opencv/ImgRenderer.hpp"
#include "graphics/opencv/ImgInputTranslator.hpp"
#include "ui/framework/ScreenManager.hpp"
#include "ui/framework/IRenderer.hpp"
#include "ui/framework/IInputTranslator.hpp"
#include "ui/screens/StartScreen.hpp"
#include "graphics/opencv/img.hpp"
#include <chrono>
#include <memory>
#include <iostream>
#define NOMINMAX
#include <windows.h>
#include <algorithm>

int main() {
    try {
        const std::string windowName = "Kung-Fu Chess Game";

        // חישוב גודל החלון המרבי שמתאים למסך — מביא בחשבון את ה-taskbar
        // ומשאיר מקום לכותרת החלון (titlebar ~30px).
        RECT workArea{};
        SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
        int availableWidth  = workArea.right  - workArea.left;
        int availableHeight = workArea.bottom - workArea.top;
        const int titleBarHeight = 30;
        int maxSide = std::min(availableWidth, availableHeight - titleBarHeight);

        const int windowWidth = 650;   
        const int windowHeight = 650;

        // 1. אתחול קנבס הציור הראשי של חלון ה-OpenCV
        Img screenCanvas;
        screenCanvas.mat() = cv::Mat::zeros(windowHeight, windowWidth, CV_8UC4);

        // 2. אתחול מנהלי התשתית - הטיפוסים הקונקרטיים חיים אך ורק כאן,
        //    ב-composition root. מרגע ההתחלה של הלולאה, כל שאר הקוד מדבר
        //    רק דרך IRenderer / IInputTranslator.
        ImgRenderer imgRenderer(screenCanvas, windowName);
        ImgInputTranslator imgInputTranslator;
        ScreenManager screenManager;

        IRenderer& renderer = imgRenderer;
        IInputTranslator& inputTranslator = imgInputTranslator;

        // 3. רישום החלון לקבלת אירועי עכבר ומקלדת מ-OpenCV (פעולת אתחול
        //    חד-פעמית וספציפית ל-backend - נשארת מול הטיפוס הקונקרטי)
        cv::namedWindow(windowName, cv::WINDOW_NORMAL);
        cv::resizeWindow(windowName, windowWidth, windowHeight);
        imgInputTranslator.registerWindow(windowName, {static_cast<float>(windowWidth), static_cast<float>(windowHeight)});

        // מיקום החלון במרכז המסך
        int screenWidth  = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        int windowX = (screenWidth  - windowWidth)  / 2;
        int windowY = (screenHeight - windowHeight) / 2;
        cv::moveWindow(windowName, windowX, windowY);

        // 4. טעינת מסך הפתיחה כמסך הראשי של האפליקציה 
        screenManager.changeScreen(std::make_unique<StartScreen>(screenManager));

        // טעינת תמונות הכלים לתוך ה-AssetManager של ה-Renderer.
        for (const std::string& color : {"w", "b"}) {
            for (const std::string& type : {"K", "Q", "R", "B", "N", "P"}) {
                std::string assetId = color + type;
                imgRenderer.getAssetManager().loadAsset<ImgTextureAsset>(
                    assetId, "assets/images/" + assetId + ".png"
                );
            }
        }

        // הצגת פריים ריק ראשוני כדי שהחלון יהיה גלוי לפני הכניסה ללולאה.
        renderer.presentFrame();
        cv::waitKey(1); // עדיין נחוץ פעם אחת - "מעוררת" את חלון OpenCV כדי שיצויר

        // 5. לולאת המשחק הראשית (Main Game Loop) בזמן אמת.
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

            // א. איסוף וניתוב אירועי קלט - דרך הממשק בלבד
            std::vector<InputEvent> events;
            inputTranslator.pollEvents(events);
            screenManager.handleInput(events);

            // ב. בדיקת סגירת החלון - דרך הממשק בלבד
            if (!renderer.isWindowOpen()) {
                running = false;
                break;
            }

            // ג. עדכון לוגיקה פיזיקלית וצינון של מנוע המשחק
            screenManager.update(deltaTime);

            // ד. רינדור והצגת הפריים הנוכחי - דרך הממשק בלבד
            renderer.beginFrame();
            screenManager.draw(renderer);
            renderer.endFrame();
            renderer.presentFrame();
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