#pragma once

/**
 * @file dialogue_runner.hpp
 * @brief Plays a conversation one entry at a time.
 *
 * The runner decides what is on screen (view()) and waits: for advance()
 * after a line, for choose() at a choice, for time at a pause or a timed
 * choice. Effects change the StoryState; commands ("sound crack") go to the
 * game through the CommandHandler.
 */

#include <while_it_lasts/system/dialogue.hpp>
#include <while_it_lasts/utility/text.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace while_it_lasts
{
    class StoryState;

    struct DialogueView
    {
        enum class Kind
        {
            None,
            Line,
            Choice,
            Pause,
        };

        Kind kind = Kind::None;
        LineStyle style = LineStyle::Speech;
        std::string speaker;              ///< character id, for Speech
        std::string text;                 ///< tokens such as {x} already replaced
        std::vector<std::string> options; ///< the options shown, tokens replaced
        float timer = 0.0f;               ///< the choice's limit; 0 = none
        float timeLeft = 0.0f;
        unsigned serial = 0;              ///< changes whenever a new entry is shown
    };

    class DialogueRunner
    {
    public:
        using CommandHandler = std::function<void(const Effect&)>;

        DialogueRunner(const ConversationMap& conversations, StoryState& state, CommandHandler onCommand);
        ~DialogueRunner();

        DialogueRunner(const DialogueRunner&) = delete;
        DialogueRunner& operator=(const DialogueRunner&) = delete;

        /// Replacements for {tokens} in the text ({x}, {y}: the names).
        void setTokens(TokenMap tokens);

        /// Starts a conversation, replacing any that is running. Returns false
        /// for an unknown id or one with nothing to show.
        bool start(const std::string& id);

        /// Ends the conversation at once.
        void stop();

        /// Runs pauses and choice timers.
        void update(float dt);

        /// Goes past the line on screen.
        void advance();

        /// Picks an option: @p index counts the options shown (view().options).
        void choose(int index);

        [[nodiscard]] bool active() const;
        [[nodiscard]] const DialogueView& view() const;

        /// The conversation that was started last (empty when none ran yet).
        [[nodiscard]] const std::string& currentId() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> d_;
    };
} // namespace while_it_lasts
