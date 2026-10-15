#include "fight.hpp"

#include "game.hpp"
#include "scripts/room_script.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>

using namespace hollow;

namespace
{
    constexpr float SwingCooldown = 0.42f;
    constexpr float SwingActive = 0.08f;  // seconds into the swing when it hits
    constexpr float SwingShow = 0.16f;
    constexpr float SwingReach = 26.0f;   // in front of Y
    constexpr float ShotCooldown = 0.8f;
    constexpr float ShotRange = 220.0f;
    constexpr int ShotDamage = 3;
    constexpr float DodgeTime = 0.2f;
    constexpr float DodgeDistance = 30.0f;
    constexpr float DodgeCooldown = 0.7f;
    constexpr float DodgeSafe = 0.35f;    // no lunge lands this long after Q
    constexpr float HurtSafe = 0.9f;      // after a hit
    constexpr float WavePause = 1.8f;
    constexpr float DownTime = 1.6f;      // Y on the ground before the "lose" conversation
    constexpr float Edge = 14.0f;         // creatures enter this far outside the scene

    const Color Blood{122, 30, 30, 255};

    float side(float from, float to)
    {
        return to >= from ? 1.0f : -1.0f;
    }
} // namespace

// ---------------------------------------------------------------------------
// Start and end
// ---------------------------------------------------------------------------

void Fight::start(Game& game)
{
    const RoomDef& room = game.room();
    if (!room.fight.present || active_) return;
    active_ = true;
    def_ = &room.fight;
    creature_ = room.creature.present ? &room.creature : nullptr;
    foes_.clear();
    wave_ = 0;
    bruteSent_ = false;
    nextWave_ = 0.0f;
    xHitThisWave_ = false;
    swingCooldown_ = shotCooldown_ = dodgeCooldown_ = 0.0f;
    swingTime_ = -1.0f;
    dodgeTime_ = invulnerable_ = hurtFlash_ = banner_ = 0.0f;
    downTime_ = -1.0f;
    noAmmo_ = holding_ = 0.0f;
    game.state().set("fighting", 1);

    // The first wave at once; with no waves, the brute.
    if (!def_->waves.empty()) spawnWave(game);
    else spawnBrute(game);
}

void Fight::stop()
{
    active_ = false;
    foes_.clear();
}

void Fight::finish(Game& game, bool won)
{
    // A brute still standing goes back to the room's script (it guards).
    for (const Foe& foe : foes_)
        if (foe.kind == Kind::Brute && foe.hp > 0)
            if (RoomScript* script = game.script()) script->creatureReturned(game, foe.x);

    active_ = false;
    foes_.clear();
    game.state().set("fighting", 0);
    game.player().pose = "idle";
    if (!won) game.state().set("y_health", std::max(1, game.state().get("y_health")));
    game.startDialogue(won ? def_->win : def_->lose);
}

// ---------------------------------------------------------------------------
// The creatures
// ---------------------------------------------------------------------------

Fight::Params Fight::params(const Foe& foe) const
{
    if (foe.kind == Kind::Brute)
    {
        const float w = creature_ ? creature_->width : 34.0f;
        const float h = creature_ ? creature_->height : 54.0f;
        const float speed = (creature_ ? creature_->speed : 30.0f) * 1.3f * bruteSpeed_;
        const float quick = std::clamp(bruteSpeed_, 1.0f, 1.4f);
        return Params{w, h, speed, 34.0f, 30.0f, 0.3f, 0.8f / quick, 1.0f, 18.0f, 2};
    }
    return Params{16.0f, 9.0f, 40.0f, 26.0f, 26.0f, 0.22f, 0.55f, 0.75f, 10.0f, 1};
}

Rectangle Fight::bounds(const Foe& foe) const
{
    const Params p = params(foe);
    return Rectangle{std::round(foe.x - p.width / 2.0f), layout::GroundY - p.height, p.width, p.height};
}

bool Fight::xTargetable(Game& game) const
{
    const Actor& x = game.companion();
    return x.visible && game.companionMode() != CompanionMode::Drag && !game.coverAt(x.x);
}

float Fight::targetX(Game& game, const Foe& foe) const
{
    return foe.forX && xTargetable(game) ? game.companion().x : game.player().x;
}

