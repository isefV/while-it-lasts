#pragma once

/**
 * @file widgets.hpp
 * @brief Small pieces of interface: panels, bars, the "E" prompt, menus.
 */

#include <hollow/ui/theme.hpp>

#include <raylib.h>

#include <string>
#include <string_view>
#include <vector>

namespace hollow
{
    class Assets;
    class Input;
    class TextRenderer;

    /// A framed box: ui/dialogue_box.png stretched when it exists, else a
    /// filled rectangle with a one-pixel frame.
    void drawPanel(Assets& assets, Rectangle rect, const UiConfig& ui);

    /// One bar: a frame and a fill for @p value of @p max.
    void drawBar(Assets& assets, Rectangle rect, int value, int max, Color color, const UiConfig& ui);

    /// "[E] Magazines" above a point of the scene (canvas pixels), kept inside the scene box;
    /// shown over the thing the player stands next to. @p note is drawn dimmed
    /// after the label ("1/2" when several things are in reach).
    void drawPrompt(const TextRenderer& text, std::string_view label, Vector2 anchor, const UiConfig& ui,
                    std::string_view note = {}, bool showKey = true);

    /// A small triangle pointing right (selected option) or down (more text).
    void drawArrow(Vector2 position, bool down, Color color);

    /// A vertical list of choices (title screen, pause).
    class Menu
    {
    public:
        std::vector<std::string> items;
        int selected = 0;

        /// Up / Down and E or Enter; returns the chosen index, or -1.
        int update(const Input& input);
        void draw(const TextRenderer& text, const UiConfig& ui, Vector2 center) const;

    private:
        [[nodiscard]] Rectangle itemRect(const TextRenderer& text, Vector2 center, int index) const;
    };
} // namespace hollow
