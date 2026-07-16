#include "graphics/sfml/SfmlRenderer.hpp"
#include "graphics/sfml/SfmlInputTranslator.hpp"
#include "ui/framework/ScreenManager.hpp"
#include "ui/framework/IRenderer.hpp"
#include "ui/framework/IInputTranslator.hpp"
#include "graphics/sfml/SfmlSoundPlayer.hpp"
#include "ui/screens/StartScreen.hpp"
#include <chrono>
#include <memory>
#include <iostream>

int main()
{
    try
    {
        const std::string windowName = "Kung-Fu Chess Game (SFML 3 Edition)";
        const unsigned int windowWidth = 800;
        const unsigned int windowHeight = 800;

        sf::ContextSettings settings;
        settings.antiAliasingLevel = 4;

        // ב-SFML 3 הבנאי של VideoMode מקבל את הממדים כוקטור יחיד, ויש להגדיר State מפורש (כמו Windowed או Fullscreen)
        sf::RenderWindow window(
            sf::VideoMode({windowWidth, windowHeight}),
            windowName,
            sf::Style::Default, // שינוי כאן
            sf::State::Windowed,
            settings);

         std::string defaultFontId = "default_font";
        SfmlRenderer sfmlRenderer(window, defaultFontId);
        SfmlInputTranslator sfmlInputTranslator(window);
        ScreenManager screenManager;

        IRenderer &renderer = sfmlRenderer;
        IInputTranslator &inputTranslator = sfmlInputTranslator;

        // --- יצירה וטעינה של נגן הסאונד של SFML ---
        auto soundPlayer = std::make_shared<SfmlSoundPlayer>();
        soundPlayer->loadSound("move", "assets/sounds/move.wav");
        soundPlayer->loadSound("capture", "assets/sounds/capture.wav");
        soundPlayer->loadSound("game_over", "assets/sounds/game_over.wav");
        soundPlayer->loadSound("walk", "assets/sounds/walk.wav");
        
        try
        {
            sfmlRenderer.getAssetManager().loadAsset<SfmlFontAsset>(defaultFontId, "assets/arial.ttf");
        }
        catch (const std::exception &e)
        {
            std::cerr << "Warning: Could not load default font: " << e.what() << std::endl;
        }

        sfmlRenderer.getAssetManager().loadAsset<SfmlTextureAsset>("wK", "assets/wK.png");
        sfmlRenderer.getAssetManager().loadAsset<SfmlTextureAsset>("wQ", "assets/wQ.png");
        sfmlRenderer.getAssetManager().loadAsset<SfmlTextureAsset>("wR", "assets/wR.png");
        sfmlRenderer.getAssetManager().loadAsset<SfmlTextureAsset>("wB", "assets/wB.png");
        sfmlRenderer.getAssetManager().loadAsset<SfmlTextureAsset>("wN", "assets/wN.png");
        sfmlRenderer.getAssetManager().loadAsset<SfmlTextureAsset>("wP", "assets/wP.png");

        sfmlRenderer.getAssetManager().loadAsset<SfmlTextureAsset>("bK", "assets/bK.png");
        sfmlRenderer.getAssetManager().loadAsset<SfmlTextureAsset>("bQ", "assets/bQ.png");
        sfmlRenderer.getAssetManager().loadAsset<SfmlTextureAsset>("bR", "assets/bR.png");
        sfmlRenderer.getAssetManager().loadAsset<SfmlTextureAsset>("bB", "assets/bB.png");
        sfmlRenderer.getAssetManager().loadAsset<SfmlTextureAsset>("bN", "assets/bN.png");
        sfmlRenderer.getAssetManager().loadAsset<SfmlTextureAsset>("bP", "assets/bP.png");

        screenManager.changeScreen(std::make_unique<StartScreen>(screenManager, soundPlayer));

        auto previousTime = std::chrono::high_resolution_clock::now();
        bool running = true;

        std::cout << "Starting SFML 3 main loop..." << std::endl;

        while (running && window.isOpen() && !screenManager.isEmpty())
        {
            auto currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> elapsed = currentTime - previousTime;
            previousTime = currentTime;
            float deltaTime = elapsed.count();
            if (deltaTime > 0.1f)
                deltaTime = 0.1f;

            std::vector<InputEvent> events;
            inputTranslator.pollEvents(events);
            screenManager.handleInput(events);

            screenManager.update(deltaTime);

            renderer.beginFrame();
            screenManager.draw(renderer);
            renderer.endFrame();
            renderer.presentFrame();
        }

        std::cout << "SFML Game loop exited cleanly." << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Critical Error in main: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}