#pragma once

/**
 * @file dialogue_box.hpp
 * @brief The box under the scene: lines typed out letter by letter, and
 * choices.
 *
 * Speech starts with the speaker's name in their colour. Thoughts have their
 * own colour and no name; narration is grey. A line too long for the box is
 * split into pages. The first press shows the whole page at once, the next
 * one moves on.
 */

#include <hollow/system/dialogue_runner.hpp>
#include <hollow/ui/theme.hpp>

#include <raylib.h>

#include <string>
#include <vector>

namespace hollow
{
    class Assets;
    class Input;
    class TextRenderer;

    class DialogueBox
    {
    public:
        struct Speaker
        {
            std::string name;
            Color nameColor{};
        };

        struct Result
        {
            bool advance = false;     ///< the line was read: call DialogueRunner::advance()
            int choice = -1;          ///< an option was picked: call DialogueRunner::choose()
            bool choiceShown = false; ///< a choice just appeared (the game plays its sound)
        };

        /// How long a new choice ignores the keys, so a player pressing
        /// through the lines before it sees it instead of picking by accident.
        static constexpr float ChoiceLock = 0.8f;

        /// Follows the runner's view and reads the input.
        Result update(const DialogueView& view, const Speaker& speaker, const Input& input, float dt,
                      const TextRenderer& text, const UiConfig& ui);

        /// A choice is on screen but cannot be picked yet (its timer waits too).
        [[nodiscard]] bool choiceLocked() const { return choice_ && choiceAge_ < ChoiceLock; }

        void draw(const DialogueView& view, const TextRenderer& text, const UiConfig& ui, Assets& assets) const;

    private:
        void layoutLine(const DialogueView& view, const Speaker& speaker, const TextRenderer& text);
        [[nodiscard]] Rectangle optionRect(int index) const;
        [[nodiscard]] std::size_t pageLength() const;
        [[nodiscard]] int linesPerPage() const;

        unsigned serial_ = ~0u;
        std::vector<std::string> lines_;
        std::size_t page_ = 0;
        float revealed_ = 0.0f;
        float blink_ = 0.0f;
        int selected_ = 0;
        bool choice_ = false;    ///< the entry on screen is a choice
        float choiceAge_ = 0.0f; ///< seconds it has been on screen
        std::string prefix_;
        Color prefixColor_{};
        Color textColor_{};
    };
} // namespace hollow
