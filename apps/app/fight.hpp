#pragma once

// A real-time fight: the room's "fight" (assets/data/rooms/*.json) started by
// the command "fight". Waves of small creatures ("crawlers") come from the
// sides, then the room's creature itself when "brute" is true.
//
//   F swing the bat, R shoot (a round each), Q dodge back, A/D move.
//
// A creature shows a red "!" before it lunges; a lunge that reaches Y costs
// Y_HEALTH, one that reaches X (not dragged, not behind cover) costs
// X_CONDITION, at most once a wave. When every creature is beaten or gone the
// room's "win" conversation starts; a beaten brute fades where it stands.
// When Y_HEALTH reaches 0, Y goes down and the game's death screen follows.
// Y cannot fight while dragging X: G lets go of him first.

#include <hollow/hollow.hpp>

#include <optional>
#include <string>
#include <vector>

class Game;

class Fight
{
public:
    void start(Game& game);
    void stop();

    [[nodiscard]] bool active() const { return active_; }
    /// Y is on the ground (lost): no walking; then the death screen.
    [[nodiscard]] bool down() const { return active_ && downTime_ >= 0.0f; }

    /// Reads F, R and Q; moves the creatures. Only while the player is free
    /// (no conversation on screen).
    void update(Game& game, float dt);

    /// The creatures, behind the characters; the hit flashes in front.
    void drawBack(Game& game) const;
    void drawFront(Game& game) const;

    /// The keys and the wave, in the dialogue box.
    void drawHud(Game& game) const;

    // For the walkthrough's autopilot.
    struct Threat
    {
        float x = 0.0f;
        bool windup = false; ///< about to lunge
        float reach = 0.0f;  ///< how close it must be to hit
    };
    [[nodiscard]] std::optional<Threat> nearest(float x) const;
    [[nodiscard]] bool canSwing() const { return swingCooldown_ <= 0.0f; }

private:
    enum class Kind
    {
        Crawler,
        Brute,
    };
    enum class State
    {
        Enter,    // walking in from the edge
        Approach, // towards its target
        Windup,   // the "!": about to lunge
        Lunge,
        Recover,  // open to hits
        Stagger,  // knocked back
        Flee,     // beaten (the brute) or done biting X
        Dead,
    };

    struct Foe
    {
        Kind kind = Kind::Crawler;
        State state = State::Enter;
        float x = 0.0f;
        float delay = 0.0f;   // seconds before it enters
        int facing = -1;
        int hp = 2;
        int maxHp = 2;
        float time = 0.0f;    // in the current state
        float flash = 0.0f;   // white after a hit
        float alpha = 1.0f;
        float lungeFrom = 0.0f;
        bool hit = false;     // this lunge already landed
        bool forX = false;    // goes for X instead of Y
    };

    struct Params
    {
        float width, height, speed, lungeRange, lungeLength, lungeTime, windup, recover, reach;
        int damage;
    };
    [[nodiscard]] Params params(const Foe& foe) const;

    void spawnWave(Game& game);
    void spawnBrute(Game& game);
    void setState(Foe& foe, State state);
    void damage(Game& game, Foe& foe, int amount, int fromSide);
    void hurtPlayer(Game& game, int amount, int fromSide);
    void hurtCompanion(Game& game, Foe& foe);
    void swing(Game& game);
    void shoot(Game& game);
    void finish(Game& game, bool won);
    [[nodiscard]] Rectangle bounds(const Foe& foe) const;
    [[nodiscard]] bool xTargetable(Game& game) const;
    [[nodiscard]] float targetX(Game& game, const Foe& foe) const;

    bool active_ = false;
    const hollow::FightDef* def_ = nullptr;
    const hollow::CreatureDef* creature_ = nullptr;
    std::optional<hollow::SpriteSheet> bruteSheet_; // the room creature's art, while it fights
    hollow::Animator bruteFrames_;
    float bruteSpeed_ = 1.0f;
    std::vector<Foe> foes_;
    std::size_t wave_ = 0;    // waves sent so far
    bool bruteSent_ = false;
    float nextWave_ = 0.0f;   // pause between waves
    bool xHitThisWave_ = false;

    float swingCooldown_ = 0.0f;
    float swingTime_ = -1.0f; // the swing on screen
    float swingPose_ = 0.0f;  // Y's swing animation still playing
    int swings_ = 0;          // the two swings alternate
    float shotCooldown_ = 0.0f;
    float shotFlash_ = 0.0f;
    float shotTo_ = 0.0f;     // where the shot ended
    float dodgeCooldown_ = 0.0f;
    float dodgeTime_ = 0.0f;  // dashing back
    int dodgeDir_ = 0;
    float invulnerable_ = 0.0f;
    float hurtFlash_ = 0.0f;  // red at the edges
    float banner_ = 0.0f;     // "More of them" on screen
    float noAmmo_ = 0.0f;     // "No rounds left" on screen
    float holding_ = 0.0f;    // "Not while holding him" on screen
    float downTime_ = -1.0f;  // Y went down: the death screen after a moment
};
