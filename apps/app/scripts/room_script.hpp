#pragma once

// Logic for one room that the data files cannot express: timers, the
// creature's patrol, stealth, the ending. A room names its script with
// "script" in its JSON; makeRoomScript() creates it.

#include <memory>
#include <string>
#include <string_view>
#include <vector>

class Game;

class RoomScript
{
public:
    virtual ~RoomScript() = default;

    /// After the room is set up, before its "enter" conversation.
    virtual void enter(Game&) {}

    /// Every frame, also while a conversation runs (check game.busy()).
    virtual void update(Game&, float) {}

    /// Drawn after the background, before props and characters.
    virtual void drawBack(Game&) {}

    /// Drawn over everything in the scene.
    virtual void drawFront(Game&) {}

    /// "event <name> ..." from dialogue or an object's "event". Returns false
    /// for events the script does not know.
    virtual bool event(Game&, std::string_view /*name*/, const std::vector<std::string>& /*args*/) { return false; }
};

/// The script called @p name, or nullptr for an empty or unknown name.
std::unique_ptr<RoomScript> makeRoomScript(std::string_view name);

/// Every script name, for the data check.
const std::vector<std::string>& roomScriptNames();
