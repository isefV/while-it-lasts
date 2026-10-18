#pragma once

/**
 * @file actor.hpp
 * @brief A character in a room: where it stands, where it faces, how it moves.
 *
 * Drawn from its sprite sheet when there is one, otherwise as a coloured
 * rectangle shaped by its pose (standing, sitting, lying, kneeling).
 */

#include <hollow/objects/character.hpp>
#include <hollow/objects/sprite.hpp>

#include <raylib.h>

#include <optional>
#include <string>

namespace hollow
{
    class Actor
    {
    public:
        const CharacterDef* def = nullptr;
        const SpriteSheet* sheet = nullptr;

        float x = 0.0f;               ///< feet, scene pixels
        float ground = 150.0f;
        int facing = 1;               ///< 1 = right, -1 = left
        std::string pose = "idle";    ///< animation while not walking: idle, sit, lying, kneel, drag, dragged, crouch, swing_*
        float speedScale = 1.0f;
        bool visible = true;

        /// Walks towards @p x at its own speed.
        void walkTo(float targetX);
        void stop();
        [[nodiscard]] bool walking() const { return target_.has_value(); }

        /// Moves by @p dx this frame (player control); marks it as moving.
        void step(float dx);

        void update(float dt);
        void draw(Color tint = WHITE) const;

        /// Where the placeholder is drawn; also what the player must stand next to.
        [[nodiscard]] Rectangle bounds() const;
        [[nodiscard]] float speed() const;
        [[nodiscard]] bool moved() const { return moved_; }

        /// True on the frame a foot lands: the animation reached one of its
        /// "steps" frames while moving.
        [[nodiscard]] bool footfall() const { return footfall_; }

        /// Where smoke rises from in the frame showing (its "emit"), scene
        /// pixels; nothing when it has none.
        [[nodiscard]] std::optional<Vector2> emitPoint() const;

    private:
        std::optional<float> target_;
        bool moved_ = false;
        bool steppedThisFrame_ = false;
        bool footfall_ = false;
        Animator animator_;
    };
} // namespace hollow
