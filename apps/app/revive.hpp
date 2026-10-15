#pragma once

// The near-death scene: X stops breathing. The scene darkens and shakes, the
// Reaper walks in from the edge towards him, and the player keeps him going:
// E (or Space) in time with the pulse, like chest compressions. Enough good
// presses before the Reaper reaches X and he gasps back. If it gets there
// first it touches him (REAPER_TOUCHED +1, X_CONDITION 0), and he still comes
// back: never game over. Either way the conversation named in the command
// plays next:
//
//   revive <conversation> [presses] [seconds]
//
// presses: good presses needed (default 8); seconds: how long the Reaper
// takes to reach him (default 14).

#include <hollow/hollow.hpp>

#include <string>

class Game;

class Revive
{
public:
    void start(Game& game, std::string then, int presses, float seconds);
    void stop() { active_ = false; }

    [[nodiscard]] bool active() const { return active_; }

    void update(Game& game, float dt);

    /// The dark, the shake and the Reaper, over the scene.
    void drawScene(Game& game) const;

    /// The pulse bar, in the dialogue box.
    void drawHud(Game& game) const;

    // For the walkthrough: inside the window, and not pressed yet this beat.
    [[nodiscard]] bool pressNow() const;

private:
    bool active_ = false;
    std::string then_;
    int needed_ = 8;
    int good_ = 0;
    float phase_ = 0.0f;       // 0 to 1 across one beat
    bool pressedThisBeat_ = false;
    float pulse_ = 0.0f;       // a good press: the heart on the bar jumps
    float miss_ = 0.0f;        // a press off the beat
    float reaperX_ = 0.0f;
    float reaperSpeed_ = 0.0f; // scene pixels per second
    int reaperSide_ = 1;       // it comes from this side of X
    float reaperAlpha_ = 0.0f;
    float closeness_ = 0.0f;   // 0 far, 1 at X
    float done_ = -1.0f;       // seconds since the end
    bool touched_ = false;
    Color tintBefore_{255, 255, 255, 255};
};
