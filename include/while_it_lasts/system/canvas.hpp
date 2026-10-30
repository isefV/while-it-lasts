#pragma once

/**
 * @file canvas.hpp
 * @brief Off-screen drawing targets, and the 480 x 270 canvas enlarged to
 * fill the screen, with the scene shown under it.
 */

#include <raylib.h>

namespace while_it_lasts
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

    /// Draws a pixel-art layer enlarged. By a whole number: as it is. By a
    /// fraction: first by the next whole number, then smoothly down to the
    /// size, so every pixel stays the same size (no uneven rows).
    class Enlarger
    {
    public:
        Enlarger() = default;
        ~Enlarger();

        Enlarger(const Enlarger&) = delete;
        Enlarger& operator=(const Enlarger&) = delete;

        /// @p premultiplied: the layer's colours are already multiplied by
        /// their alpha (the see-through canvas); otherwise it is shown over
        /// black, as an opaque picture.
        void draw(const Texture2D& layer, Rectangle destination, bool premultiplied, bool allowSmooth = true);
        void unload();

    private:
        RenderTexture2D big_{};
    };

    class Canvas
    {
    public:
        void load(int width, int height);
        void unload();

        /// Everything drawn between begin() and end() lands on the canvas.
        /// Clear it with BLANK to see the scene through it, BLACK to hide it.
        void begin() const;
        void end() const;

        /// Draws on the screen, centred, black around it: @p scene (when not
        /// null) in @p sceneBox (canvas pixels), then the canvas over it. The
        /// scale is the biggest whole number that fits when that leaves 10 %
        /// of the screen black or less; otherwise the canvas fills the screen
        /// as far as its shape allows. Call inside BeginDrawing().
        void present(const RenderLayer* scene = nullptr, Rectangle sceneBox = {});

        /// The same picture on the current render target, @p width x @p height
        /// (a screenshot; enlarged as it is, never smoothed).
        void compose(int width, int height, const RenderLayer* scene, Rectangle sceneBox);

        /// Screen position (mouse) to canvas position.
        [[nodiscard]] Vector2 toCanvas(Vector2 screen) const;

        [[nodiscard]] const Texture2D& texture() const { return layer_.texture(); }
        [[nodiscard]] float scale() const { return scale_; }
        [[nodiscard]] int width() const { return width_; }
        [[nodiscard]] int height() const { return height_; }

    private:
        void layout(int screenW, int screenH);
        void drawLayers(const RenderLayer* scene, Rectangle sceneBox, bool allowSmooth);

        RenderLayer layer_;
        Enlarger canvasEnlarger_;
        Enlarger sceneEnlarger_;
        int width_ = 0;
        int height_ = 0;
        float scale_ = 1.0f;
        Rectangle destination_{};
    };
} // namespace while_it_lasts
