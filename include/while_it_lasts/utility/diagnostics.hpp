#pragma once

/**
 * @file diagnostics.hpp
 * @brief Errors and warnings collected while loading and checking data.
 *
 * Each message starts with where the problem is: a file and line for JSON
 * syntax errors, a file and a path inside it for everything else
 * ("dialogue/stage00.json: campsite.x[4].choice[0].next: ...").
 */

#include <string>
#include <utility>
#include <vector>

namespace while_it_lasts
{
    struct Diagnostics
    {
        std::vector<std::string> errors;
        std::vector<std::string> warnings;

        void error(std::string message) { errors.push_back(std::move(message)); }
        void warning(std::string message) { warnings.push_back(std::move(message)); }

        [[nodiscard]] bool ok() const noexcept { return errors.empty(); }
    };
} // namespace while_it_lasts
