#pragma once

/**
 * @file dialogue.hpp
 * @brief Conversations as they are written in the files in assets/data/dialogue/.
 *
 * A dialogue file maps conversation ids to lists of entries:
 *
 *     {
 *       "campsite.cigarettes": [
 *         { "y": "One left. Of course there's one left. Just one." },
 *         { "choice": [
 *             { "text": "Take it.",  "do": ["cig_count += 1"], "next": "campsite.cig_taken" },
 *             { "text": "Leave it." }
 *         ] }
 *       ]
 *     }
 *
 * The entries are described in docs/DATA-FORMAT.md.
 */

#include <hollow/system/script.hpp>

#include <map>
#include <string>
#include <vector>

namespace hollow
{
    enum class LineStyle
    {
        Speech,    ///< a character speaks ("y": "...")
        Thought,   ///< the player's inner voice, not spoken ("thought": "...")
        Narration, ///< what happens, told in a few words ("narration": "...")
    };

    struct DialogueOption
    {
        std::string text;            ///< empty: only taken when the timer runs out
        Condition condition;         ///< "if": shown only while true
        std::vector<Effect> effects; ///< "do"
        std::string next;            ///< "next": conversation to continue with; empty: go on below the choice
        bool timeout = false;        ///< "timeout": taken when the choice's timer runs out
    };

    struct DialogueEntry
    {
        enum class Kind
        {
            Line,   ///< a speaker, a thought or narration
            Choice, ///< up to four options
            Goto,   ///< continue in another conversation
            Pause,  ///< nothing on screen for a moment
            Block,  ///< "if" with "then" and "else" lists
            Action, ///< only "do"
            End,    ///< the conversation stops here
        };

        Kind kind = Kind::Action;
        std::string where; ///< file and path, for messages

        Condition condition;         ///< "if": the entry is skipped while false
        std::vector<Effect> effects; ///< "do": applied when the entry is reached

        // Line
        LineStyle style = LineStyle::Speech;
        std::string speaker; ///< character id for Speech
        std::string text;

        // Choice
        std::vector<DialogueOption> options;
        float timer = 0.0f; ///< seconds to choose; 0 = no limit

        // Goto
        std::string target;

        // Pause
        float seconds = 0.0f;

        // Block
        std::vector<DialogueEntry> thenEntries;
        std::vector<DialogueEntry> elseEntries;
    };

    struct Conversation
    {
        std::string id;
        std::string file; ///< relative to assets/
        std::vector<DialogueEntry> entries;
    };

    using ConversationMap = std::map<std::string, Conversation, std::less<>>;
} // namespace hollow
