#pragma once

/**
 * @file story_state.hpp
 * @brief The story variables, the inventory and what has been seen.
 *
 * Every value is a whole number; true/false are 1/0. Names a condition can
 * read:
 *
 *   x_condition          a variable declared in game.json (clamped to its range)
 *   item.bandage         1 while the bandage is in the inventory, else 0
 *   seen.campsite.fire   how many times that conversation has started
 */

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace while_it_lasts
{
    class StoryState
    {
    public:
        StoryState();
        ~StoryState();

        StoryState(const StoryState&) = delete;
        StoryState& operator=(const StoryState&) = delete;
        StoryState(StoryState&&) noexcept;
        StoryState& operator=(StoryState&&) noexcept;

        /// Declares a variable with its start value and range.
        void define(const std::string& name, int value, int min, int max);

        /// true for a declared variable.
        [[nodiscard]] bool defined(std::string_view name) const;

        /// The value of a variable, item.<id> or seen.<conversation>; 0 for
        /// anything unknown.
        [[nodiscard]] int get(std::string_view name) const;

        /// Sets a declared variable, clamped to its range. Unknown names are
        /// ignored (the data check reports them before the game runs).
        void set(std::string_view name, int value);
        void add(std::string_view name, int delta);

        [[nodiscard]] bool hasItem(std::string_view id) const;
        void give(const std::string& id);
        void take(std::string_view id);

        /// Items held, in the order they were picked up.
        [[nodiscard]] const std::vector<std::string>& items() const;

        void markSeen(const std::string& conversation);
        [[nodiscard]] int seen(std::string_view conversation) const;

        /// Every variable back to its start value; no items; nothing seen.
        void reset();

        /// A copy of everything (a checkpoint), and putting one back. The
        /// state itself stays the same object (the dialogue runner holds it).
        [[nodiscard]] StoryState snapshot() const;
        void restore(const StoryState& from);

    private:
        struct Impl;
        std::unique_ptr<Impl> d_;
    };
} // namespace while_it_lasts
