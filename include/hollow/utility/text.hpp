#pragma once

/**
 * @file text.hpp
 * @brief String helpers: name tokens, word wrapping, UTF-8 counting.
 */

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace hollow
{
    /// Map of token -> replacement, for replaceTokens().
    using TokenMap = std::map<std::string, std::string, std::less<>>;

    /// Replaces every "{key}" in @p text with its value from @p tokens. Unknown
    /// keys are left as they are, braces included.
    std::string replaceTokens(std::string_view text, const TokenMap& tokens);

    /// Measures the width of a piece of text, in pixels.
    using MeasureFn = std::function<float(std::string_view)>;

    /**
     * @brief Splits @p text into lines no wider than @p maxWidth.
     *
     * Breaks at spaces and at '\n'. A word wider than a whole line is cut
     * between characters. The first line is @p firstIndent pixels narrower
     * (room for the speaker's name in front of it).
     */
    std::vector<std::string> wrapText(std::string_view text, float maxWidth, const MeasureFn& measure,
                                      float firstIndent = 0.0f);

    /// Number of characters (code points) in UTF-8 text.
    std::size_t utf8Count(std::string_view text);

    /// The first @p count characters of UTF-8 text.
    std::string_view utf8Prefix(std::string_view text, std::size_t count);

    /// @p text without spaces, tabs and line breaks at both ends.
    std::string_view trim(std::string_view text);

    /// @p text cut at runs of spaces and tabs.
    std::vector<std::string> splitWords(std::string_view text);
} // namespace hollow
