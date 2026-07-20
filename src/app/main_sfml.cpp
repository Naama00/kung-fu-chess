// app/main_sfml.cpp
#include "graphics/sfml/SfmlRenderer.hpp"
#include "graphics/sfml/SfmlInputTranslator.hpp"
#include "ui/framework/ScreenManager.hpp"
#include "ui/framework/IRenderer.hpp"
#include "ui/framework/IInputTranslator.hpp"
#include "graphics/sfml/SfmlSoundPlayer.hpp"
#include "ui/screens/LoginScreen.hpp" 
#include "server/network/NetworkMessages.hpp"
#include "server/network/Serializer.hpp"   
#include "players/network/ClientAuth.hpp"    
#include <chrono>
#include <memory>
#include <iostream>
#include <string>

int main()
{
    try
    {
        const std::string windowName = "Kung-Fu Chess Game (SFML 3 Edition)";
        const unsigned int windowWidth = 800;
        const unsigned int windowHeight = 800;

        sf::ContextSettings settings;
        settings.antiAliasingLevel = 4;

        sf::RenderWindow window(
            sf::VideoMode({windowWidth, windowHeight}),
            windowName,
            sf::Style::Default, 
            sf::State::Windowed,
            settings);

        std::string defaultFontId = "default_font";
        SfmlRenderer sfmlRenderer(window, defaultFontId);
        SfmlInputTranslator sfmlInputTranslator(window);
        ScreenManager screenManager;

        IRenderer &renderer = sfmlRenderer;
        IInputTranslator &inputTranslator = sfmlInputTranslator;

        auto soundPlayer = std::make_shared<SfmlSoundPlayer>();
        soundPlayer->loadSound("move", "assets/sounds/move.wav");
        soundPlayer->loadSound("capture", "assets/sounds/capture.wav");
        soundPlayer->loadSound("game_over", "assets/sounds/game_over.wav");
        soundPlayer->loadSound("walk", "assets/sounds/walk.wav");
        
        try
        {
            sfmlRenderer.getAssetManager().loadAsset<SfmlFontAsset>(defaultFontId, "assets/fonts/arial.ttf");
        }
        catch (const std::exception &e)
        {
            std::cerr << "Warning: Could not load default font: " << e.what() << std::endl;
        }

        // Loading 12 three-dimensional (3D) pieces from assets/images/
        for (const std::string& color : {"w", "b"}) {
            for (const std::string& type : {"K", "Q", "R", "B", "N", "P"}) {
                std::string assetId = color + type;
                sfmlRenderer.getAssetManager().loadAsset<SfmlTextureAsset>(
                    assetId, "assets/images/" + assetId + ".png"
                );
            }
        }

        // Loading board textures (wood/marble) when available to prevent a crash if a file is missing
        try
        {
            sfmlRenderer.getAssetManager().loadAsset<SfmlTextureAsset>("tile_light", "assets/images/wood_light.png");
            sfmlRenderer.getAssetManager().loadAsset<SfmlTextureAsset>("tile_dark", "assets/images/wood_dark.png");
        }
        catch (const std::exception &e)
        {
            std::cerr << "Note: Board tiles wood_light/dark not loaded, using programmatic 3D tiles: " << e.what() << std::endl;
        }

        // Load the new visual login screen
        screenManager.changeScreen(std::make_unique<LoginScreen>(screenManager, soundPlayer, true));

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