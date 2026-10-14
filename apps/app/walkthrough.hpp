#pragma once

// Plays the game from a script of commands, with virtual keys and mouse, so
// a whole playthrough can be checked without a person ("--walkthrough").
// The commands, one per line (# starts a comment):
//
//   use <object>         walk to it (A/D) and click it
//   talk                 walk to X and click him
//   walk <x>             walk until the player stands at x
//   choose <text>        at the next choice, pick the option starting with
//                        <text>; "choose timeout" lets a timed choice run out
//   wait <seconds>       do nothing (timers keep running)
//   expect <condition>   fail unless the condition is true ("cig_count >= 1")
//   room <id>            fail unless the player is in that room
//   inventory            open and close the inventory
//   shot <file.png>      save the canvas
//   ended                fail unless the game reached the end card (waits up to 20 s)
//
// Conversations advance on their own; a choice without a "choose" takes the
// first option. The walkthrough fails when the player is stuck (cannot reach
// what it walks to) or a command takes longer than a minute.

#include <string>
#include <vector>

class Game;

class Walkthrough
{
public:
    bool load(const std::string& path, std::string& error);

    /// Sets this frame's virtual input; call before Game::frame(). Returns
    /// false once the script is finished or has failed.
    bool step(Game& game, float dt);

    [[nodiscard]] bool failed() const { return failed_; }
    [[nodiscard]] const std::string& message() const { return message_; }

private:
    struct Command
    {
        std::string verb;
        std::vector<std::string> args;
        std::string rest; // everything after the verb
        int line = 0;
    };

    bool walkTowards(Game& game, float left, float right, const std::string& what, float dt);
    void next();
    bool fail(const std::string& why);

    std::string file_;
    std::vector<Command> commands_;
    std::size_t index_ = 0;
    float time_ = 0.0f;       // in the current command
    float stuck_ = 0.0f;
    float lastX_ = -1.0f;
    int frame_ = 0;
    unsigned lastSerial_ = ~0u;
    int serialAge_ = 0;
    bool started_ = false;
    std::string startRoom_;
    bool waitingTimeout_ = false;
    bool failed_ = false;
    std::string message_;
};
