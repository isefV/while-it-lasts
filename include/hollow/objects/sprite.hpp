#pragma once

/**
 * @file sprite.hpp
 * @brief Sprite sheets and their animations (visual-asset-config.txt, section 5).
 *
 * Each sheet has a config file beside it, sprites/y.json:
 *
 *     {
 *       "texture": "sprites/y.png",
 *       "frame":  { "w": 32, "h": 48 },
 *       "origin": { "x": 16, "y": 48 },
 *       "animations": { "idle": { "row": 0, "frames": 2, "fps": 2, "loop": true }, ... }
 *     }
 */

#include <hollow/utility/diagnostics.hpp>

#include <raylib.h>

#include <map>
#include <optional>
#include <string>

namespace hollow
{
    class Assets;

    struct Animation
    {
        int row = 0;
        int frames = 1;
        float fps = 1.0f;
        bool loop = true;
    };

    struct SpriteSheet
    {
        const Texture2D* texture = nullptr; ///< nullptr while the image does not exist
        int frameWidth = 32;
        int frameHeight = 48;
        Vector2 origin{16, 48}; ///< the feet, inside a frame
        std::map<std::string, Animation, std::less<>> animations;
    };

    /// Reads a sheet config; nothing when the config file does not exist yet.
    std::optional<SpriteSheet> loadSpriteSheet(Assets& assets, const std::string& config, Diagnostics& diag);

    /// Which frame of which animation is showing.
    class Animator
    {
    public:
        /// Switches animation; the same one keeps running.
        void play(const std::string& name);
        void update(float dt, const SpriteSheet* sheet);

        [[nodiscard]] const std::string& current() const { return current_; }
        [[nodiscard]] int frame() const { return frame_; }

    private:
        std::string current_ = "idle";
        float time_ = 0.0f;
        int frame_ = 0;
    };

    /// Draws the current frame with its origin at @p feet. Returns false (and
    /// draws nothing) when the sheet has no image or no such animation.
    bool drawSprite(const SpriteSheet& sheet, const Animator& animator, Vector2 feet, bool flipX, Color tint);
} // namespace hollow
