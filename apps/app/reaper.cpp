#include "reaper.hpp"

#include "game.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace while_it_lasts;

namespace
{
    constexpr float Creep = 3.0f;        // pixels a second closer while the player stands still
    constexpr float Reach = 30.0f;       // closer than this, it reaches X
    constexpr float Seen = 0.12f;        // its alpha while the player looks back at it
    constexpr float Unseen = 0.55f;

    // Stalking: how far away it first shows up, how much closer each time,
    // and how close it ever gets; seconds shown, and hidden in between.
    constexpr float StalkStart = 170.0f;
    constexpr float StalkStep = 25.0f;
    constexpr float StalkClosest = 60.0f;
    constexpr float StalkShown = 2.4f;
    constexpr float StalkHiddenMin = 5.0f;
    constexpr float StalkHiddenMax = 9.0f;

    // Chasing: a little slower than dragging X (48 x 0.45, less when hurt).
    constexpr float ChaseSpeed = 15.0f;
    constexpr float ChaseFarthest = 160.0f; // it never falls further back than this
    constexpr float CatchReach = 22.0f;
    constexpr float CatchKnockback = 100.0f; // then about 5 s before it catches them again

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
    shape_.flipX = false;
    following_ = false;
    stalking_ = stalkShown_ = false;
    chasing_ = chaseStarted_ = false;
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
    if (verb != "stalk") stalking_ = false;
    if (verb != "chase") chasing_ = false;
    if (verb == "stalk")
    {
        stalking_ = true;
        stalkShown_ = false;
        stalkGap_ = StalkStart;
        stalkTimer_ = game.random().uniform(2.5f, 4.0f); // the first time comes soon
        following_ = false;
        glimpse_ = -1.0f;
        shape_.fadeTo(0.0f, 1.0f);
    }
    else if (verb == "chase")
    {
        chasing_ = true;
        chaseStarted_ = false; // placed on the next update, behind them
        chaseDistance_ = number(args, 1, 110.0f);
        following_ = false;
        glimpse_ = -1.0f;
    }
    else if (verb == "show")
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
    if (stalking_) updateStalk(game, dt);
    if (chasing_) updateChase(game, dt);
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

float Reaper::rear(Game& game)
{
    const Actor& companion = game.companion();
    return companion.visible ? std::min(game.player().x, companion.x) : game.player().x;
}

void Reaper::updateStalk(Game& game, float dt)
{
    if (game.busy()) return; // it waits out conversations
    stalkTimer_ -= dt;
    if (stalkTimer_ > 0.0f) return;

    if (stalkShown_)
    {
        // Gone again; next time a little closer.
        shape_.fadeTo(0.0f, 1.2f);
        stalkShown_ = false;
        stalkGap_ = std::max(StalkClosest, stalkGap_ - StalkStep);
        stalkTimer_ = game.random().uniform(StalkHiddenMin, StalkHiddenMax);
        return;
    }

    // Behind them on the path; with no room behind, waiting ahead of them.
    const float back = rear(game);
    float x = back - stalkGap_;
    if (x < 30.0f) x = std::min(game.player().x + stalkGap_, layout::SceneWidth - 30.0f); // clear of the foliage frame
    place(x, 80.0f);
    shape_.flipX = x > game.player().x; // the art faces right: it faces them
    shape_.fadeTo(0.8f, 0.7f);
    game.sound("sting", 0.45f);
    game.shake(0.2f, 1.0f);
    stalkShown_ = true;
    stalkTimer_ = StalkShown;
}

void Reaper::updateChase(Game& game, float dt)
{
    const float back = rear(game);
    if (!chaseStarted_)
    {
        chaseStarted_ = true;
        chaseX_ = back - chaseDistance_;
        targetAlpha_ = -1.0f;
    }
    if (!game.busy()) chaseX_ += ChaseSpeed * dt;
    chaseX_ = std::max(chaseX_, back - ChaseFarthest);
    place(chaseX_, 80.0f);
    shape_.flipX = false;

    const float alpha = chaseX_ < -10.0f ? 0.0f : 0.8f;
    if (alpha != targetAlpha_)
    {
        targetAlpha_ = alpha;
        shape_.fadeTo(alpha, 0.8f);
    }

    if (back - chaseX_ < CatchReach && !game.busy())
    {
        // It catches them: it costs both, and it falls back a little.
        chaseX_ = back - CatchKnockback;
        game.sound("sting");
        game.sound("hurt", 0.8f);
        game.shake(0.5f, 3.0f);
        game.state().add("y_health", -1);
        game.state().set("y_hurt", 1);
        if (game.companion().visible)
        {
            game.state().add("x_condition", -1);
            game.bark("x_reaper_close");
        }
    }
}

void Reaper::draw(Game&) const
{
    shape_.draw();
}
