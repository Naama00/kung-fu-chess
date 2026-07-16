#pragma once
#include "ui/framework/AssetManager.hpp"
#include <SFML/Graphics.hpp>
#include <string>
#include <stdexcept>

// נכס תמונה/טקסטורה עבור ספריטים
class SfmlTextureAsset : public IAsset {
public:
    sf::Texture texture;

    explicit SfmlTextureAsset(const std::string& filePath) {
        if (!texture.loadFromFile(filePath)) {
            throw std::runtime_error("Failed to load SFML texture: " + filePath);
        }
        texture.setSmooth(true);
    }
};

// נכס פונט עבור כתיבת טקסט
class SfmlFontAsset : public IAsset {
public:
    sf::Font font;

    explicit SfmlFontAsset(const std::string& filePath) {
        // שינוי כאן: loadFromFile הוחלף ב-openFromFile ב-SFML 3
        if (!font.openFromFile(filePath)) {
            throw std::runtime_error("Failed to load SFML font: " + filePath);
        }
    }
};