void Fight::spawnWave(Game& game)
{
    const WaveDef& wave = def_->waves[wave_++];
    xHitThisWave_ = false;
    const bool xHere = xTargetable(game);
    for (int i = 0; i < wave.crawlers; ++i)
    {
        Foe foe;
        foe.kind = Kind::Crawler;
        bool left = wave.from == "left";
        if (wave.from == "both") left = i % 2 == 1;
        foe.x = left ? -Edge : layout::SceneBox.width + Edge;
        foe.facing = left ? 1 : -1;
        foe.delay = 0.7f * static_cast<float>(i / (wave.from == "both" ? 2 : 1));
        foe.hp = foe.maxHp = 2;
        // Those coming from X's side go for him.
        foe.forX = xHere && (left == (game.companion().x < game.player().x));
        foes_.push_back(foe);
    }
}

void Fight::spawnBrute(Game& game)
{
    bruteSent_ = true;
    Foe foe;
    foe.kind = Kind::Brute;
    foe.hp = foe.maxHp = def_->hp;
    foe.x = layout::SceneBox.width + Edge;
    bruteSpeed_ = 1.0f;
    if (RoomScript* script = game.script())
    {
        if (const auto c = script->creature())
        {
            foe.x = c->x;
            bruteSpeed_ = c->speedScale;
            script->creatureTaken();
        }
    }
    foe.facing = foe.x > game.player().x ? -1 : 1;
    foe.state = State::Approach;
    game.sound("roar", 0.8f);
    foes_.push_back(foe);
}

void Fight::setState(Foe& foe, State state)
{
    foe.state = state;
    foe.time = 0.0f;
    if (state == State::Lunge)
    {
        foe.lungeFrom = foe.x;
        foe.hit = false;
    }
}

// ---------------------------------------------------------------------------
// Hits
// ---------------------------------------------------------------------------

void Fight::damage(Game& game, Foe& foe, int amount, int fromSide)
{
    foe.hp -= amount;
    foe.flash = 0.12f;
    const Rectangle r = bounds(foe);
    Particles::Style spray;
    spray.color = Color{20, 22, 28, 255};
    spray.velocityMin = Vector2{static_cast<float>(fromSide) * 10.0f, -24.0f};
    spray.velocityMax = Vector2{static_cast<float>(fromSide) * 40.0f, -6.0f};
    spray.life = 0.6f;
    game.particles().emit(Vector2{foe.x, r.y + r.height * 0.4f}, foe.kind == Kind::Brute ? 7 : 4, spray, game.random());

    if (foe.kind == Kind::Crawler)
    {
        foe.x += static_cast<float>(fromSide) * 14.0f;
        if (foe.hp <= 0)
        {
            setState(foe, State::Dead);
            game.sound("crawler", 0.6f, game.random().uniform(1.1f, 1.3f));
            return;
        }
        setState(foe, State::Stagger); // a hit stops its lunge
        return;
    }

    foe.x += static_cast<float>(fromSide) * 4.0f;
    if (foe.hp <= 0)
    {
        setState(foe, State::Flee);
        game.sound("roar", 0.9f, 0.85f);
        game.shake(0.4f, 2.0f);
        return;
    }
    if (foe.state == State::Recover || foe.state == State::Windup) setState(foe, State::Stagger);
}

void Fight::hurtPlayer(Game& game, int amount, int fromSide)
{
    StoryState& state = game.state();
    state.add("y_health", -amount);
    state.set("y_hurt", 1);
    invulnerable_ = HurtSafe;
    hurtFlash_ = 1.0f;
    game.shake(0.35f, 2.5f);
    game.sound("hurt", 1.0f, game.random().uniform(0.9f, 1.05f));
    Actor& player = game.player();
    player.x = std::clamp(player.x + static_cast<float>(fromSide) * 10.0f, game.room().walkMin, game.room().walkMax);

    if (state.get("y_health") <= 0)
    {
        downTime_ = 0.0f;
        player.pose = "lying";
        for (Foe& foe : foes_)
            if (foe.state != State::Dead) setState(foe, State::Flee);
    }
}

