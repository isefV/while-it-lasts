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
                                                         "step", "face", "shake", "tint", "event", "end", "bark",
                                                         "fight", "revive", "reaper"};
    hollow::Diagnostics diag;
    hollow::checkGameData(data(), commands, diag);
    for (const auto& e : diag.errors)
        ADD_FAILURE() << e;
    for (const auto& w : diag.warnings)
        ADD_FAILURE() << "warning: " << w;
    EXPECT_EQ(data().rooms.size(), 11u); // the foot of the mountain holds the Sheltered Wall and the First Landing
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
    EXPECT_TRUE(s.saw("I might not need this at all"));
    EXPECT_EQ(s.state.get("stage01_done"), 1);
}

TEST(Story, NoisyCabinetMakesTheSneakFail)
{
    Story s;
    s.play("office.cabinet", {"Force it"});
    EXPECT_EQ(s.state.get("noisy_entry"), 1);
    EXPECT_EQ(s.state.get("cig_count"), 1);
    EXPECT_EQ(s.state.get("bandages"), 2);

    // Seen: it charges, and the fight decides.
    s.play("floor.encounter", {"Sneak"});
    EXPECT_TRUE(s.ran("fight"));
    EXPECT_EQ(s.state.get("factory_won"), 0);

    // Won, the creature is gone (losing is death: the level starts again).
    s.play("floor.fight_won");
    EXPECT_EQ(s.state.get("factory_won"), 1);
}

TEST(Story, BandagesHealEitherOfThem)
{
    Story s;
    s.state.set("bandages", 2);
    s.state.set("y_health", 2);
    s.state.set("x_here", 1);
    s.state.set("x_condition", 1);
    s.play("use.bandages", {"Bind"});
    EXPECT_EQ(s.state.get("y_health"), 4);
    s.play("use.bandages", {"Change"});
    EXPECT_EQ(s.state.get("x_condition"), 2);
    EXPECT_EQ(s.state.get("bandages"), 0);

    // X not here and Y unhurt: nothing to do, nothing used.
    s.state.set("bandages", 1);
    s.state.set("y_health", 5);
    s.state.set("x_here", 0);
    s.play("use.bandages");
    EXPECT_EQ(s.state.get("bandages"), 1);
}

TEST(Story, BottlesDistractOnlyAtTheCheckpoint)
{
    Story s;
    s.state.set("bottles", 2);
    s.play("use.bottle");
    EXPECT_EQ(s.state.get("bottles"), 2);
    EXPECT_FALSE(s.ran("event distract"));

    s.state.set("stealth_active", 1);
    s.play("use.bottle", {"Throw"});
    EXPECT_EQ(s.state.get("bottles"), 1);
    EXPECT_TRUE(s.ran("event distract"));
}

TEST(Story, BarricadeWavesEndInTheFirstNearDeath)
{
    Story s;
    s.play("barricade.lever");
    EXPECT_TRUE(s.ran("fight"));
    s.play("barricade.waves_won");
    EXPECT_TRUE(s.ran("revive barricade.revived 7 15"));
    s.play("barricade.revived");
    EXPECT_TRUE(s.saw("Up there. Like we said."));
    EXPECT_EQ(s.state.get("x_down"), 1);
}

TEST(Story, CollapseIsTheWorseNearDeath)
{
    Story s;
    s.play("collapse.fall");
    EXPECT_TRUE(s.ran("revive collapse.revived 10 12"));
    s.play("collapse.revived");
    EXPECT_TRUE(s.saw("That was the deal."));
    EXPECT_TRUE(s.ran("companion drag"));
    EXPECT_EQ(s.state.get("x_down"), 1);
}

TEST(Story, AtTheEndYDoesNotLie)
{
    Story s;
    s.state.set("cig_count", 1);
    s.state.set("smoked_with_x", 2);
    s.play("summit.cigarette_scene");
    s.play("summit.reaper");
    EXPECT_FALSE(s.saw("We still can."));
    EXPECT_FALSE(s.saw("Wake up"));
    EXPECT_FALSE(s.saw("Don't do this"));
    EXPECT_TRUE(s.saw("You didn't even try to lie."));
    EXPECT_TRUE(s.saw("It's a little bit goodbye."));
    EXPECT_TRUE(s.saw("The sun comes up over the town"));
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
