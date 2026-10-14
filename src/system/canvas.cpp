#include <hollow/system/canvas.hpp>

#include <algorithm>

namespace hollow
{
    RenderLayer::~RenderLayer()
    {
        unload();
    }

    void RenderLayer::load(int width, int height)
    {
        unload();
        target_ = LoadRenderTexture(width, height);
        SetTextureFilter(target_.texture, TEXTURE_FILTER_POINT);
    }

    void RenderLayer::unload()
    {
        if (target_.id != 0) UnloadRenderTexture(target_);
        target_ = RenderTexture2D{};
    }

    void RenderLayer::begin() const
    {
        BeginTextureMode(target_);
    }

    void RenderLayer::end() const
    {
        EndTextureMode();
    }

    void RenderLayer::draw(Vector2 position, Color tint) const
    {
        // Render textures are stored upside down: flip with a negative height.
        const auto w = static_cast<float>(target_.texture.width);
        const auto h = static_cast<float>(target_.texture.height);
        DrawTextureRec(target_.texture, Rectangle{0, 0, w, -h}, position, tint);
    }

    void Canvas::load(int width, int height)
    {
        width_ = width;
        height_ = height;
        layer_.load(width, height);
        layout();
    }

    void Canvas::unload()
    {
        layer_.unload();
    }

    void Canvas::layout()
    {
        const int screenW = GetScreenWidth();
        const int screenH = GetScreenHeight();
        scale_ = std::max(1, std::min(screenW / width_, screenH / height_));

        const auto w = static_cast<float>(width_ * scale_);
        const auto h = static_cast<float>(height_ * scale_);
        destination_ = Rectangle{static_cast<float>((screenW - static_cast<int>(w)) / 2),
                                 static_cast<float>((screenH - static_cast<int>(h)) / 2), w, h};
    }

    void Canvas::present()
    {
        layout(); // the window may have changed size (fullscreen toggle)
        ClearBackground(BLACK);
        const auto w = static_cast<float>(width_);
        const auto h = static_cast<float>(height_);
        DrawTexturePro(layer_.texture(), Rectangle{0, 0, w, -h}, destination_, Vector2{0, 0}, 0.0f, WHITE);
    }

    Vector2 Canvas::toCanvas(Vector2 screen) const
    {
        const auto s = static_cast<float>(scale_);
        return Vector2{(screen.x - destination_.x) / s, (screen.y - destination_.y) / s};
    }
} // namespace hollow
