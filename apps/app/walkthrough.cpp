#include "walkthrough.hpp"

#include "game.hpp"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>

using namespace hollow;

namespace
{
    constexpr float CommandTimeout = 60.0f; // seconds of game time
    constexpr float StuckAfter = 1.5f;      // not moving while trying to walk
    constexpr float ReachMargin = 4.0f;     // inside Game's reach of 6
} // namespace

bool Walkthrough::load(const std::string& path, std::string& error)
{
    std::ifstream in(path);
    if (!in)
    {
        error = "cannot open " + path;
        return false;
    }
    file_ = path;
    std::string text;
    for (int line = 1; std::getline(in, text); ++line)
    {
        if (const auto hash = text.find('#'); hash != std::string::npos) text.resize(hash);
        auto words = splitWords(text);
        if (words.empty()) continue;

        Command command;
        command.verb = words[0];
        command.args.assign(words.begin() + 1, words.end());
        command.rest = std::string(trim(std::string_view(text).substr(text.find(words[0]) + words[0].size())));
        command.line = line;

        static const std::vector<std::string> verbs = {"use",  "talk",  "walk",  "choose", "wait",   "expect", "room",
                                                       "inventory", "item", "grab", "fight", "revive", "shot", "ended"};
        if (std::find(verbs.begin(), verbs.end(), command.verb) == verbs.end())
        {
            error = path + ":" + std::to_string(line) + ": unknown command \"" + command.verb + "\"";
            return false;
        }
        commands_.push_back(std::move(command));
    }
    return true;
}

void Walkthrough::next()
{
    ++index_;
    while (index_ < commands_.size() && commands_[index_].verb == "done") ++index_; // taken early, see step()
    time_ = 0.0f;
    stuck_ = 0.0f;
    lastX_ = -1.0f;
    cycles_ = 0;
    started_ = false;
    sawScene_ = false;
}

bool Walkthrough::fail(const std::string& why)
{
    failed_ = true;
    const Command& c = commands_[std::min(index_, commands_.size() - 1)];
    message_ = file_ + ":" + std::to_string(c.line) + ": " + c.verb + " " + c.rest + ": " + why;
    return false;
}

bool Walkthrough::walkTowards(Game& game, float left, float right, const std::string& what, float dt)
{
    Actor& player = game.player();
    if (player.x >= left && player.x <= right) return true;

    game.input_.setVirtualDown(player.x < left ? Action::Right : Action::Left, true);

    // Not moving although walking: something blocks the way.
    if (lastX_ >= 0.0f && std::fabs(player.x - lastX_) < 0.01f && !player.walking())
    {
        stuck_ += dt;
        if (stuck_ > StuckAfter)
            fail("stuck at x = " + std::to_string(static_cast<int>(player.x)) + " on the way to " + what + " in " +
                 game.room().id);
    }
    else
    {
        stuck_ = 0.0f;
    }
    lastX_ = player.x;
    return false;
}

