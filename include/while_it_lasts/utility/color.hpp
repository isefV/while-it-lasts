#pragma once

/**
 * @file color.hpp
 * @brief Colours written in the data files as "#rrggbb" or "#rrggbbaa".
 */

#include <raylib.h>

#include <optional>
#include <string_view>

namespace while_it_lasts
{
    /// The colour of "#rrggbb" or "#rrggbbaa"; nothing if @p text is neither.
    std::optional<Color> parseColor(std::string_view text);

    /// Multiplies two colours channel by channel (a tint).
    Color multiply(Color a, Color b);

    /// @p a to @p b by @p t, all four channels.
    Color mix(Color a, Color b, float t);
} // namespace while_it_lasts
