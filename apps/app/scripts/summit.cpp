// Stage 04, the summit, and the climb before it, at the end of the night.
//
// "sunrise": the sky lightens and warms as the player climbs (left to right).
// They left the campsite at midnight; the sun comes up over the town.
//
// "summit": after the last cigarette the player may look at the horizon, X
// or the cigarette. After two of those (summit_moments) or half a minute,
// the Reaper scene starts. Its conversation drives the apparitions:
//
//   event wind_stop         only the cigarette's crackle is left
//   event reaper_appear     the Reaper at the edge of the screen
//   event spirit_rise       a faint light rises from X
//   event spirit_to_reaper  it drifts to the Reaper and fades
//   event reaper_vanish     the Reaper turns into the dark
//   event final_image       the morning light comes up, the ember fades

#include "room_script.hpp"

#include "../game.hpp"

using namespace hollow;

namespace
{
    constexpr float ReaperAfter = 30.0f;  // seconds of quiet after the cigarette
    constexpr float SunTime = 7.0f;       // the morning light coming up, in the final image
    const Color Dawn{248, 206, 176, 255};
    const Color Morning{255, 240, 222, 255};

    class SunriseScript final : public RoomScript
    {
    public:
        void enter(Game& game) override { start_ = game.room().tint; }

        void update(Game& game, float) override
        {
            const RoomDef& room = game.room();
            t_ = smoothstep((game.player().x - room.walkMin) / std::max(1.0f, room.walkMax - room.walkMin));
            game.setTint(mix(start_, Dawn, t_), 0.0f);
        }

    private:
        Color start_{};
        float t_ = 0.0f;
    };

    class SummitScript final : public RoomScript
    {
    public:
        void enter(Game& game) override
        {
            reaper_.rect = Rectangle{285, 62, 32, 80};
            reaper_.color = Color{6, 6, 10, 255};
            reaper_.sheet = game.reaperSheet();
            reaper_.flipX = true; // the sheet faces right; here it stands right of them and faces them
            spirit_.color = Color{230, 236, 244, 255};
            spirit_.texture = game.assets().texture("sprites/spirit.png");
        }

        void update(Game& game, float dt) override
        {
            reaper_.update(dt);
            spirit_.update(dt);

            if (started_ || game.busy() || game.state().get("cig_scene_done") == 0) return;
            quiet_ += dt;
            if (game.state().get("summit_moments") >= 2 || quiet_ >= ReaperAfter)
            {
                started_ = true;
                game.startDialogue("summit.reaper");
            }
        }

        void drawFront(Game&) override
        {
            reaper_.draw();
            spirit_.draw();
        }

        bool event(Game& game, std::string_view name, const std::vector<std::string>&) override
        {
            if (name == "wind_stop")
            {
                game.setAmbient({"ember"});
                return true;
            }
            if (name == "reaper_appear")
            {
                reaper_.fadeTo(1.0f, 3.0f);
                return true;
            }
            if (name == "spirit_rise")
            {
                const Rectangle body = game.companion().bounds();
                spirit_.rect = Rectangle{body.x + body.width / 2.0f - 8, body.y - 10, 16, 30};
                spirit_.fadeTo(0.75f, 2.5f);
                spirit_.moveTo(Vector2{spirit_.rect.x, spirit_.rect.y - 18}, 3.0f);
                return true;
            }
            if (name == "spirit_to_reaper")
            {
                spirit_.moveTo(Vector2{reaper_.rect.x - 6, reaper_.rect.y + 20}, 5.0f);
                spirit_.fadeTo(0.0f, 5.0f);
                return true;
            }
            if (name == "reaper_vanish")
            {
                reaper_.fadeTo(0.0f, 3.0f);
                return true;
            }
            if (name == "final_image")
            {
                // No sun drawn: the morning is the light (the tint) alone.
                game.setTint(Morning, SunTime);
                game.state().set("ember_out", 1);
                return true;
            }
            return false;
        }

    private:
        Apparition reaper_;
        Apparition spirit_;
        float quiet_ = 0.0f;
        bool started_ = false;
    };
} // namespace

std::unique_ptr<RoomScript> makeSunriseScript()
{
    return std::make_unique<SunriseScript>();
}

std::unique_ptr<RoomScript> makeSummitScript()
{
    return std::make_unique<SummitScript>();
}
