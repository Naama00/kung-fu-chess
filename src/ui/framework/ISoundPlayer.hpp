#pragma once
#include <string>

class ISoundPlayer {
public:
    virtual ~ISoundPlayer() = default;
    virtual void playSound(const std::string& soundId) = 0;
    virtual void playLoop(const std::string& soundId) = 0; // השמעה בלופ רציף
    virtual void stopSound(const std::string& soundId) = 0; // עצירת סאונד ספציפי
};

// Null Object Pattern מעודכן
class NullSoundPlayer : public ISoundPlayer {
public:
    void playSound(const std::string& /*soundId*/) override {}
    void playLoop(const std::string& /*soundId*/) override {}
    void stopSound(const std::string& /*soundId*/) override {}
};