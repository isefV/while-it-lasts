#pragma once

// The Reaper between the campsite and the summit: it shows up, a little
// closer each time, and on the mountain it follows. Data drives it with the
// command "reaper" (in any "do"):
//
//   reaper show <x> [height] [seconds]   stands at x (height 80: near, 40: far away)
//   reaper hide [seconds]
//   reaper glimpse <x> [height]          there for a moment, with a sting
//   reaper follow <distance>             walks behind them, that far back
//   reaper stop                          gone, and stops following
//
// Following, it keeps pace while the player walks on, creeps closer while
// they stand still, and fades while they look back at it. If it gets too
// close it reaches X: X_CONDITION -1 (once a room), and it falls back. X
// left lying alone (G) is what it walks to, whatever Y does. Every room
// starts without it.

#include <hollow/hollow.hpp>

#include <string>
#include <vector>

class Game;

class Reaper
{
public:
    void reset();
    void command(Game& game, const std::vector<std::string>& args);
    void update(Game& game, float dt);
    void draw(Game& game) const;

    [[nodiscard]] bool following() const { return following_; }

private:
    void place(float x, float height);

    hollow::Apparition shape_;
    float height_ = 80.0f;
    bool following_ = false;
    float distance_ = 120.0f; // how far back it walks
    float gap_ = 120.0f;      // how far back it is now
    bool reached_ = false;    // it reached X in this room
    float glimpse_ = -1.0f;   // seconds into a glimpse
    float targetAlpha_ = 0.0f;
};
