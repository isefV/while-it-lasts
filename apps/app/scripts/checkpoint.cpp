// Stage 02, the checkpoint: stealth. The creature runs a loop of phases from
// the room's JSON. While it "watch"es the road, the player must be behind
// one of the hiding spots; while it "listen"s, the player must not move.
// Breaking either rule for a moment gets the player spotted:
//
//   SPOTTED_COUNT +1, conversation "checkpoint.spotted" (X_CONDITION -1 on the
//   second time, in its lines), then "event stealth_reset" puts both back at
//   the last hiding spot. The third time is "checkpoint.chase", which ends in
//   the next room: never game over.
//
// Sneaking is not the only way: a thrown bottle or bolt ("event distract",
// from the item's "use" conversation) sends it off into the trees for a
// while, and the road is open. STEALTH_ACTIVE is 1 while a throw would work.

#include "creature.hpp"
#include "room_script.hpp"

#include "../game.hpp"

#include <cmath>

using namespace hollow;

namespace
{
    constexpr float Grace = 0.4f;       // seconds of being in the open before it notices
    constexpr float WarnBefore = 0.9f;  // its "!" blinks this long before a dangerous phase
    constexpr float AwayFor = 12.0f;    // seconds a throw keeps it busy in the trees
    constexpr float AwayX = 345.0f;     // where it goes: off the road, into the dark

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
            game.state().set("stealth_active", active_ && away_ < 0.0f ? 1 : 0);
            if (!active_ || game.busy()) return;

            // Gone after the noise; then back to its round from the start.
            if (away_ >= 0.0f)
            {
                away_ += dt;
                if (away_ >= AwayFor)
                {
                    away_ = -1.0f;
                    creature_.reset(game, game.room().creature);
                    creature_.x = AwayX;
                    creature_.walkTo(game.room().creature.x);
                    loop_.reset(game.room().creature.phases);
                    creature_.look = "turned";
                    suspicion_ = 0.0f;
                }
                game.player().pose = "idle";
                return;
            }

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
            // Sneaking: a crouch walk, and still behind cover, down on one knee.
            player.pose = cover && !player.moved() ? "kneel" : "crouch";

            const PatrolPhase* phase = loop_.current();
            const bool seen = (phase->state == "watch" && !cover) || (phase->state == "listen" && player.moved());
            suspicion_ = seen ? suspicion_ + dt : std::max(0.0f, suspicion_ - dt * 0.5f);
            if (suspicion_ >= Grace) spotted(game);
        }

        void drawBack(Game& game) override
        {
            creature_.draw(game);
            if (!active_ || creature_.gone()) return;

            if (away_ >= 0.0f) return;
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
                away_ = -1.0f;
                active_ = true;
                return true;
            }
            if (name == "stealth_stop")
            {
                active_ = false;
                game.player().pose = "idle";
                return true;
            }
            if (name == "distract")
            {
                if (!active_ || away_ >= 0.0f) return true;
                // It lurches off after the sound, into the trees past the road.
                away_ = 0.0f;
                suspicion_ = 0.0f;
                creature_.look = "turned";
                creature_.leave(AwayX);
                game.sound("glass");
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
        float away_ = -1.0f; // seconds since a throw sent it off; -1: it is here
    };
} // namespace

std::unique_ptr<RoomScript> makeCheckpointScript()
{
    return std::make_unique<CheckpointScript>();
}
