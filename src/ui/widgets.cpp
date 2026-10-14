#include <hollow/system/assets.hpp>
#include <hollow/system/input.hpp>
#include <hollow/ui/text_renderer.hpp>
#include <hollow/ui/widgets.hpp>

#include <algorithm>
#include <cmath>

namespace hollow
{
    void drawPanel(Assets& assets, Rectangle rect, const UiConfig& ui)
    {
        if (const Texture2D* frame = assets.texture("ui/dialogue_box.png"))
        {
            const NPatchInfo info{Rectangle{0, 0, static_cast<float>(frame->width), static_cast<float>(frame->height)},
                                  8, 8, 8, 8, NPATCH_NINE_PATCH};
            DrawTextureNPatch(*frame, info, rect, Vector2{0, 0}, 0.0f, WHITE);
            return;
        }
        DrawRectangleRec(rect, ui.box);
        DrawRectangleLinesEx(rect, 1.0f, ui.frame);
        // A darker inner line gives the frame some depth.
        DrawRectangleLinesEx(Rectangle{rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2}, 1.0f,
                             ColorAlpha(BLACK, 0.6f));
    }

    void drawBar(Assets& assets, Rectangle rect, int value, int max, Color color, const UiConfig& ui)
    {
        const float fraction = max > 0 ? std::clamp(static_cast<float>(value) / static_cast<float>(max), 0.0f, 1.0f) : 0.0f;
        const Rectangle inner{rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2};
        const Rectangle fill{inner.x, inner.y, std::round(inner.width * fraction), inner.height};

        if (const Texture2D* fillTexture = assets.texture("ui/bar_fill.png"))
            DrawTexturePro(*fillTexture, Rectangle{0, 0, fill.width, static_cast<float>(fillTexture->height)}, fill,
                           Vector2{0, 0}, 0.0f, color);
        else
            DrawRectangleRec(fill, color);

        if (const Texture2D* frame = assets.texture("ui/bar_frame.png"))
            DrawTexture(*frame, static_cast<int>(rect.x), static_cast<int>(rect.y), WHITE);
        else
            DrawRectangleLinesEx(rect, 1.0f, ui.frame);
    }

    void drawArrow(Vector2 p, bool down, Color color)
    {
        if (down)
            DrawTriangle(Vector2{p.x, p.y}, Vector2{p.x + 2.5f, p.y + 3}, Vector2{p.x + 5, p.y}, color);
        else
            DrawTriangle(Vector2{p.x, p.y}, Vector2{p.x, p.y + 6}, Vector2{p.x + 4, p.y + 3}, color);
    }

    void drawPrompt(const TextRenderer& text, std::string_view label, Vector2 anchor, bool reachable,
                    const UiConfig& ui, bool showKey)
    {
        const float keyW = showKey ? 10.0f : 0.0f;
        const float labelW = text.measure(label);
        const float w = std::round(keyW + labelW + 6.0f);
        const float h = 12.0f;
        const float sceneW = layout::SceneBox.width;

        float x = std::round(anchor.x - w / 2.0f);
        x = std::clamp(x, 1.0f, sceneW - w - 1.0f);
        const float y = std::max(1.0f, std::round(anchor.y - h - 3.0f));

        const Color textColor = reachable ? ui.highlight : ui.dim;
        DrawRectangleRec(Rectangle{x, y, w, h}, ColorAlpha(ui.box, 0.85f));
        DrawRectangleLinesEx(Rectangle{x, y, w, h}, 1.0f, reachable ? ui.frame : ColorAlpha(ui.frame, 0.5f));
        if (showKey)
        {
            DrawRectangleRec(Rectangle{x + 2, y + 2, 8, 8}, reachable ? ui.highlight : ui.dim);
            text.draw("E", Vector2{x + 3, y + 1}, ui.box);
        }
        text.draw(label, Vector2{x + 3 + keyW, y + 1}, textColor);
    }

    Rectangle Menu::itemRect(const TextRenderer& text, Vector2 center, int index) const
    {
        const float w = text.measure(items[static_cast<std::size_t>(index)]) + 16.0f;
        const float y = center.y + static_cast<float>(index) * 14.0f - static_cast<float>(items.size()) * 7.0f;
        return Rectangle{std::round(center.x - w / 2.0f), std::round(y), std::round(w), 12.0f};
    }

    int Menu::update(const Input& input, const TextRenderer& text, Vector2 center)
    {
        const int count = static_cast<int>(items.size());
        if (count == 0) return -1;
        if (input.pressed(Action::Up)) selected = (selected + count - 1) % count;
        if (input.pressed(Action::Down)) selected = (selected + 1) % count;

        for (int i = 0; i < count; ++i)
        {
            if (CheckCollisionPointRec(input.mouse(), itemRect(text, center, i)))
            {
                if (input.mouseMoved()) selected = i;
                if (input.clicked()) return i;
            }
        }
        if (input.pressed(Action::Confirm)) return selected;
        return -1;
    }

    void Menu::draw(const TextRenderer& text, const UiConfig& ui, Vector2 center) const
    {
        for (int i = 0; i < static_cast<int>(items.size()); ++i)
        {
            const Rectangle r = itemRect(text, center, i);
            const bool active = i == selected;
            if (active) drawArrow(Vector2{r.x, r.y + 2}, false, ui.highlight);
            text.draw(items[static_cast<std::size_t>(i)], Vector2{r.x + 10, r.y + 1}, active ? ui.highlight : ui.dim);
        }
    }
} // namespace hollow
