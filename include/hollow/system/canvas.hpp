#pragma once

/**
 * @file canvas.hpp
 * @brief Off-screen drawing targets, and the 480 x 270 canvas scaled up by
 * a whole number to fill the screen.
 */

#include <raylib.h>

namespace hollow
{
    /// An off-screen picture to draw into (the canvas, the scene box).
    class RenderLayer
    {
    public:
        RenderLayer() = default;
        ~RenderLayer();

        RenderLayer(const RenderLayer&) = delete;
        RenderLayer& operator=(const RenderLayer&) = delete;

        /// Creates the GPU texture; call after the window exists.
        void load(int width, int height);
        void unload();

        void begin() const;
        void end() const;

        /// Draws the layer with its top-left corner at @p position.
        void draw(Vector2 position, Color tint = WHITE) const;

        [[nodiscard]] const Texture2D& texture() const { return target_.texture; }
        [[nodiscard]] bool loaded() const { return target_.id != 0; }

    private:
        RenderTexture2D target_{};
    };

    class Canvas
    {
    public:
        void load(int width, int height);
        void unload();

        /// Everything drawn between begin() and end() lands on the canvas.
        void begin() const { layer_.begin(); }
        void end() const { layer_.end(); }

        /// Draws the canvas on the screen, enlarged by the biggest whole number
        /// that fits and centred, black around it. Call inside BeginDrawing().
        void present();

        /// Screen position (mouse) to canvas position.
        [[nodiscard]] Vector2 toCanvas(Vector2 screen) const;

        [[nodiscard]] const Texture2D& texture() const { return layer_.texture(); }
        [[nodiscard]] int scale() const { return scale_; }
        [[nodiscard]] int width() const { return width_; }
        [[nodiscard]] int height() const { return height_; }

    private:
        void layout();

        RenderLayer layer_;
        int width_ = 0;
        int height_ = 0;
        int scale_ = 1;
        Rectangle destination_{};
    };
} // namespace hollow
