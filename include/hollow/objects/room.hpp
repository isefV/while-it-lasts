#pragma once

/**
 * @file room.hpp
 * @brief Rooms and the things in them, from the files in assets/data/rooms/.
 *
 * Positions are in scene pixels: the scene box is 320 x 180 and characters
 * stand on the ground line at y = 150.
 */

#include <hollow/system/script.hpp>

#include <raylib.h>

#include <string>
#include <vector>

namespace hollow
{
    /// Something in a room: a prop, an exit, a hiding spot, an invisible zone.
    struct ObjectDef
    {
        std::string id;
        std::string name;            ///< shown when the player can use it
        std::string where;           ///< file and path, for messages
        Rectangle rect{};
        Color color{90, 100, 110, 255};
        std::string sprite;          ///< relative to assets/; the hover image is <name>_hover.png
        bool front = false;          ///< drawn in front of the characters
        bool hidden = false;         ///< nothing is drawn (zones, triggers)
        Condition condition;         ///< "if": in the room only while true

        // Interaction (E, or a click while standing close)
        std::string dialogue;        ///< conversation played on use
        std::string again;           ///< conversation for the second and later uses
        bool interactive = true;     ///< false: decoration only
        bool once = false;           ///< can be used one time only

        // Exit
        std::string exitTo;          ///< room id
        float spawnX = -1.0f;        ///< where the player appears there; -1 = the room's default
        Condition exitCondition;     ///< "exit_if": the exit works only while true
        std::string locked;          ///< conversation when exit_if is false

        // Zones
        bool trigger = false;        ///< fires when the player walks into it, not on use
        bool repeat = false;         ///< a trigger that can fire again after the player leaves it
        bool cover = false;          ///< a hiding spot (stealth)
        bool rest = false;           ///< a rest spot (dragging)
        bool hasBlock = false;
        Condition block;             ///< "block": the player cannot pass while true
        std::string blocked;         ///< conversation when the player walks into the block

        std::string effect;          ///< "fire", "ember", "glow": drawn by the game
        std::string event;           ///< "event": sent to the room's script on use
    };

    /// X in this room.
    struct CompanionDef
    {
        bool present = false;
        float x = 0.0f;
        std::string mode = "stay";   ///< stay, follow, drag
        std::string pose = "idle";   ///< idle, sit, lying (while staying)
        std::string dialogue;        ///< conversation when the player talks to X
        std::string again;
        std::string bark;            ///< or a bark pool, which does not stop the game
        std::string release;         ///< conversation when the player lets go of X here (G)
        std::string grab;            ///< conversation when the player takes hold of him again
    };

    /// A conversation (and effects) that starts when the player enters the room.
    struct EnterRule
    {
        Condition condition;
        std::string dialogue;
        std::vector<Effect> effects;
        bool once = true;
        std::string where;
    };

    /// One step of a creature's loop.
    struct PatrolPhase
    {
        std::string state;  ///< watch, turned, walk, listen, ...: the room script decides what each means
        float time = 1.0f;  ///< seconds
        float toX = -1.0f;  ///< walk to this x during the phase; -1 = stay
    };

    struct CreatureDef
    {
        bool present = false;
        Condition condition;
        float x = 0.0f;
        float y = 150.0f;            ///< feet; smaller = further away
        float width = 32.0f;
        float height = 52.0f;
        float speed = 30.0f;
        Color color{30, 32, 40, 255};
        std::string sprite;
        float minX = 0.0f;           ///< patrol range
        float maxX = 320.0f;
        std::vector<PatrolPhase> phases;
    };

    /// One wave of small creatures ("crawlers") in a fight.
    struct WaveDef
    {
        int crawlers = 2;
        std::string from = "right"; ///< right, left or both
    };

    /// A real-time fight, started by the command "fight" (apps/app/fight.cpp).
    struct FightDef
    {
        bool present = false;
        bool brute = false;          ///< the room's creature takes part, after the waves
        int hp = 4;                  ///< the brute's strength
        std::vector<WaveDef> waves;
        std::string win;             ///< conversation when every creature is beaten or gone
        std::string lose;            ///< conversation when Y goes down (never game over)
    };

    struct RoomDef
    {
        std::string id;
        std::string name;
        std::string file;            ///< relative to assets/
        std::string background;      ///< 320 x 180, relative to assets/; a flat colour until it exists
        Color color{20, 26, 36, 255};
        Color tint{255, 255, 255, 255}; ///< night, dawn: multiplies the background
        std::vector<std::string> ambient; ///< sound loops, assets/sounds/<id>.ogg, .wav, .mp3 or .qoa
        std::string music;
        float playerX = 40.0f;
        float walkMin = 8.0f;
        float walkMax = 312.0f;
        CompanionDef companion;
        CreatureDef creature;
        FightDef fight;
        std::vector<EnterRule> onEnter;
        std::vector<ObjectDef> objects;
        std::vector<std::string> idleBarks; ///< bark pools for when the player stands still
        std::string script;          ///< logic in apps/app/scripts for this room
    };
} // namespace hollow
