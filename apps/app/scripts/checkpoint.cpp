// Stage 02, the checkpoint: stealth. The creature runs a loop of phases from
// the room's JSON. While it "watch"es the road, the player must be behind
// one of the hiding spots; while it "listen"s, the player must not move.
// Breaking either rule for a moment gets the player spotted:
//
//   SPOTTED_COUNT +1, conversation "checkpoint.spotted" (X_CONDITION -1 on the
//   second time, in its lines), then "event stealth_reset" puts both back at
//   the last hiding spot. The third time is "checkpoint.chase", which ends in
//   the next room: never game over.

#include "creature.hpp"
#include "room_script.hpp"

#include "../game.hpp"

#include <cmath>

using namespace hollow;

namespace
{
    constexpr float Grace = 0.4f;       // seconds of being in the open before it notices
    constexpr float WarnBefore = 0.9f;  // its "!" blinks this long before a dangerous phase

    bool dangerous(const PatrolPhase* phase)
    {
        return phase && (phase->state == "watch" || phase->state == "listen");
    }

    class CheckpointScript final : public RoomScript
    {
    public:
        void enter(Game& game) override
        {
            creature_.reset(game, game.room().creature);
            loop_.reset(game.room().creature.phases);
            creature_.look = loop_.current() ? loop_.current()->state : "turned";
            lastCover_ = game.player().x;
        }

        void update(Game& game, float dt) override
        {
            creature_.update(dt);
            if (!active_ || game.busy()) return;

            if (loop_.update(dt))
            {
                const PatrolPhase* phase = loop_.current();
                creature_.look = phase->state;
                if (phase->toX >= 0.0f) creature_.walkTo(phase->toX);
            }

            Actor& player = game.player();
            const ObjectDef* cover = game.coverAt(player.x);
            if (cover)
            {
                lastCover_ = cover->rect.x + cover->rect.width / 2.0f;
                if (!wasHidden_ && game.random().chance(0.35f)) game.bark("hide");
            }
            wasHidden_ = cover != nullptr;
            player.pose = cover && !player.moved() ? "kneel" : "idle";

            const PatrolPhase* phase = loop_.current();
            const bool seen = (phase->state == "watch" && !cover) || (phase->state == "listen" && player.moved());
            suspicion_ = seen ? suspicion_ + dt : std::max(0.0f, suspicion_ - dt * 0.5f);
            if (suspicion_ >= Grace) spotted(game);
        }

        void drawBack(Game& game) override
        {
            creature_.draw(game);
            if (!active_ || creature_.gone()) return;

            const Rectangle r = creature_.bounds();
            const Vector2 mark{r.x + r.width / 2.0f - 1, r.y - 12};
            const float t = static_cast<float>(GetTime());
            const UiConfig& ui = game.ui();

            // It noticed something: a solid "!" that grows with suspicion.
            if (suspicion_ > 0.0f)
            {
                const float h = 3.0f + 5.0f * std::min(1.0f, suspicion_ / Grace);
                DrawRectangleRec(Rectangle{mark.x, mark.y + 8 - h, 2, h}, ui.highlight);
                DrawRectangleRec(Rectangle{mark.x, mark.y + 10, 2, 2}, ui.highlight);
            }
            // About to turn or listen: a blinking dim "!".
            else if (!dangerous(loop_.current()) && dangerous(loop_.next()) && loop_.timeLeft() < WarnBefore &&
                     std::fmod(t, 0.3f) < 0.18f)
            {
                DrawRectangleRec(Rectangle{mark.x, mark.y + 2, 2, 6}, ui.dim);
                DrawRectangleRec(Rectangle{mark.x, mark.y + 10, 2, 2}, ui.dim);
            }
            // Listening: two small arcs beside its head.
            else if (loop_.current() && loop_.current()->state == "listen")
            {
                DrawRectangleRec(Rectangle{r.x - 4, r.y + 6, 1, 4}, ui.dim);
                DrawRectangleRec(Rectangle{r.x + r.width + 3, r.y + 6, 1, 4}, ui.dim);
            }
        }

        bool event(Game& game, std::string_view name, const std::vector<std::string>&) override
        {
            if (name == "stealth_start")
            {
                active_ = true;
                return true;
            }
            if (name == "stealth_reset")
            {
                // Back behind the last hiding spot, the creature's loop from the start.
                game.player().x = lastCover_;
                game.player().stop();
                game.companion().x = lastCover_ - 10.0f;
                game.companion().stop();
                creature_.reset(game, game.room().creature);
                loop_.reset(game.room().creature.phases);
                creature_.look = loop_.current() ? loop_.current()->state : "turned";
                suspicion_ = 0.0f;
                active_ = true;
                return true;
            }
            if (name == "stealth_stop")
            {
                active_ = false;
                return true;
            }
            return false;
        }

    private:
        void spotted(Game& game)
        {
            suspicion_ = 0.0f;
            active_ = false;
            creature_.look = "alert";
            game.state().add("spotted_count", 1);
            game.shake(0.4f, 2.0f);
            game.sound("alert");
            game.player().pose = "idle";
            game.startDialogue(game.state().get("spotted_count") >= 3 ? "checkpoint.chase" : "checkpoint.spotted");
        }

        Creature creature_;
        PhaseLoop loop_;
        bool active_ = false;
        bool wasHidden_ = false;
        float suspicion_ = 0.0f;
        float lastCover_ = 0.0f;
    };
} // namespace

std::unique_ptr<RoomScript> makeCheckpointScript()
{
    return std::make_unique<CheckpointScript>();
}
