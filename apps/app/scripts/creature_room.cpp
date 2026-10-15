// A room with the creature in it that is met through dialogue: the factory
// floor (it patrols; the encounter is a timed choice) and the broken stairs
// (it blocks the path). The choices send events back here:
//
//   event creature_alert       it heard the cabinet: faster, eyes on us
//   event creature_guard <x>   it plants itself at x (Y lost the fight)
//   event creature_leave [x]   it walks off and is gone
//
// "fight" (apps/app/fight.cpp) takes the creature over while it lasts and
// gives it back if it is still standing at the end.

#include "creature.hpp"
#include "room_script.hpp"

#include "../game.hpp"

#include <cstdlib>

using namespace hollow;

namespace
{
    class CreatureRoomScript final : public RoomScript
    {
    public:
        void enter(Game& game) override
        {
            creature_.reset(game, game.room().creature);
            const CreatureDef& def = game.room().creature;
            patrolling_ = def.maxX > def.minX;
            creature_.look = patrolling_ ? "turned" : "watch";
        }

        void update(Game& game, float dt) override
        {
            if (patrolling_ && !game.busy() && !creature_.walking())
            {
                // Back and forth between the ends of its patrol.
                const CreatureDef& def = game.room().creature;
                const float far = std::abs(creature_.x - def.minX) < std::abs(creature_.x - def.maxX) ? def.maxX : def.minX;
                creature_.walkTo(far);
            }
            if (!game.busy() || !patrolling_) creature_.update(dt);
        }

        void drawBack(Game& game) override
        {
            if (!taken_) creature_.draw(game);
        }

        [[nodiscard]] std::optional<CreatureState> creature() const override
        {
            if (taken_ || creature_.gone()) return std::nullopt;
            return CreatureState{creature_.x, creature_.speedScale};
        }

        void creatureTaken() override
        {
            taken_ = true;
            patrolling_ = false;
        }

        void creatureReturned(Game&, float x) override
        {
            taken_ = false;
            creature_.x = x;
            creature_.visible = true;
            creature_.alpha = 1.0f;
            creature_.look = "watch";
        }

        bool event(Game& game, std::string_view name, const std::vector<std::string>& args) override
        {
            const auto arg = [&](std::size_t i, float fallback) {
                return i < args.size() ? std::strtof(args[i].c_str(), nullptr) : fallback;
            };
            if (name == "creature_alert")
            {
                creature_.speedScale = 1.8f;
                creature_.look = "alert";
                return true;
            }
            if (name == "creature_guard")
            {
                patrolling_ = false;
                taken_ = false;
                creature_.visible = true;
                creature_.alpha = 1.0f;
                creature_.look = "watch";
                creature_.walkTo(arg(0, game.room().creature.maxX));
                return true;
            }
            if (name == "creature_leave")
            {
                patrolling_ = false;
                creature_.look = "turned";
                creature_.leave(arg(0, 360.0f));
                return true;
            }
            return false;
        }

    private:
        Creature creature_;
        bool patrolling_ = false;
        bool taken_ = false; // the fight has it
    };
} // namespace

std::unique_ptr<RoomScript> makeCreatureRoomScript()
{
    return std::make_unique<CreatureRoomScript>();
}