bool Walkthrough::step(Game& game, float dt)
{
    if (failed_) return false;
    Input& input = game.input_;
    input.clearVirtual();
    ++frame_;

    if (index_ >= commands_.size())
    {
        message_ = file_ + ": walkthrough finished";
        return false;
    }
    const Command& command = commands_[index_];
    time_ += dt;
    if (time_ > CommandTimeout) return fail("took longer than a minute");

    const bool ended = game.screen_ == Game::Screen::EndCard;
    if (ended && command.verb != "wait" && command.verb != "expect" && command.verb != "shot" && command.verb != "ended")
        return fail("the game has ended");

    // "wait" and "shot" also run while a conversation is on screen (pauses,
    // the ending), which keeps advancing below.
    if (command.verb == "wait" && time_ >= std::strtof(command.rest.c_str(), nullptr))
    {
        next();
        return true;
    }
    if (command.verb == "shot")
    {
        std::cout << "  shot " << command.rest << '\n';
        if (!game.saveCanvas(command.rest)) return fail("could not save");
        next();
        return true;
    }

    // The "choose" for a choice on screen: this command, or the next one when
    // this one is still running (walking into a trigger that asks something).
    std::size_t chooseAt = commands_.size();
    if (command.verb == "choose") chooseAt = index_;
    else if (index_ + 1 < commands_.size() && commands_[index_ + 1].verb == "choose") chooseAt = index_ + 1;
    const auto takeChoose = [&] {
        if (chooseAt == index_) next();
        else commands_[chooseAt].verb = "done";
    };

    // Conversations move on by themselves.
    if (!ended && game.dialogueActive())
    {
        const DialogueView& view = game.runner_->view();
        // The dialogue box ignores input on the frame a new entry appears, so
        // wait a couple of frames before pressing anything for it.
        if (view.serial != lastSerial_)
        {
            lastSerial_ = view.serial;
            serialAge_ = 0;
        }
        const bool ready = ++serialAge_ > 2;
        const bool letRunOut = chooseAt < commands_.size() && commands_[chooseAt].rest == "timeout";
        if (view.kind == DialogueView::Kind::Choice && letRunOut)
        {
            waitingTimeout_ = true;
            return true;
        }
        if (waitingTimeout_)
        {
            std::cout << "  (timed out)\n";
            waitingTimeout_ = false;
            if (letRunOut) takeChoose();
        }

        if (view.kind == DialogueView::Kind::Line)
        {
            if (ready && frame_ % 2 == 0) input.pressVirtual(Action::Advance); // first press shows all, second goes on
        }
        else if (view.kind == DialogueView::Kind::Choice && ready)
        {
            if (chooseAt < commands_.size())
            {
                const std::string want = commands_[chooseAt].rest;
                for (std::size_t i = 0; i < view.options.size(); ++i)
                {
                    if (view.options[i].starts_with(want))
                    {
                        std::cout << "  choose \"" << view.options[i] << "\"\n";
                        input.pressVirtualNumber(static_cast<int>(i) + 1);
                        takeChoose();
                        return true;
                    }
                }
                return fail("no option starting with \"" + want + "\"");
            }
            std::cout << "  (first option) \"" << view.options.front() << "\"\n";
            input.pressVirtualNumber(1);
        }
        return true;
    }
    if (waitingTimeout_)
    {
        std::cout << "  (timed out)\n";
        waitingTimeout_ = false;
        takeChoose();
        return true;
    }

    // Checks: also while a fight or a revive scene is on, but not halfway
    // through a room change.
    const bool settled = ended || (game.pendingRoom_.empty() && !game.inventoryPanel_.isOpen() && !game.ending_);
    if (command.verb == "expect" && settled)
    {
        if (!started_) std::cout << game.room().id << ": expect " << command.rest << '\n';
        std::string error;
        const auto condition = Condition::parse(command.rest, error);
        if (!condition) return fail(error);
        if (!condition->evaluate(game.state())) return fail("is false");
        next();
        return true;
    }
    if (command.verb == "room" && settled)
    {
        if (!started_) std::cout << game.room().id << ": room " << command.rest << '\n';
        if (game.room().id != command.rest) return fail("the player is in " + game.room().id);
        next();
        return true;
    }

    // A fight or revive that is over ends its command (the next scene may
    // follow at once). A walk or use that ran into one hands over to the
    // "fight", "revive" or "wait" after it.
    if ((command.verb == "fight" && sawScene_ && !game.fight_.active()) ||
        (command.verb == "revive" && sawScene_ && !game.revive_.active()))
    {
        next();
        return true;
    }
    if (game.fight_.active() || game.revive_.active())
    {
        std::size_t after = index_ + 1;
        while (after < commands_.size() && commands_[after].verb == "done") ++after;
        const char* scene = game.fight_.active() ? "fight" : "revive";
        const bool interrupted = command.verb == "walk" || command.verb == "use" || command.verb == "talk" || command.verb == "grab";
        if (command.verb != scene && interrupted && after < commands_.size() &&
            (commands_[after].verb == scene || commands_[after].verb == "wait"))
        {
            std::cout << "  (" << scene << " began)\n";
            index_ = after - 1;
            next();
            return true;
        }
    }

    // The near-death scene: E on the beat (or nothing, "revive slow").
    if (!ended && game.revive_.active() && command.verb != "wait")
    {
        if (command.verb != "revive") return fail("the revive scene is on; the next command must be \"revive\"");
        if (!sawScene_) std::cout << game.room().id << ": revive " << command.rest << '\n';
        sawScene_ = true;
        if (command.rest != "slow" && game.revive_.pressNow()) input.pressVirtual(Action::Advance);
        return true;
    }
    if (command.verb == "revive")
    {
        if (sawScene_) next();
        return true; // waiting for it to begin
    }

    // A fight: the autopilot, or standing still ("fight lose").
    if (!ended && game.fight_.active() && command.verb != "wait")
    {
        if (command.verb != "fight") return fail("a fight is on; the next command must be \"fight\"");
        if (!sawScene_) std::cout << game.room().id << ": fight " << command.rest << '\n';
        sawScene_ = true;
        if (command.rest == "lose") return true;

        Actor& player = game.player();
        if (game.companionMode() == CompanionMode::Drag)
        {
            if (frame_ % 10 == 0) input.pressVirtual(Action::Grab); // hands free first
            return true;
        }
        const auto threat = game.fight_.nearest(player.x);
        if (!threat) return true;
        const float d = threat->x - player.x;
        const int side = d >= 0.0f ? 1 : -1;
        if (side != player.facing)
        {
            input.setVirtualDown(side > 0 ? Action::Right : Action::Left, true); // turn to it
            return true;
        }
        if (std::fabs(d) <= 30.0f)
        {
            if (game.fight_.canSwing()) input.pressVirtual(Action::Attack);
        }
        else if (std::fabs(d) > 34.0f)
        {
            input.setVirtualDown(side > 0 ? Action::Right : Action::Left, true);
        }
        return true;
    }
    if (command.verb == "fight")
    {
        if (sawScene_) next();
        return true; // waiting for it to begin
    }

    // Using an item from the inventory.
    if (command.verb == "item" && !ended)
    {
        const std::string& id = command.args.empty() ? std::string() : command.args[0];
        if (!game.inventoryPanel_.isOpen())
        {
            if (game.busy()) return true;
            std::cout << game.room().id << ": item " << id << '\n';
            input.pressVirtual(Action::Inventory);
            return true;
        }
        const auto entries = game.inventory();
        for (std::size_t i = 0; i < entries.size(); ++i)
        {
            if (entries[i].item->id != id) continue;
            if (entries[i].item->use.empty()) return fail("\"" + id + "\" cannot be used from the inventory");
            game.inventoryPanel_.select(static_cast<int>(i));
            input.pressVirtual(Action::Confirm);
            next();
            return true;
        }
        return fail("\"" + id + "\" is not in the inventory");
    }
    if (!ended && game.inventoryPanel_.isOpen())
    {
        if (frame_ % 20 == 0) input.pressVirtual(Action::Inventory); // close it again
        return true;
    }
    if (!ended && game.busy()) return true;

    // A room change (a chase, a cutscene) ends a walk that was under way.
    if (started_ && game.room().id != startRoom_ &&
        (command.verb == "walk" || command.verb == "use" || command.verb == "talk"))
    {
        std::cout << "  (room changed to " << game.room().id << ")\n";
        next();
        return true;
    }

    if (!started_)
    {
        started_ = true;
        startRoom_ = game.room().id;
        std::cout << game.room().id << ": " << command.verb << " " << command.rest << '\n';
    }

    // Walks next to the thing, presses S until it is the selected one (when
    // several things are in reach), then E: what a player does.
    const auto useWhenSelected = [&](Rectangle rect, bool companion, const ObjectDef* object, const std::string& what) {
        if (!walkTowards(game, rect.x - ReachMargin, rect.x + rect.width + ReachMargin, what, dt)) return false;
        if (game.selectedTarget() == Game::Target{object, companion})
        {
            input.pressVirtual(Action::Interact);
            return true;
        }
        if (++cycles_ > static_cast<int>(game.targetsInReach().size()))
            fail(what + " is in reach but S never selects it");
        else
            input.pressVirtual(Action::Down);
        return false;
    };

    if (command.verb == "use")
    {
        const std::string& id = command.args.empty() ? std::string() : command.args[0];
        const ObjectDef* object = nullptr;
        for (const auto& o : game.room().objects)
            if (o.id == id) object = &o;
        if (!object) return fail("no object \"" + id + "\" in " + game.room().id);
        if (!game.present(*object)) return fail("\"" + id + "\" is not there now (its \"if\" is false)");
        if (!game.usable(*object)) return fail("\"" + id + "\" cannot be used now");

        if (useWhenSelected(object->rect, false, object, id)) next();
        return !failed_;
    }
    if (command.verb == "talk")
    {
        if (!game.companionUsable()) return fail("cannot talk to X now");
        if (useWhenSelected(game.companion().bounds(), true, nullptr, "X")) next();
        return !failed_;
    }
    if (command.verb == "walk")
    {
        const float x = std::strtof(command.rest.c_str(), nullptr);
        if (walkTowards(game, x - 1.5f, x + 1.5f, "x = " + command.rest, dt)) next();
        return !failed_;
    }
    if (command.verb == "choose")
    {
        return true; // waiting for the choice to appear
    }
    if (command.verb == "wait")
    {
        if (time_ >= std::strtof(command.rest.c_str(), nullptr)) next();
        return true;
    }
    if (command.verb == "inventory")
    {
        input.pressVirtual(Action::Inventory);
        next();
        return true;
    }
    if (command.verb == "grab")
    {
        input.pressVirtual(Action::Grab);
        next();
        return true;
    }
    if (command.verb == "ended")
    {
        if (!ended) return time_ < 20.0f || fail("the game has not ended"); // the last fade takes a moment
        next();
        return true;
    }
    if (command.verb == "shot")
    {
        if (!game.saveCanvas(command.rest)) return fail("could not save");
        next();
        return true;
    }
    return fail("unknown command");
}
