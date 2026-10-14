#pragma once

/**
 * @file effects.hpp
 * @brief Screen and scene effects: fades, shake, firelight, apparitions,
 * particles.
 */

#include <hollow/utility/random.hpp>

#include <raylib.h>

#include <vector>

namespace hollow
{
    /// A value that moves to a target over a given time (0 = clear, 1 = black).
    class Fade
    {
    public:
        void to(float target, float seconds);
        void set(float value);
        void update(float dt);

        [[nodiscard]] float value() const { return value_; }
        [[nodiscard]] bool done() const { return value_ == target_; }

        /// Covers @p area with @p color at the fade's strength.
        void draw(Rectangle area, Color color = BLACK) const;

    private:
        float value_ = 0.0f;
        float target_ = 0.0f;
        float speed_ = 1.0f;
    };

    /// Whole-pixel jitter for a moment (spotted, a loud crack).
    class Shake
    {
    public:
        void start(float seconds, float strength);
        void update(float dt, Random& random);
        [[nodiscard]] Vector2 offset() const { return offset_; }

    private:
        float time_ = 0.0f;
        float strength_ = 0.0f;
        Vector2 offset_{};
    };

    /// The unsteady brightness of a flame, around 1.
    class Flicker
    {
    public:
        float update(float dt, Random& random);
        [[nodiscard]] float value() const { return value_; }

    private:
        float time_ = 0.0f;
        float value_ = 1.0f;
        float jitter_ = 0.0f;
    };

    /// Soft additive light (fire, ember); @p strength 0 to 1.
    void drawGlow(Vector2 center, float radius, Color color, float strength);

    /// A shape that fades in and out and drifts: the Reaper, X's spirit, a
    /// silhouette between the trees.
    class Apparition
    {
    public:
        Rectangle rect{};             ///< position and size (scene pixels)
        Color color{8, 8, 12, 255};
        const Texture2D* texture = nullptr;

        void fadeTo(float alpha, float seconds);
        void moveTo(Vector2 position, float seconds);
        void update(float dt);
        void draw() const;

        [[nodiscard]] float alpha() const { return alpha_.value(); }
        [[nodiscard]] bool visible() const { return alpha_.value() > 0.0f; }

    private:
        Fade alpha_;
        Vector2 from_{};
        Vector2 to_{};
        float moveTime_ = 0.0f;
        float moveLength_ = 0.0f;
    };

    /// Small pixels that rise, drift and fade (embers, ash).
    class Particles
    {
    public:
        struct Style
        {
            Color color{224, 135, 58, 255};
            Vector2 velocityMin{-4, -18};
            Vector2 velocityMax{4, -8};
            float life = 1.4f;
            float size = 1.0f;
        };

        void emit(Vector2 position, int count, const Style& style, Random& random);
        void update(float dt);
        void draw() const;
        void clear() { particles_.clear(); }

    private:
        struct Particle
        {
            Vector2 position;
            Vector2 velocity;
            float life;
            float age;
            float size;
            Color color;
        };
        std::vector<Particle> particles_;
    };
} // namespace hollow
