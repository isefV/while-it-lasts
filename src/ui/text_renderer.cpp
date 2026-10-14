#include <hollow/system/assets.hpp>
#include <hollow/ui/text_renderer.hpp>

#include <cmath>
#include <string>

namespace hollow
{
    TextRenderer::~TextRenderer()
    {
        unload();
    }

    void TextRenderer::load(Assets& assets, const UiConfig& ui)
    {
        unload();
        if (assets.exists(ui.font))
        {
            font_ = LoadFontEx(assets.path(ui.font).c_str(), ui.fontSize, nullptr, 0);
            if (IsFontValid(font_))
            {
                SetTextureFilter(font_.texture, TEXTURE_FILTER_POINT);
                owned_ = true;
                size_ = static_cast<float>(ui.fontSize);
                spacing_ = 0.0f;
                return;
            }
        }
        // raylib's own font is a 10 px pixel font; drawing it at its base size
        // keeps every pixel sharp.
        font_ = GetFontDefault();
        owned_ = false;
        size_ = static_cast<float>(font_.baseSize > 0 ? font_.baseSize : 10);
        spacing_ = 1.0f;
    }

    void TextRenderer::unload()
    {
        if (owned_) UnloadFont(font_);
        owned_ = false;
        font_ = Font{};
    }

    void TextRenderer::draw(std::string_view text, Vector2 position, Color color) const
    {
        if (text.empty()) return;
        const std::string s(text);
        DrawTextEx(font_, s.c_str(), Vector2{std::round(position.x), std::round(position.y)}, size_, spacing_, color);
    }

    void TextRenderer::drawShadowed(std::string_view text, Vector2 position, Color color, Color shadow) const
    {
        draw(text, Vector2{position.x, position.y + 1}, ColorAlpha(shadow, static_cast<float>(color.a) / 255.0f));
        draw(text, position, color);
    }

    void TextRenderer::drawScaled(std::string_view text, Vector2 position, Color color, int scale) const
    {
        if (text.empty()) return;
        const std::string s(text);
        const auto f = static_cast<float>(scale);
        DrawTextEx(font_, s.c_str(), Vector2{std::round(position.x), std::round(position.y)}, size_ * f, spacing_ * f,
                   color);
    }

    float TextRenderer::measureScaled(std::string_view text, int scale) const
    {
        if (text.empty()) return 0.0f;
        const std::string s(text);
        const auto f = static_cast<float>(scale);
        return MeasureTextEx(font_, s.c_str(), size_ * f, spacing_ * f).x;
    }

    float TextRenderer::measure(std::string_view text) const
    {
        if (text.empty()) return 0.0f;
        const std::string s(text);
        return MeasureTextEx(font_, s.c_str(), size_, spacing_).x;
    }

    MeasureFn TextRenderer::measureFn() const
    {
        return [this](std::string_view text) { return measure(text); };
    }
} // namespace hollow
