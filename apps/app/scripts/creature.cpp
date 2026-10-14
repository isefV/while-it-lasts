#include "creature.hpp"

#include "../game.hpp"

#include <cmath>

using namespace hollow;

void Creature::reset(Game& game, const CreatureDef& creature)
{
    def = &creature;
    x = creature.x;
    y = creature.y;
    facing = -1;
    speedScale = 1.0f;
    alpha = 1.0f;
    visible = creature.present && creature.condition.evaluate(game.state());
    look = "turned";
    target_.reset();
    leaving_ = false;

    Diagnostics diag;
    sheet_ = loadSpriteSheet(game.assets(), creature.sprite, diag);
}

void Creature::walkTo(float targetX)
{
    target_ = targetX;
}

void Creature::leave(float targetX)
{
    target_ = targetX;
    leaving_ = true;
    speedScale = std::max(speedScale, 1.6f);
}

void Creature::update(float dt)
{
    bool moved = false;
    if (target_)
    {
        const float before = x;
        x = approach(x, *target_, def->speed * speedScale * dt);
        if (x != before)
        {
            facing = x > before ? 1 : -1;
            moved = true;
        }
        if (std::fabs(x - *target_) < 0.01f) target_.reset();
    }
    if (leaving_)
    {
        alpha = approach(alpha, 0.0f, dt * 0.7f);
        if (alpha <= 0.0f) visible = false;
    }
    animator_.play(moved ? "walk" : "idle");
    animator_.update(dt, sheet_ ? &*sheet_ : nullptr);
}

Rectangle Creature::bounds() const
{
    return Rectangle{std::round(x - def->width / 2.0f), std::round(y - def->height), def->width, def->height};
}

void Creature::draw(Game& game) const
{
    if (!visible || alpha <= 0.0f || !def) return;
    const Color tint = ColorAlpha(WHITE, alpha);
    if (sheet_ && drawSprite(*sheet_, animator_, Vector2{x, y}, facing > 0, tint)) return;

    // Placeholder: a hunched dark mass, wider at the shoulders, and its eyes
    // when it looks our way.
    const Rectangle r = bounds();
    const Color body = ColorAlpha(def->color, alpha);
    const float shoulder = std::round(r.height * 0.3f);
    DrawRectangleRec(Rectangle{r.x, r.y + shoulder, r.width, r.height - shoulder}, body);
    const float headW = std::round(r.width * 0.45f);
    const float headX = facing > 0 ? r.x + r.width - headW - 2 : r.x + 2;
    DrawRectangleRec(Rectangle{look == "watch" ? r.x + std::round((r.width - headW) / 2) : headX,
                               r.y + std::round(shoulder * 0.35f), headW, shoulder},
                     body);

    const Color eye = ColorAlpha(game.ui().highlight, alpha);
    const float eyeY = r.y + std::round(shoulder * 0.35f) + 4;
    const float mid = r.x + r.width / 2.0f;
    if (look == "watch" || look == "alert")
    {
        DrawRectangleRec(Rectangle{std::round(mid - 3), eyeY, 2, 1}, eye);
        DrawRectangleRec(Rectangle{std::round(mid + 1), eyeY, 2, 1}, eye);
    }
    else if (look == "listen")
    {
        const float ex = facing > 0 ? headX + headW - 3 : headX + 1;
        DrawRectangleRec(Rectangle{ex, eyeY, 2, 1}, ColorAlpha(eye, 0.6f * alpha));
    }
}

void PhaseLoop::reset(const std::vector<PatrolPhase>& phases)
{
    phases_ = &phases;
    index_ = 0;
    time_ = 0.0f;
}

bool PhaseLoop::update(float dt)
{
    if (!phases_ || phases_->empty()) return false;
    time_ += dt;
    bool changed = false;
    while (time_ >= (*phases_)[index_].time)
    {
        time_ -= (*phases_)[index_].time;
        index_ = (index_ + 1) % phases_->size();
        changed = true;
    }
    return changed;
}

const PatrolPhase* PhaseLoop::current() const
{
    return phases_ && !phases_->empty() ? &(*phases_)[index_] : nullptr;
}

const PatrolPhase* PhaseLoop::next() const
{
    return phases_ && !phases_->empty() ? &(*phases_)[(index_ + 1) % phases_->size()] : nullptr;
}

float PhaseLoop::timeLeft() const
{
    const PatrolPhase* phase = current();
    return phase ? phase->time - time_ : 0.0f;
}
