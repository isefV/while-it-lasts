#pragma once

/**
 * @file bark.hpp
 * @brief Short lines over a character's head while the game goes on.
 */

#include <while_it_lasts/data/game_data.hpp>

#include <raylib.h>

#include <vector>

namespace while_it_lasts
{
    class TextRenderer;

    class BarkPlayer
    {
    public:
        /// Plays one line or a short exchange, replacing what is playing.
        void play(std::vector<BarkLine> lines);
        void stop();
        void update(float dt);

        [[nodiscard]] bool active() const { return index_ < lines_.size(); }

        /// The line showing now, or nullptr.
        [[nodiscard]] const BarkLine* current() const;

        /// Draws the current line centred above @p head (canvas pixels), inside the scene box.
        void draw(const TextRenderer& text, Vector2 head, Color color) const;

    private:
        std::vector<BarkLine> lines_;
        std::size_t index_ = 0;
        float time_ = 0.0f;
    };
} // namespace while_it_lasts
