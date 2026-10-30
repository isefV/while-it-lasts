#pragma once

// The creature of stages 01 to 03: one shape reused in the factory, at the
// checkpoint and on the mountain, with a loop of phases from its room's JSON.

#include <while_it_lasts/while_it_lasts.hpp>

#include <optional>
#include <string>
#include <vector>

class Game;

class Creature
{
public:
    void reset(Game& game, const while_it_lasts::CreatureDef& def);

    void walkTo(float x);
    /// Walks to @p x and fades away.
    void leave(float x);
    void update(float dt);
    void draw(Game& game) const;

    [[nodiscard]] bool walking() const { return target_.has_value(); }
    [[nodiscard]] bool gone() const { return !visible || alpha <= 0.0f; }
    [[nodiscard]] Rectangle bounds() const;

    const while_it_lasts::CreatureDef* def = nullptr;
    float x = 0.0f;
    float y = 150.0f;
    int facing = -1;
    float speedScale = 1.0f;
    float alpha = 1.0f;
    bool visible = true;
    std::string look = "turned"; ///< watch: faces the camera; listen; turned: back to us; alert

private:
    std::optional<float> target_;
    bool leaving_ = false;
    std::optional<while_it_lasts::SpriteSheet> sheet_;
    while_it_lasts::Animator animator_;
};

/// Steps through a creature's phases, round and round.
class PhaseLoop
{
public:
    void reset(const std::vector<while_it_lasts::PatrolPhase>& phases);
    /// Returns true when a new phase began.
    bool update(float dt);

    [[nodiscard]] const while_it_lasts::PatrolPhase* current() const;
    [[nodiscard]] const while_it_lasts::PatrolPhase* next() const;
    [[nodiscard]] float timeLeft() const;

private:
    const std::vector<while_it_lasts::PatrolPhase>* phases_ = nullptr;
    std::size_t index_ = 0;
    float time_ = 0.0f;
};
