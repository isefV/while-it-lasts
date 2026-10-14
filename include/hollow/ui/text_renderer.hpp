#pragma once

/**
 * @file text_renderer.hpp
 * @brief The game's one pixel font: drawing and measuring text.
 */

#include <hollow/ui/theme.hpp>
#include <hollow/utility/text.hpp>

#include <raylib.h>

#include <string_view>

namespace hollow
{
    class Assets;

    class TextRenderer
    {
    public:
        TextRenderer() = default;
        ~TextRenderer();

        TextRenderer(const TextRenderer&) = delete;
        TextRenderer& operator=(const TextRenderer&) = delete;

        /// The font named in @p ui (assets/fonts/...), or raylib's built-in
        /// pixel font while there is none. Needs the window.
        void load(Assets& assets, const UiConfig& ui);
        void unload();

        void draw(std::string_view text, Vector2 position, Color color) const;

        /// With a dark copy one pixel lower, for text over the scene.
        void drawShadowed(std::string_view text, Vector2 position, Color color, Color shadow = BLACK) const;

        /// @p scale times larger, still whole pixels (the title).
        void drawScaled(std::string_view text, Vector2 position, Color color, int scale) const;

        [[nodiscard]] float measure(std::string_view text) const;
        [[nodiscard]] float measureScaled(std::string_view text, int scale) const;
        [[nodiscard]] MeasureFn measureFn() const;
        [[nodiscard]] float size() const { return size_; }

    private:
        Font font_{};
        bool owned_ = false;
        float size_ = 10.0f;
        float spacing_ = 1.0f;
    };
} // namespace hollow
