// app/main_opencv.cpp
#include "graphics/opencv/ImgRenderer.hpp"
#include "graphics/opencv/ImgInputTranslator.hpp"
#include "ui/framework/ScreenManager.hpp"
#include "ui/framework/IRenderer.hpp"
#include "ui/framework/IInputTranslator.hpp"
#include "ui/screens/LoginScreen.hpp" // ייבוא מסך ההתחברות החדש
#include "graphics/opencv/img.hpp"
#include <chrono>
#include <memory>
#include <iostream>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>

int main() {
    try {
        const std::string windowName = "Kung-Fu Chess Game";

        RECT workArea{};
        SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
        int availableWidth  = workArea.right  - workArea.left;
        int availableHeight = workArea.bottom - workArea.top;
        const int titleBarHeight = 30;
        int maxSide = std::min(availableWidth, availableHeight - titleBarHeight);

        const int windowWidth = 650;   
        const int windowHeight = 650;

        Img screenCanvas;
        screenCanvas.mat() = cv::Mat::zeros(windowHeight, windowWidth, CV_8UC4);

        ImgRenderer imgRenderer(screenCanvas, windowName);
        ImgInputTranslator imgInputTranslator;
        ScreenManager screenManager;

        IRenderer& renderer = imgRenderer;
        IInputTranslator& inputTranslator = imgInputTranslator;

        cv::namedWindow(windowName, cv::WINDOW_NORMAL);
        cv::resizeWindow(windowName, windowWidth, windowHeight);
        imgInputTranslator.registerWindow(windowName, {static_cast<float>(windowWidth), static_cast<float>(windowHeight)});

        int screenWidth  = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        int windowX = (screenWidth  - windowWidth)  / 2;
        int windowY = (screenHeight - windowHeight) / 2;
        cv::moveWindow(windowName, windowX, windowY);

        // טעינת מסך הלוגין הויזואלי החדש (במעבר OpenCV, נסמן false)
        screenManager.changeScreen(std::make_unique<LoginScreen>(screenManager, nullptr, false));

        for (const std::string& color : {"w", "b"}) {
            for (const std::string& type : {"K", "Q", "R", "B", "N", "P"}) {
                std::string assetId = color + type;
                imgRenderer.getAssetManager().loadAsset<ImgTextureAsset>(
                    assetId, "assets/images/" + assetId + ".png"
                );
            }
        }

        renderer.presentFrame();
        cv::waitKey(1); 

        auto previousTime = std::chrono::high_resolution_clock::now();
        bool running = true;

        std::cout << "Starting main loop..." << std::endl;

        while (running && !screenManager.isEmpty()) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> elapsed = currentTime - previousTime;
            previousTime = currentTime;
            float deltaTime = elapsed.count();
            if (deltaTime > 0.1f) deltaTime = 0.1f;

            std::vector<InputEvent> events;
            inputTranslator.pollEvents(events);
            screenManager.handleInput(events);

            if (!renderer.isWindowOpen()) {
                running = false;
                break;
            }

            screenManager.update(deltaTime);

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