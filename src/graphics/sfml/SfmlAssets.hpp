#pragma once
#include "ui/framework/AssetManager.hpp"
#include <SFML/Graphics.hpp>
#include <string>
#include <stdexcept>

// Image/texture asset for sprites
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

// Font asset for text rendering
class SfmlFontAsset : public IAsset {
public:
    sf::Font font;

    explicit SfmlFontAsset(const std::string& filePath) {
        if (!font.openFromFile(filePath)) {
            throw std::runtime_error("Failed to load SFML font: " + filePath);
        }
    }
};