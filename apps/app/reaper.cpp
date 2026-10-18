#include "reaper.hpp"

#include "game.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace hollow;

namespace
{
    constexpr float Creep = 3.0f;        // pixels a second closer while the player stands still
    constexpr float Reach = 30.0f;       // closer than this, it reaches X
    constexpr float Seen = 0.12f;        // its alpha while the player looks back at it
    constexpr float Unseen = 0.55f;

    float number(const std::vector<std::string>& args, std::size_t index, float fallback)
    {
        if (index >= args.size()) return fallback;
        char* end = nullptr;
        const float value = std::strtof(args[index].c_str(), &end);
        return end == args[index].c_str() ? fallback : value;
    }
} // namespace

void Reaper::reset()
{
    shape_.fadeTo(0.0f, 0.0f);
    following_ = false;
    reached_ = false;
    glimpse_ = -1.0f;
    targetAlpha_ = 0.0f;
}

void Reaper::place(float x, float height)
{
    // Smaller is further away: it stands higher up, on the far ground.
    height_ = std::clamp(height, 20.0f, 90.0f);
    const float w = std::round(height_ * 0.4f); // the art is 32 x 80
    const float bottom = layout::GroundY - (80.0f - std::min(80.0f, height_)) * 0.45f;
    shape_.rect = Rectangle{std::round(x - w / 2.0f), std::round(bottom - height_), w, height_};
}

void Reaper::command(Game& game, const std::vector<std::string>& args)
{
    const std::string verb = args.empty() ? std::string() : args[0];
    shape_.sheet = game.reaperSheet();
    if (verb == "show")
    {
        place(number(args, 1, 290.0f), number(args, 2, 80.0f));
        shape_.fadeTo(0.85f, number(args, 3, 2.0f));
        following_ = false;
        glimpse_ = -1.0f;
    }
    else if (verb == "hide")
    {
        shape_.fadeTo(0.0f, number(args, 1, 1.5f));
        glimpse_ = -1.0f;
    }
    else if (verb == "glimpse")
    {
        place(number(args, 1, 290.0f), number(args, 2, 80.0f));
        shape_.fadeTo(0.8f, 0.25f);
        glimpse_ = 0.0f;
        following_ = false;
        game.sound("sting", 0.7f);
    }
    else if (verb == "follow")
    {
        distance_ = gap_ = std::max(Reach + 20.0f, number(args, 1, 120.0f));
        following_ = true;
        glimpse_ = -1.0f;
        targetAlpha_ = -1.0f; // set on the next update
    }
    else if (verb == "stop")
    {
        shape_.fadeTo(0.0f, 1.0f);
        following_ = false;
        glimpse_ = -1.0f;
    }
}

void Reaper::update(Game& game, float dt)
{
    shape_.update(dt);

    if (glimpse_ >= 0.0f)
    {
        glimpse_ += dt;
        if (glimpse_ >= 1.1f)
        {
            shape_.fadeTo(0.0f, 0.6f);
            glimpse_ = -1.0f;
        }
    }
    if (!following_) return;

    // Behind whoever is further back (X, when he follows or is dragged). X
    // left lying alone is what it walks to, steadily, whatever Y does.
    const Actor& player = game.player();
    const Actor& companion = game.companion();
    const bool xAlone = companion.visible && game.companionMode() == CompanionMode::Stay;
    float rear = player.x;
    if (xAlone) rear = companion.x;
    else if (companion.visible) rear = std::min(rear, companion.x);

    const bool lookingBack = player.facing < 0 && !xAlone;
    if (!game.busy())
    {
        if (xAlone) gap_ -= Creep * 2.0f * dt;
        else if (!player.moved() && !lookingBack) gap_ -= Creep * dt;
        if (lookingBack) gap_ = std::min(distance_, gap_ + Creep * 0.5f * dt);
    }
    const float x = rear - gap_;
    place(x, 80.0f);

    const float alpha = x < -10.0f ? 0.0f : lookingBack ? Seen : Unseen;
    if (alpha != targetAlpha_)
    {
        targetAlpha_ = alpha;
        shape_.fadeTo(alpha, 0.8f);
    }

    if (gap_ < Reach && !game.busy())
    {
        // It reaches him. Once a room it costs him; then it falls back.
        gap_ = distance_;
        game.sound("sting");
        game.shake(0.4f, 2.0f);
        if (!reached_ && game.companion().visible)
        {
            reached_ = true;
            game.state().add("x_condition", -1);
            game.bark("x_reaper_close");
        }
    }
}

void Reaper::draw(Game&) const
{
    shape_.draw();
}
