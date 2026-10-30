#pragma once

/**
 * @file script.hpp
 * @brief The two small languages of the data files: conditions ("if") and
 * effects ("do").
 *
 * Conditions:
 *
 *     "has_antibiotics"                    true when not 0
 *     "!noisy_entry"                       not
 *     "cig_count >= 1"                     == != < <= > >=
 *     "x_condition <= 1 && item.bandage"   && (and), || (or), parentheses
 *     "seen.campsite.fire > 0"
 *
 * Effects, one per string:
 *
 *     "cig_count += 1"    "x_condition -= 1"    "noisy_entry = true"
 *     "give bandage"      "take bandage"
 *     anything else is a command for the game: its first word is the verb,
 *     the rest are its arguments ("sound crack", "goto factory_entrance").
 */

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace while_it_lasts
{
    class StoryState;

    class Condition
    {
    public:
        /// An empty condition, which is always true.
        Condition() = default;

        /// Parses @p text; on a mistake returns nothing and explains it in @p error.
        static std::optional<Condition> parse(std::string_view text, std::string& error);

        [[nodiscard]] bool evaluate(const StoryState& state) const;

        /// The variable names the condition reads.
        [[nodiscard]] std::vector<std::string> names() const;

        [[nodiscard]] bool empty() const noexcept { return !root_; }
        [[nodiscard]] const std::string& source() const noexcept { return source_; }

        struct Node;

    private:
        std::shared_ptr<const Node> root_;
        std::string source_;
    };

    struct Effect
    {
        enum class Kind
        {
            Set,     ///< name = value
            Add,     ///< name += value (or -=, stored negative)
            Give,    ///< give item
            Take,    ///< take item
            Command, ///< verb and arguments, handled by the game
        };

        Kind kind = Kind::Command;
        std::string name;              ///< variable or item; the verb for a command
        int value = 0;
        std::vector<std::string> args; ///< a command's arguments, after the verb
        std::string source;            ///< the text as written

        /// Parses @p text; on a mistake returns nothing and explains it in @p error.
        static std::optional<Effect> parse(std::string_view text, std::string& error);
    };

    /// Applies @p effects in order: variables and items change @p state;
    /// commands go to @p onCommand.
    template <typename CommandFn>
    void applyEffects(const std::vector<Effect>& effects, StoryState& state, CommandFn&& onCommand);

    /// Applies one variable or item effect; returns false for a command.
    bool applyStateEffect(const Effect& effect, StoryState& state);
} // namespace while_it_lasts

namespace while_it_lasts
{
    template <typename CommandFn>
    void applyEffects(const std::vector<Effect>& effects, StoryState& state, CommandFn&& onCommand)
    {
        for (const Effect& effect : effects)
        {
            if (!applyStateEffect(effect, state)) onCommand(effect);
        }
    }
} // namespace while_it_lasts
