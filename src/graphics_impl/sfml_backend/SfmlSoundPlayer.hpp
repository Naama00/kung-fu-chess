#pragma once
#include "ui/framework/ISoundPlayer.hpp"
#include <SFML/Audio.hpp>
#include <unordered_map>
#include <list>
#include <memory>
#include <string>
#include <iostream>

class SfmlSoundPlayer : public ISoundPlayer {
private:
    std::unordered_map<std::string, sf::SoundBuffer> m_buffers;
    std::list<sf::Sound> m_sounds;
    
    // SFML 3 מחק את הבנאי הדיפולטיבי של sf::Sound. 
    // שימוש ב-unique_ptr פותר זאת ומאפשר עבודה חלקה עם מפות ושימוש בזיכרון בטוח
    std::unordered_map<std::string, std::unique_ptr<sf::Sound>> m_loops;

public:
    SfmlSoundPlayer() = default;

    void loadSound(const std::string& soundId, const std::string& filePath) {
        sf::SoundBuffer buffer;
        if (buffer.loadFromFile(filePath)) {
            m_buffers.emplace(soundId, std::move(buffer));
        } else {
            std::cerr << "Warning: Failed to load sound file: " << filePath << std::endl;
        }
    }

    void playSound(const std::string& soundId) override {
        auto it = m_buffers.find(soundId);
        if (it == m_buffers.end()) {
            return;
        }

        m_sounds.remove_if([](const sf::Sound& s) {
            return s.getStatus() == sf::Sound::Status::Stopped;
        });

        m_sounds.emplace_back(it->second);
        m_sounds.back().play();
    }

    void playLoop(const std::string& soundId) override {
        auto it = m_buffers.find(soundId);
        if (it == m_buffers.end()) return;

        auto loopIt = m_loops.find(soundId);
        if (loopIt != m_loops.end() && loopIt->second) {
            if (loopIt->second->getStatus() == sf::Sound::Status::Playing) {
                return;
            }
        }

        // יצירה דינמית מנוהלת של ה-Sound באמצעות הבנאי של SFML 3 המחייב קבלת Buffer
        auto sound = std::make_unique<sf::Sound>(it->second);
        sound->setLooping(true);
        sound->play();
        m_loops[soundId] = std::move(sound);
    }

    void stopSound(const std::string& soundId) override {
        auto it = m_loops.find(soundId);
        if (it != m_loops.end() && it->second) {
            it->second->stop();
            m_loops.erase(it);
        }
    }
};