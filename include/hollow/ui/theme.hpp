#pragma once

/**
 * @file theme.hpp
 * @brief Screen layout and colours.
 *
 * Everything is drawn on one 480 x 270 canvas (visual-asset-config.txt,
 * section 2), which is then enlarged by a whole number to fill the screen.
 */

#include <raylib.h>

#include <string>

namespace hollow
{
    namespace layout
    {
        inline constexpr int CanvasWidth = 480;
        inline constexpr int CanvasHeight = 270;

        inline constexpr Rectangle SceneBox{80, 16, 320, 180};
        inline constexpr Rectangle DialogueBox{80, 202, 320, 64};
        inline constexpr Rectangle BarSlots[3] = {{8, 16, 64, 8}, {8, 28, 64, 8}, {8, 40, 64, 8}};

        inline constexpr float GroundY = 150.0f; ///< scene y where characters stand
        inline constexpr float Padding = 8.0f;   ///< inside the dialogue box
        inline constexpr float LineHeight = 10.0f;
        inline constexpr int MaxOptions = 4;
    } // namespace layout

    /// Colours and text settings, from "ui" in assets/data/game.json. The
    /// palette is dark and cold, with one warm accent.
    struct UiConfig
    {
        float textSpeed = 45.0f;         ///< letters per second
        std::string font;                ///< .ttf relative to assets/; empty = raylib's built-in font
        int fontSize = 10;
        Color text{200, 210, 218, 255};
        Color thought{122, 160, 196, 255};
        Color narration{112, 122, 136, 255};
        Color dim{70, 80, 92, 255};
        Color box{10, 13, 18, 255};
        Color frame{58, 70, 84, 255};
        Color highlight{226, 232, 238, 255};
        Color accent{224, 135, 58, 255}; ///< the one warm colour: fire, ember, sunset
    };
} // namespace hollow
