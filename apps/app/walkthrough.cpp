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

        static const std::vector<std::string> verbs = {"use", "talk", "walk", "choose", "wait", "expect", "room", "inventory", "shot", "ended"};
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
    started_ = false;
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

    const auto click = [&](Rectangle rect) {
        input.clickVirtual(Vector2{layout::SceneBox.x + rect.x + rect.width / 2.0f,
                                   layout::SceneBox.y + rect.y + rect.height / 2.0f});
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

        if (walkTowards(game, object->rect.x - ReachMargin, object->rect.x + object->rect.width + ReachMargin, id, dt))
        {
            click(object->rect);
            next();
        }
        return !failed_;
    }
    if (command.verb == "talk")
    {
        if (!game.companionUsable()) return fail("cannot talk to X now");
        const Rectangle body = game.companion().bounds();
        if (walkTowards(game, body.x - ReachMargin, body.x + body.width + ReachMargin, "X", dt))
        {
            click(body);
            next();
        }
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
    if (command.verb == "expect")
    {
        std::string error;
        const auto condition = Condition::parse(command.rest, error);
        if (!condition) return fail(error);
        if (!condition->evaluate(game.state())) return fail("is false");
        next();
        return true;
    }
    if (command.verb == "room")
    {
        if (game.room().id != command.rest) return fail("the player is in " + game.room().id);
        next();
        return true;
    }
    if (command.verb == "inventory")
    {
        input.pressVirtual(Action::Inventory);
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
