#pragma once

#include "ui/framework/IRenderer.hpp"
#include "ui/framework/InputEvents.hpp"
#include <vector>
#include <cmath>
#include <cstdlib>

class ParticleSystem {
private:
    struct Particle {
        Vector2D position;
        Vector2D velocity;
        Color color;
        float life = 0.0f;
        float maxLife = 0.0f;
        float size = 0.0f;
    };

    std::vector<Particle> m_particles;

public:
    ParticleSystem() = default;
    ~ParticleSystem() = default;

    // Prevent copying to avoid unnecessary duplicate memory usage
    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;
    ParticleSystem(ParticleSystem&&) noexcept = default;
    ParticleSystem& operator=(ParticleSystem&&) noexcept = default;

    /**
     * Create a particle burst at a given position and base color.
     */
    void spawnExplosion(Vector2D center, Color baseColor) {
        // Create 35 particles in random directions
        for (int i = 0; i < 35; ++i) {
            Particle p;
            p.position = center;
            
            float angle = (static_cast<float>(std::rand()) / RAND_MAX) * 2.0f * 3.14159265f;
            float speed = 70.0f + (static_cast<float>(std::rand()) / RAND_MAX) * 190.0f;

            p.velocity = {
                std::cos(angle) * speed,
                std::sin(angle) * speed - 60.0f // a gentle upward push for the fountain effect
            };

            // Create a visually varied effect with gold and red sparks
            if (std::rand() % 4 == 0) {
                p.color = Color{240, 190, 70, 255}; // Gold spark
            } else if (std::rand() % 5 == 0) {
                p.color = Color{215, 60, 60, 255};  // Red friction spark
            } else {
                p.color = baseColor;
            }

            p.maxLife = 0.4f + (static_cast<float>(std::rand()) / RAND_MAX) * 0.5f;
            p.life = p.maxLife;
            p.size = 2.5f + (static_cast<float>(std::rand()) / RAND_MAX) * 5.0f;

            m_particles.push_back(p);
        }
    }

    /**
     * Update particle positions, apply gravity, and remove expired particles.
     */
    void update(float deltaTime) {
        for (auto it = m_particles.begin(); it != m_particles.end(); ) {
            it->life -= deltaTime;
            if (it->life <= 0.0f) {
                it = m_particles.erase(it);
            } else {
                it->position.x += it->velocity.x * deltaTime;
                it->position.y += it->velocity.y * deltaTime;
                it->velocity.y += 120.0f * deltaTime; // gravity
                ++it;
            }
        }
    }

    /**
     * רינדור כל החלקיקים הפעילים עם החלשת שקיפות הדרגתית.
     */
    void draw(IRenderer& renderer) const {
        for (const auto& p : m_particles) {
            Color fadeColor = p.color;
            fadeColor.a = static_cast<std::uint8_t>((p.life / p.maxLife) * 255);
            renderer.drawCircle(p.position, p.size, fadeColor, true);
        }
    }

    /**
     * ניקוי כל החלקיקים הפעילים מהמסך (למשל בעת ריסטארט)
     */
    void clear() {
        m_particles.clear();
    }
};