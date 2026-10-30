#pragma once

/**
 * @file character.hpp
 * @brief The people of the game, from "characters" in assets/data/game.json.
 */

#include <raylib.h>

#include <string>

namespace while_it_lasts
{
    struct CharacterDef
    {
        std::string id;          ///< used in dialogue: { "y": "..." }
        std::string name;        ///< shown on screen, and the value of {y} in text
        Color color{150, 160, 170, 255};     ///< body of the placeholder rectangle
        Color textColor{210, 218, 224, 255}; ///< name in the dialogue box, barks
        std::string sprite;      ///< sprite sheet config (sprites/y.json); a rectangle until it exists
        float width = 14.0f;     ///< size of the placeholder, standing
        float height = 40.0f;
        float speed = 48.0f;     ///< walking speed, pixels per second
    };
} // namespace while_it_lasts
