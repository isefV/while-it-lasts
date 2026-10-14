// The game's real data (assets/data/): it loads without mistakes, and key
// branches of the story play the lines they should.

#include <hollow/data/game_data.hpp>
#include <hollow/system/dialogue_runner.hpp>
#include <hollow/system/story_state.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <deque>

namespace
{
    const hollow::GameData& data()
    {
        static const hollow::GameData loaded = [] {
            hollow::Diagnostics diag;
            auto d = hollow::loadGameData(HOLLOW_TEST_ASSETS, diag);
            for (const auto& e : diag.errors)
                ADD_FAILURE() << e;
            return d;
        }();
        return loaded;
    }

    struct Story
    {
        hollow::StoryState state;
        std::vector<std::string> lines;    // every line shown, names replaced
        std::vector<std::string> commands; // "verb arg..."

        Story()
        {
            for (const auto& v : data().variables)
                state.define(v.name, v.value, v.min, v.max);
            for (const auto& item : data().startItems)
                state.give(item);
        }

        bool saw(std::string_view text) const
        {
            return std::any_of(lines.begin(), lines.end(), [&](const std::string& l) { return l.find(text) != std::string::npos; });
        }
        bool ran(std::string_view command) const
        {
            return std::find(commands.begin(), commands.end(), command) != commands.end();
        }

        /// Plays a conversation to its end. At each choice it picks the first
        /// option starting with the next of @p picks (the first option when
        /// none are left).
        void play(const std::string& id, std::deque<std::string> picks = {})
        {
            hollow::DialogueRunner runner(data().conversations, state, [this](const hollow::Effect& e) {
                std::string text = e.name;
                for (const auto& a : e.args) text += " " + a;
                commands.push_back(text);
            });
            runner.setTokens(data().tokens());
            ASSERT_TRUE(runner.start(id)) << id;

            for (int steps = 0; runner.active() && steps < 1000; ++steps)
            {
                const auto& view = runner.view();
                switch (view.kind)
                {
                case hollow::DialogueView::Kind::Line:
                    lines.push_back(view.text);
                    runner.advance();
                    break;
                case hollow::DialogueView::Kind::Pause: runner.update(view.timeLeft + 0.01f); break;
                case hollow::DialogueView::Kind::Choice:
                {
                    int index = 0;
                    if (!picks.empty())
                    {
                        const auto it = std::find_if(view.options.begin(), view.options.end(),
                                                     [&](const std::string& o) { return o.starts_with(picks.front()); });
                        ASSERT_NE(it, view.options.end()) << "no option \"" << picks.front() << "\" in " << id;
                        index = static_cast<int>(it - view.options.begin());
                        picks.pop_front();
                    }
                    runner.choose(index);
                    break;
                }
                case hollow::DialogueView::Kind::None: break;
                }
            }
            EXPECT_FALSE(runner.active()) << id << " did not end";
        }
    };
} // namespace

TEST(Story, DataLoadsAndChecks)
{
    const std::set<std::string, std::less<>> commands = {"goto", "sound", "ambient", "music", "pose", "companion", "place",
                                                         "step", "face", "shake", "tint", "event", "end", "bark"};
    hollow::Diagnostics diag;
    hollow::checkGameData(data(), commands, diag);
    for (const auto& e : diag.errors)
        ADD_FAILURE() << e;
    for (const auto& w : diag.warnings)
        ADD_FAILURE() << "warning: " << w;
    EXPECT_EQ(data().rooms.size(), 13u);
}

TEST(Story, CampsiteNeedsTheBandageThenEndsWithTheFireOut)
{
    Story s;
    s.play("campsite.x");
    EXPECT_TRUE(s.saw("Let me find the kit"));
    EXPECT_EQ(s.state.get("x_woken"), 1);
    EXPECT_EQ(s.state.get("wound_dressed"), 0);

    s.play("campsite.kit");
    EXPECT_TRUE(s.state.hasItem("last_bandage"));
    s.play("campsite.x", {"Apply"});
    EXPECT_FALSE(s.state.hasItem("last_bandage"));
    EXPECT_EQ(s.state.get("wound_dressed"), 1);
    EXPECT_EQ(s.state.get("fire_out"), 1);
    EXPECT_TRUE(s.saw("very sad deer"));
    EXPECT_TRUE(s.ran("companion follow"));
    EXPECT_TRUE(s.ran("event escape_start"));
}