void Fight::hurtCompanion(Game& game, Foe& foe)
{
    xHitThisWave_ = true;
    game.state().add("x_condition", -1);
    game.shake(0.3f, 1.5f);
    game.sound("hurt", 0.7f, 1.25f);
    game.bark("x_hurt");
    setState(foe, State::Flee); // it got what it came for
}

void Fight::swing(Game& game)
{
    const Actor& player = game.player();
    const float from = player.facing > 0 ? player.x : player.x - SwingReach;
    const float to = from + SwingReach;
    bool landed = false;
    for (Foe& foe : foes_)
    {
        if (foe.state == State::Dead || foe.state == State::Flee || foe.state == State::Enter) continue;
        const Rectangle r = bounds(foe);
        if (r.x + r.width < from || r.x > to) continue;
        damage(game, foe, 1, player.facing);
        landed = true;
    }
    if (landed)
    {
        game.sound("thud", 0.9f, game.random().uniform(0.9f, 1.15f));
        game.shake(0.12f, 1.0f);
    }
}

void Fight::shoot(Game& game)
{
    StoryState& state = game.state();
    if (!state.hasItem("pistol")) return;
    if (state.get("ammo") <= 0)
    {
        game.sound("click");
        noAmmo_ = 1.2f;
        return;
    }
    state.add("ammo", -1);
    shotCooldown_ = ShotCooldown;
    shotFlash_ = 0.07f;
    game.sound("shot");
    game.shake(0.25f, 2.0f);

    const Actor& player = game.player();
    Foe* target = nullptr;
    float best = ShotRange;
    for (Foe& foe : foes_)
    {
        if (foe.state == State::Dead || foe.state == State::Flee || foe.state == State::Enter) continue;
        const float d = (foe.x - player.x) * static_cast<float>(player.facing);
        if (d >= -4.0f && d < best)
        {
            best = d;
            target = &foe;
        }
    }
    shotTo_ = player.x + static_cast<float>(player.facing) * (target ? best : ShotRange);
    if (target) damage(game, *target, ShotDamage, player.facing);
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void Fight::update(Game& game, float dt)
{
    if (!active_) return;
    Actor& player = game.player();
    const RoomDef& room = game.room();
    const Input& input = game.input();

    swingCooldown_ -= dt;
    shotCooldown_ -= dt;
    dodgeCooldown_ -= dt;
    invulnerable_ -= dt;
    shotFlash_ -= dt;
    noAmmo_ -= dt;
    holding_ -= dt;
    banner_ -= dt;
    hurtFlash_ = std::max(0.0f, hurtFlash_ - dt * 1.6f);

    // Y is down: the creatures scatter, then the "lose" conversation.
    if (downTime_ >= 0.0f)
    {
        downTime_ += dt;
        for (Foe& foe : foes_)
        {
            foe.flash -= dt;
            const float dir = foe.x < player.x ? -1.0f : 1.0f;
            foe.x += dir * params(foe).speed * 1.4f * dt;
            foe.alpha = std::max(0.0f, foe.alpha - dt * 0.8f);
        }
        if (downTime_ >= DownTime) finish(game, false);
        return;
    }

    // Y's moves. Not while dragging X: the hands are full.
    const bool dragging = game.companionMode() == CompanionMode::Drag;
    if (input.pressed(Action::Attack) || input.pressed(Action::Shoot))
    {
        if (dragging)
        {
            holding_ = 1.5f;
        }
        else if (input.pressed(Action::Attack) && swingCooldown_ <= 0.0f && game.state().hasItem("bat"))
        {
            swingCooldown_ = SwingCooldown;
            swingTime_ = 0.0f;
            game.sound("swing", 0.7f, game.random().uniform(0.9f, 1.1f));
        }
        else if (input.pressed(Action::Shoot) && shotCooldown_ <= 0.0f)
        {
            shoot(game);
        }
    }
    if (input.pressed(Action::Dodge) && dodgeCooldown_ <= 0.0f && !dragging)
    {
        dodgeCooldown_ = DodgeCooldown;
        dodgeTime_ = DodgeTime;
        dodgeDir_ = -player.facing;
        invulnerable_ = std::max(invulnerable_, DodgeSafe);
    }
    if (dodgeTime_ > 0.0f)
    {
        const float step = std::min(dt, dodgeTime_);
        dodgeTime_ -= step;
        const float nx = std::clamp(player.x + static_cast<float>(dodgeDir_) * DodgeDistance / DodgeTime * step,
                                    room.walkMin, room.walkMax);
        player.x = nx; // a dash, not a walk: Y keeps facing the creature
    }
    if (swingTime_ >= 0.0f)
    {
        const float before = swingTime_;
        swingTime_ += dt;
        if (before < SwingActive && swingTime_ >= SwingActive) swing(game);
        if (swingTime_ > SwingShow) swingTime_ = -1.0f;
    }

    // The creatures.
    for (Foe& foe : foes_)
    {
        const Params p = params(foe);
        foe.time += dt;
        foe.flash -= dt;
        const float target = targetX(game, foe);
        if (foe.forX && !xTargetable(game)) foe.forX = false; // he is out of reach now

        switch (foe.state)
        {
        case State::Enter:
            foe.delay -= dt;
            if (foe.delay <= 0.0f)
            {
                setState(foe, State::Approach);
                game.sound("crawler", 0.5f, game.random().uniform(0.85f, 1.15f));
            }
            break;
        case State::Approach:
        {
            const float dir = side(foe.x, target);
            foe.facing = static_cast<int>(dir);
            if (std::fabs(target - foe.x) <= p.lungeRange) setState(foe, State::Windup);
            else foe.x += dir * p.speed * dt;
            break;
        }
        case State::Windup:
            foe.facing = static_cast<int>(side(foe.x, target));
            if (foe.time >= p.windup) setState(foe, State::Lunge);
            break;
        case State::Lunge:
        {
            const float t = std::min(1.0f, foe.time / p.lungeTime);
            foe.x = foe.lungeFrom + static_cast<float>(foe.facing) * p.lungeLength * t;
            if (!foe.hit && std::fabs(target - foe.x) <= p.reach)
            {
                foe.hit = true;
                if (foe.forX && xTargetable(game))
                {
                    if (!xHitThisWave_) hurtCompanion(game, foe);
                    else foe.forX = false;
                }
                else if (invulnerable_ <= 0.0f)
                {
                    hurtPlayer(game, p.damage, foe.facing);
                    if (downTime_ >= 0.0f) return;
                }
            }
            if (foe.state == State::Lunge && t >= 1.0f) setState(foe, State::Recover);
            break;
        }
        case State::Recover:
            if (foe.time >= p.recover) setState(foe, State::Approach);
            break;
        case State::Stagger:
            if (foe.time >= (foe.kind == Kind::Brute ? 0.25f : 0.35f)) setState(foe, State::Approach);
            break;
        case State::Flee:
        {
            const float away = foe.x < player.x ? -1.0f : 1.0f;
            foe.facing = static_cast<int>(away);
            foe.x += away * p.speed * 1.5f * dt;
            foe.alpha = std::max(0.0f, foe.alpha - dt * (foe.kind == Kind::Brute ? 0.5f : 0.9f));
            break;
        }
        case State::Dead: foe.alpha = std::max(0.0f, foe.alpha - dt * 2.5f); break;
        }
    }

    // Gone: faded out or off the scene.
    std::erase_if(foes_, [](const Foe& foe) {
        const bool off = foe.x < -40.0f || foe.x > layout::SceneBox.width + 40.0f;
        return foe.state != State::Enter && (foe.alpha <= 0.0f || (foe.state == State::Flee && off));
    });

    // The next wave, the brute, or the end.
    if (foes_.empty())
    {
        nextWave_ += dt;
        if (wave_ < def_->waves.size())
        {
            if (nextWave_ >= WavePause)
            {
                nextWave_ = 0.0f;
                banner_ = 1.6f;
                spawnWave(game);
            }
        }
        else if (def_->brute && !bruteSent_)
        {
            if (nextWave_ >= 1.0f)
            {
                nextWave_ = 0.0f;
                spawnBrute(game);
            }
        }
        else if (nextWave_ >= 0.8f)
        {
            finish(game, true);
        }
    }
}

std::optional<Fight::Threat> Fight::nearest(float x) const
{
    std::optional<Threat> best;
    float distance = 1e9f;
    for (const Foe& foe : foes_)
    {
        if (foe.state == State::Dead || foe.state == State::Flee || foe.state == State::Enter) continue;
        const float d = std::fabs(foe.x - x);
        if (d < distance)
        {
            distance = d;
            best = Threat{foe.x, foe.state == State::Windup || foe.state == State::Lunge, params(foe).lungeRange};
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void Fight::drawBack(Game& game) const
{
    if (!active_) return;
    const UiConfig& ui = game.ui();
    const float t = static_cast<float>(GetTime());
    const Color body = multiply(creature_ ? creature_->color : Color{11, 13, 17, 255}, game.tint());

    for (const Foe& foe : foes_)
    {
        if (foe.state == State::Enter) continue;
        const Rectangle r = bounds(foe);
        const Color c = ColorAlpha(foe.flash > 0.0f ? ui.highlight : body, foe.alpha);
        const Color eye = ColorAlpha(ui.highlight, foe.alpha);
        const float headX = foe.facing > 0 ? r.x + r.width - 5 : r.x + 1;

        if (foe.kind == Kind::Crawler)
        {
            // Low and long, a hump at the back, legs that skitter.
            const bool moving = foe.state == State::Approach || foe.state == State::Flee || foe.state == State::Lunge;
            const float bob = moving && std::fmod(t * 9.0f + foe.x * 0.1f, 1.0f) < 0.5f ? 1.0f : 0.0f;
            DrawRectangleRec(Rectangle{r.x, r.y + 3 - bob, r.width, r.height - 5}, c);
            DrawRectangleRec(Rectangle{foe.facing > 0 ? r.x + 2 : r.x + r.width - 8, r.y - bob, 6, 4}, c);
            // A faint rim along the back, so it shows against the dark.
            const Color rim = ColorAlpha(ui.dim, 0.7f * foe.alpha);
            DrawRectangleRec(Rectangle{foe.facing > 0 ? r.x + 2 : r.x + r.width - 8, r.y - bob, 6, 1}, rim);
            DrawRectangleRec(Rectangle{r.x, r.y + 3 - bob, r.width, 1}, rim);
            for (int i = 0; i < 3; ++i)
            {
                const float lx = r.x + 2 + static_cast<float>(i) * 5;
                const float lift = moving && (i + static_cast<int>(t * 9.0f)) % 2 == 0 ? 1.0f : 0.0f;
                DrawRectangleRec(Rectangle{lx, r.y + r.height - 2, 1, 2 - lift}, c);
            }
            DrawRectangleRec(Rectangle{headX + (foe.facing > 0 ? 2 : 0), r.y + 4 - bob, 1, 1}, eye);
        }
        else
        {
            // The room's creature: a hunched mass, eyes on Y.
            const float shoulder = std::round(r.height * 0.3f);
            const float lean = foe.state == State::Windup ? static_cast<float>(-foe.facing) * 2.0f
                             : foe.state == State::Lunge  ? static_cast<float>(foe.facing) * 3.0f
                                                          : 0.0f;
            DrawRectangleRec(Rectangle{r.x, r.y + shoulder, r.width, r.height - shoulder}, c);
            const float headW = std::round(r.width * 0.45f);
            const float hx = (foe.facing > 0 ? r.x + r.width - headW - 2 : r.x + 2) + lean;
            DrawRectangleRec(Rectangle{hx, r.y + std::round(shoulder * 0.35f), headW, shoulder}, c);
            const float ey = r.y + std::round(shoulder * 0.35f) + 4;
            DrawRectangleRec(Rectangle{hx + headW / 2 - 3, ey, 2, 1}, eye);
            DrawRectangleRec(Rectangle{hx + headW / 2 + 1, ey, 2, 1}, eye);

            // Its strength, while it stands.
            if (foe.state != State::Flee)
                for (int i = 0; i < foe.maxHp; ++i)
                    DrawRectangleRec(Rectangle{r.x + r.width / 2 - static_cast<float>(foe.maxHp) * 2 + static_cast<float>(i) * 4,
                                               r.y - 6, 3, 2},
                                     i < foe.hp ? ColorAlpha(ui.highlight, 0.8f) : ColorAlpha(ui.dim, 0.6f));
        }

        // The "!" before a lunge.
        if (foe.state == State::Windup && std::fmod(t, 0.16f) < 0.11f)
        {
            const float mx = std::round(foe.x) - 1;
            const float my = r.y - (foe.kind == Kind::Brute ? 18.0f : 12.0f);
            DrawRectangleRec(Rectangle{mx, my, 2, 6}, ui.highlight);
            DrawRectangleRec(Rectangle{mx, my + 8, 2, 2}, ui.highlight);
        }
    }
}

void Fight::drawFront(Game& game) const
{
    if (!active_) return;
    const UiConfig& ui = game.ui();
    const Actor& player = game.player();
    const Rectangle pr = player.bounds();

    // The bat's arc.
    if (swingTime_ >= 0.0f)
    {
        const float k = std::min(1.0f, swingTime_ / SwingShow);
        const float dir = static_cast<float>(player.facing);
        const float hand = player.facing > 0 ? pr.x + pr.width : pr.x;
        const float y = pr.y + 8 + k * 14.0f;
        const float length = 16.0f;
        const float x0 = player.facing > 0 ? hand : hand - length;
        DrawRectangleRec(Rectangle{std::round(x0), std::round(y), length, 2}, ColorAlpha(Color{122, 106, 86, 255}, 1.0f - k * 0.3f));
        DrawRectangleRec(Rectangle{std::round(hand + dir * (length + 2) - (dir < 0 ? 1 : 0)), std::round(y - 6), 1, 12},
                         ColorAlpha(ui.highlight, 0.5f * (1.0f - k)));
    }
    // The shot.
    if (shotFlash_ > 0.0f)
    {
        const float y = pr.y + 14;
        const float from = player.facing > 0 ? pr.x + pr.width + 2 : pr.x - 2;
        DrawRectangleRec(Rectangle{std::min(from, shotTo_), y, std::fabs(shotTo_ - from), 1}, ColorAlpha(ui.highlight, 0.8f));
        drawGlow(Vector2{from, y}, 10.0f, ui.highlight, 0.9f);
    }
    // Hurt: red at the edges of the scene.
    if (hurtFlash_ > 0.0f)
    {
        const float a = 0.55f * hurtFlash_;
        const float w = layout::SceneBox.width;
        const float h = layout::SceneBox.height;
        for (int i = 0; i < 6; ++i)
        {
            const float f = static_cast<float>(i);
            const Color c = ColorAlpha(Blood, a * (1.0f - f / 6.0f));
            DrawRectangleLinesEx(Rectangle{f * 2, f * 2, w - f * 4, h - f * 4}, 2.0f, c);
        }
    }
}

void Fight::drawHud(Game& game) const
{
    if (!active_) return;
    const UiConfig& ui = game.ui();
    const TextRenderer& text = game.text();
    const Rectangle box = layout::DialogueBox;
    const StoryState& state = game.state();

    std::string keys = "A/D move   F swing   Q dodge";
    if (state.hasItem("pistol")) keys += "   R shoot (" + std::to_string(state.get("ammo")) + ")";
    text.draw(keys, Vector2{box.x + layout::Padding, box.y + box.height - layout::Padding - layout::LineHeight}, ui.dim);

    std::string status;
    if (downTime_ >= 0.0f) status = "...";
    else if (holding_ > 0.0f) status = "Not while holding him. G: let go.";
    else if (noAmmo_ > 0.0f) status = "No rounds left.";
    else if (banner_ > 0.0f) status = "More of them.";
    else if (!state.hasItem("bat") && !state.hasItem("pistol")) status = "Nothing to fight with. Dodge.";
    else
    {
        const std::size_t waves = def_->waves.size() + (def_->brute ? 1 : 0);
        const std::size_t now = std::min(waves, wave_ + (bruteSent_ ? 1 : 0));
        status = waves > 1 ? "Fight   " + std::to_string(now) + "/" + std::to_string(waves) : "Fight";
    }
    text.draw(status, Vector2{box.x + layout::Padding, box.y + layout::Padding}, ui.highlight);
}
