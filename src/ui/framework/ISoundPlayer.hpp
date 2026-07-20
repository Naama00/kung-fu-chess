#pragma once
#include <string>

class ISoundPlayer {
public:
    virtual ~ISoundPlayer() = default;
    virtual void playSound(const std::string& soundId) = 0;
    virtual void playLoop(const std::string& soundId) = 0; // Play in a continuous loop
    virtual void stopSound(const std::string& soundId) = 0; // Stop a specific sound
};

// Updated Null Object Pattern
class NullSoundPlayer : public ISoundPlayer {
public:
    void playSound(const std::string& /*soundId*/) override {}
    void playLoop(const std::string& /*soundId*/) override {}
    void stopSound(const std::string& /*soundId*/) override {}
};