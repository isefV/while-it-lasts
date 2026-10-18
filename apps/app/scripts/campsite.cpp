// Stage 00, the campsite: the fire going out, the first glimpse of the
// Reaper, and the short escape after the sound in the trees.

#include "room_script.hpp"

#include "../game.hpp"

using namespace hollow;

namespace
{
    constexpr float HurryAfter = 20.0f;  // seconds after the sound: branches snap, X urges
    constexpr float FigureAfter = 32.0f; // a tall silhouette between the trees, X_CONDITION -1
    const Color ColdNight{150, 165, 200, 255}; // the night without the fire

    class CampsiteScript final : public RoomScript
    {
    public:
        void enter(Game& game) override
        {
            hint_.rect = Rectangle{6, 66, 24, 60}; // the art is 32 x 80
            figure_.rect = Rectangle{266, 58, 29, 72};
            hint_.sheet = figure_.sheet = game.reaperSheet();
        }

        void update(Game& game, float dt) override
        {
            hint_.update(dt);
            figure_.update(dt);

            if (hintTime_ >= 0.0f)
            {
                hintTime_ += dt;
                if (hintTime_ > 2.5f)
                {
                    hint_.fadeTo(0.0f, 1.5f);
                    hintTime_ = -1.0f;
                }
            }

            // The escape clock runs only while the player is free to act.
            if (escape_ < 0.0f || game.busy()) return;
            escape_ += dt;
            if (escape_ >= HurryAfter && !hurried_)
            {
                hurried_ = true;
                game.sound("branches");
                game.shake(0.3f, 1.0f);
                game.startDialogue("campsite.hurry");
            }
            else if (escape_ >= FigureAfter && !figureShown_)
            {
                figureShown_ = true;
                figure_.fadeTo(0.85f, 2.0f);
                game.state().add("x_condition", -1);
                game.startDialogue("campsite.figure");
            }
        }

        void drawFront(Game&) override
        {
            hint_.draw();
            figure_.draw();
        }

        bool event(Game& game, std::string_view name, const std::vector<std::string>&) override
        {
            if (name == "reaper_hint")
            {
                hint_.fadeTo(0.7f, 1.2f);
                hintTime_ = 0.0f;
                return true;
            }
            if (name == "fire_out")
            {
                game.sound("dirt");
                game.setTint(ColdNight, 2.5f);
                game.setAmbient({"wind"});
                return true;
            }
            if (name == "escape_start")
            {
                escape_ = 0.0f;
                return true;
            }
            return false;
        }

    private:
        Apparition hint_;
        Apparition figure_;
        float hintTime_ = -1.0f;
        float escape_ = -1.0f;
        bool hurried_ = false;
        bool figureShown_ = false;
    };
} // namespace

std::unique_ptr<RoomScript> makeCampsiteScript()
{
    return std::make_unique<CampsiteScript>();
}
