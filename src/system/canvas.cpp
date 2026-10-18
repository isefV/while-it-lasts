#include <hollow/system/canvas.hpp>

#include <rlgl.h>

#include <algorithm>
#include <cmath>

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

    Enlarger::~Enlarger()
    {
        unload();
    }

    void Enlarger::unload()
    {
        if (big_.id != 0) UnloadRenderTexture(big_);
        big_ = RenderTexture2D{};
    }

    void Enlarger::draw(const Texture2D& layer, Rectangle destination, bool premultiplied, bool allowSmooth)
    {
        // Render textures are stored upside down: flip with a negative height.
        const auto w = static_cast<float>(layer.width);
        const auto h = static_cast<float>(layer.height);
        const float scale = destination.width / w;
        const BlendMode blend = premultiplied ? BLEND_ALPHA_PREMULTIPLY : BLEND_ALPHA;

        if (!allowSmooth || scale < 1.0f || std::fabs(scale - std::round(scale)) < 0.01f)
        {
            BeginBlendMode(blend);
            DrawTexturePro(layer, Rectangle{0, 0, w, -h}, destination, Vector2{0, 0}, 0.0f, WHITE);
            EndBlendMode();
            return;
        }

        const int k = static_cast<int>(std::ceil(scale));
        const int bigW = layer.width * k;
        const int bigH = layer.height * k;
        if (big_.id == 0 || big_.texture.width != bigW || big_.texture.height != bigH)
        {
            unload();
            big_ = LoadRenderTexture(bigW, bigH);
            SetTextureFilter(big_.texture, TEXTURE_FILTER_BILINEAR);
            SetTextureWrap(big_.texture, TEXTURE_WRAP_CLAMP);
        }

        // An opaque layer is laid over black first, as it would be on screen,
        // so the big copy holds its final colours (multiplied by alpha).
        BeginTextureMode(big_);
        ClearBackground(premultiplied ? BLANK : BLACK);
        BeginBlendMode(blend);
        DrawTexturePro(layer, Rectangle{0, 0, w, -h}, Rectangle{0, 0, static_cast<float>(bigW), static_cast<float>(bigH)},
                       Vector2{0, 0}, 0.0f, WHITE);
        EndBlendMode();
        EndTextureMode();

        BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
        DrawTexturePro(big_.texture, Rectangle{0, 0, static_cast<float>(bigW), -static_cast<float>(bigH)}, destination,
                       Vector2{0, 0}, 0.0f, WHITE);
        EndBlendMode();
    }

    void Canvas::load(int width, int height)
    {
        width_ = width;
        height_ = height;
        layer_.load(width, height);
        layout(GetScreenWidth(), GetScreenHeight());
    }

    void Canvas::unload()
    {
        layer_.unload();
        canvasEnlarger_.unload();
        sceneEnlarger_.unload();
    }

    void Canvas::begin() const
    {
        layer_.begin();
        // Colours multiplied by alpha, alpha adding up: what is drawn over the
        // see-through canvas (a fade, a dimmed scene) covers the scene under it
        // as much as it would have covered it drawn straight on top.
        rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD,
                                  RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    }

    void Canvas::end() const
    {
        EndBlendMode();
        layer_.end();
    }

    void Canvas::layout(int screenW, int screenH)
    {
        const float fit = std::min(static_cast<float>(screenW) / static_cast<float>(width_),
                                   static_cast<float>(screenH) / static_cast<float>(height_));
        const float whole = std::floor(fit);
        const float covered = whole * static_cast<float>(width_) * whole * static_cast<float>(height_) /
                              (static_cast<float>(screenW) * static_cast<float>(screenH));
        scale_ = whole >= 1.0f && covered >= 0.9f ? whole : fit;

        const float w = std::round(static_cast<float>(width_) * scale_);
        const float h = std::round(static_cast<float>(height_) * scale_);
        destination_ = Rectangle{std::round((static_cast<float>(screenW) - w) / 2.0f),
                                 std::round((static_cast<float>(screenH) - h) / 2.0f), w, h};
    }

    void Canvas::drawLayers(const RenderLayer* scene, Rectangle sceneBox, bool allowSmooth)
    {
        if (scene && scene->loaded())
        {
            const Rectangle at{std::round(destination_.x + sceneBox.x * scale_), std::round(destination_.y + sceneBox.y * scale_),
                               std::round(sceneBox.width * scale_), std::round(sceneBox.height * scale_)};
            sceneEnlarger_.draw(scene->texture(), at, false, allowSmooth);
        }
        canvasEnlarger_.draw(layer_.texture(), destination_, true, allowSmooth);
    }

    void Canvas::present(const RenderLayer* scene, Rectangle sceneBox)
    {
        layout(GetScreenWidth(), GetScreenHeight()); // the window may have changed size (fullscreen toggle)
        ClearBackground(BLACK);
        drawLayers(scene, sceneBox, true);
    }

    void Canvas::compose(int width, int height, const RenderLayer* scene, Rectangle sceneBox)
    {
        layout(width, height);
        ClearBackground(BLACK);
        drawLayers(scene, sceneBox, false);
        layout(GetScreenWidth(), GetScreenHeight());
    }

    Vector2 Canvas::toCanvas(Vector2 screen) const
    {
        return Vector2{(screen.x - destination_.x) / scale_, (screen.y - destination_.y) / scale_};
    }
} // namespace hollow