TEST(Story, FactoryAntibioticsNeedTheShelfThenTheSink)
{
    Story s;
    s.play("office.sink");
    EXPECT_EQ(s.state.get("has_antibiotics"), 0);
    s.play("office.shelves");
    s.play("office.sink");
    EXPECT_EQ(s.state.get("has_antibiotics"), 1);

    s.state.set("x_condition", 2);
    s.state.set("factory_cleared", 1);
    s.play("factory.return");
    EXPECT_EQ(s.state.get("x_condition"), 3);
    EXPECT_TRUE(s.saw("Dealer, doctor, hero"));
    EXPECT_EQ(s.state.get("stage01_done"), 1);
}

TEST(Story, NoisyCabinetMakesTheSneakFail)
{
    Story s;
    s.play("office.cabinet", {"Force it"});
    EXPECT_EQ(s.state.get("noisy_entry"), 1);
    EXPECT_EQ(s.state.get("cig_count"), 1);

    s.play("floor.encounter", {"Sneak"});
    EXPECT_EQ(s.state.get("factory_lost"), 1);
    EXPECT_EQ(s.state.get("factory_won"), 0);
    EXPECT_TRUE(s.ran("event creature_guard 262"));
}

TEST(Story, CheckpointSecondSpottingCostsCondition)
{
    Story s;
    s.state.set("spotted_count", 1);
    s.play("checkpoint.spotted");
    EXPECT_EQ(s.state.get("x_condition"), 3);
    s.state.set("spotted_count", 2);
    s.play("checkpoint.spotted");
    EXPECT_EQ(s.state.get("x_condition"), 2);
    EXPECT_TRUE(s.ran("event stealth_reset"));
}

TEST(Story, RestLinesPlayInOrder)
{
    Story s;
    s.play("mountain.rest");
    s.play("mountain.rest");
    s.play("mountain.rest");
    s.play("mountain.rest");
    const auto at = [&](std::string_view text) {
        return std::find_if(s.lines.begin(), s.lines.end(), [&](const std::string& l) { return l.find(text) != std::string::npos; }) -
               s.lines.begin();
    };
    EXPECT_LT(at("my own knees"), at("under ten minutes"));
    EXPECT_LT(at("under ten minutes"), at("For not leaving me"));
    EXPECT_LT(at("For not leaving me"), at("Is it still far"));
    EXPECT_EQ(s.state.get("rests"), 4);
}

TEST(Story, SummitWithASavedCigaretteAndTwoShared)
{
    Story s;
    s.state.set("cig_count", 1);
    s.state.set("smoked_with_x", 2);
    s.play("summit.cigarette_scene");
    EXPECT_TRUE(s.saw("I kept one."));
    EXPECT_FALSE(s.saw("I was saving that."));
    EXPECT_TRUE(s.saw("Thanks for all the cigarettes."));
    EXPECT_EQ(s.state.get("cig_count"), 0);
    EXPECT_EQ(s.state.get("cig_lit"), 1);
}

TEST(Story, SummitWithNoCigaretteFindsXs)
{
    Story s;
    s.play("summit.cigarette_scene");
    EXPECT_TRUE(s.saw("I was saving that."));
    EXPECT_FALSE(s.saw("Thanks for all the cigarettes."));
}

TEST(Story, NotesUnlockTheSummitLine)
{
    Story s;
    s.play("summit.ground");
    EXPECT_FALSE(s.saw("Don't let them follow you home"));
    s.play("office.filing");
    s.play("office.notice");
    s.play("summit.ground");
    EXPECT_TRUE(s.saw("Maybe it just wanted to see the view."));
}

TEST(Story, ReaperSceneEndsTheGame)
{
    Story s;
    s.play("summit.reaper");
    EXPECT_TRUE(s.ran("event reaper_appear"));
    EXPECT_TRUE(s.ran("event spirit_rise"));
    EXPECT_EQ(s.commands.back(), "end");
}

TEST(Story, NamesComeFromGameJson)
{
    Story s;
    s.play("campsite.hurry");
    EXPECT_TRUE(s.saw(data().characters.at("y").name + ". Now. Please."));
    for (const auto& line : s.lines)
        EXPECT_EQ(line.find('{'), std::string::npos) << line;
}
