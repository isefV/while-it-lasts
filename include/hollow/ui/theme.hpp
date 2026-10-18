#pragma once

/**
 * @file theme.hpp
 * @brief Screen layout and colours.
 *
 * The text and panels are drawn on one 480 x 270 canvas (visual-asset-config.txt,
 * section 2), enlarged to fill the screen. The scene, 320 x 180 pixels of its
 * own, is enlarged 1.25 times more and shown under the canvas, in SceneBox:
 * on a 1080p screen the canvas is 4x and the scene 5x, both whole pixels.
 */

#include <raylib.h>

#include <string>
#include <vector>

namespace hollow
{
    namespace layout
    {
        inline constexpr int CanvasWidth = 480;
        inline constexpr int CanvasHeight = 270;

        inline constexpr float SceneWidth = 320.0f; ///< the scene's own pixels: rooms, sprites, fights
        inline constexpr float SceneHeight = 180.0f;
        inline constexpr float SceneScale = 1.25f;  ///< canvas pixels per scene pixel

        /// The scene on the canvas: at the top, centred, 1.25 x its size. Only
        /// two thin strips beside it stay dark; the bars sit in the left one.
        inline constexpr Rectangle SceneBox{40, 0, SceneWidth * SceneScale, SceneHeight * SceneScale};
        /// Under the scene, the width of the screen; its top covers the
        /// ground below the ground line (scene y 162 and down).
        inline constexpr Rectangle DialogueBox{8, 202, 464, 64};
        inline constexpr Rectangle BarSlots[3] = {{4, 16, 32, 6}, {4, 36, 32, 6}, {4, 56, 32, 6}}; ///< the name goes above

        inline constexpr float GroundY = 150.0f; ///< scene y where characters stand
        inline constexpr float Padding = 8.0f;   ///< inside the dialogue box
        inline constexpr float LineHeight = 10.0f;
        inline constexpr int MaxOptions = 4;
    } // namespace layout

    /// A font the debug build's Settings can switch to ("fonts" in "ui").
    struct FontChoice
    {
        std::string name;
        std::string file; ///< .ttf relative to assets/; empty = raylib's built-in font
        int size = 10;    ///< the pixel size it was drawn for
    };

    /// Colours and text settings, from "ui" in assets/data/game.json. The
    /// palette is dark and cold, with one warm accent.
    struct UiConfig
    {
        float textSpeed = 45.0f;         ///< letters per second
        std::string font = "fonts/pixeloid_sans.ttf"; ///< .ttf relative to assets/; empty = raylib's built-in font
        int fontSize = 11;
        std::vector<FontChoice> fonts;   ///< to compare fonts in a debug build
        Color text{200, 210, 218, 255};
        Color thought{122, 160, 196, 255};
        Color narration{112, 122, 136, 255};
        Color dim{70, 80, 92, 255};
        Color box{10, 13, 18, 255};
        Color frame{58, 70, 84, 255};
        Color highlight{226, 232, 238, 255};
        Color accent{224, 135, 58, 255}; ///< the one warm colour: fire, ember, sunrise
    };
} // namespace hollow
