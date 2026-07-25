#pragma once

namespace kungfu {

// Configuration settings for the game engine, including movement speeds, cooldown durations, and gameplay options.
struct GameConfig {
    int cooldownDurationMs = 2000;
    int msPerCellSpeed = 250; // Optimized speed for smooth and responsive real-time movement
    int jumpDurationMs = 600;

    bool allowSimultaneousMovement = true;
    bool enablePremoves = true;
    bool allowJumping = true; 
};

}  // namespace kungfu