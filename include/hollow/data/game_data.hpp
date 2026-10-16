#pragma once

/**
 * @file game_data.hpp
 * @brief Everything the game reads from assets/data/, loaded and checked.
 *
 * assets/data/game.json names the other files. loadGameData() reads them
 * all; checkGameData() then looks for mistakes that span files: a "next"
 * naming a conversation that does not exist, a condition reading a
 * variable nobody declared, an exit to an unknown room. `game --check`
 * runs both and prints every problem with its file and place.
 */

#include <hollow/objects/character.hpp>
#include <hollow/objects/item.hpp>
#include <hollow/objects/room.hpp>
#include <hollow/system/dialogue.hpp>
#include <hollow/ui/theme.hpp>
#include <hollow/utility/diagnostics.hpp>
#include <hollow/utility/text.hpp>

#include <raylib.h>

#include <map>
#include <set>
#include <string>
#include <vector>

namespace hollow
{
    struct VariableDef
    {
        std::string name;
        int value = 0;
        int min = 0;
        int max = 1;
    };

    struct BarDef
    {
        std::string label;
        std::string var;
        int max = 1;
        Color color{150, 170, 190, 255};
    };

    struct BarkLine
    {
        std::string speaker; ///< character id
        std::string text;
    };

    /// Short lines over a character's head; the game keeps playing.
    struct BarkPool
    {
        std::string id;
        std::string where;
        Condition condition;
        std::vector<std::vector<BarkLine>> entries; ///< each entry: one line or a short exchange
    };

    struct GameData
    {
        std::string assetsDir; ///< ends with '/'

        std::string title;
        std::string startRoom;
        std::vector<std::string> startItems;
        std::string player = "y";
        std::string companion = "x";

        std::vector<VariableDef> variables;
        std::map<std::string, CharacterDef, std::less<>> characters;
        std::map<std::string, ItemDef, std::less<>> items;
        std::vector<std::string> itemOrder; ///< as written in items.json
        std::map<std::string, RoomDef, std::less<>> rooms;
        ConversationMap conversations;
        std::map<std::string, BarkPool, std::less<>> barks;

        UiConfig ui;
        std::vector<BarDef> bars;            ///< top left; none in the prototype
        std::vector<std::string> endCard;    ///< lines of the end card

        /// {y} and {x} -> the characters' names.
        [[nodiscard]] TokenMap tokens() const;
    };

    /// Reads assets/data/game.json and the files it names. Problems go to
    /// @p diag; whatever could be read is kept.
    GameData loadGameData(const std::string& assetsDir, Diagnostics& diag);

    /**
     * @brief Checks references between the files.
     * @param commands the command verbs the game understands ("goto", "sound", ...)
     */
    void checkGameData(const GameData& data, const std::set<std::string, std::less<>>& commands, Diagnostics& diag);

    /// Reads one dialogue file's text into @p out (used by loadGameData and the tests).
    void parseDialogueText(std::string_view text, const std::string& file, ConversationMap& out, Diagnostics& diag);
} // namespace hollow
