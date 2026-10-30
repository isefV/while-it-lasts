#include "room_script.hpp"

std::unique_ptr<RoomScript> makeCampsiteScript();
std::unique_ptr<RoomScript> makeCreatureRoomScript();
std::unique_ptr<RoomScript> makeCheckpointScript();
std::unique_ptr<RoomScript> makeSunriseScript();
std::unique_ptr<RoomScript> makeSummitScript();

std::unique_ptr<RoomScript> makeRoomScript(std::string_view name)
{
    if (name == "campsite") return makeCampsiteScript();
    if (name == "creature") return makeCreatureRoomScript();
    if (name == "checkpoint") return makeCheckpointScript();
    if (name == "sunrise") return makeSunriseScript();
    if (name == "summit") return makeSummitScript();
    return nullptr;
}

const std::vector<std::string>& roomScriptNames()
{
    static const std::vector<std::string> names = {"campsite", "creature", "checkpoint", "sunrise", "summit"};
    return names;
}
