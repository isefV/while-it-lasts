#include <while_it_lasts/system/script.hpp>
#include <while_it_lasts/system/story_state.hpp>

#include <gtest/gtest.h>

namespace
{
    while_it_lasts::StoryState makeState()
    {
        while_it_lasts::StoryState state;
        state.define("x_condition", 3, 0, 3);
        state.define("cig_count", 0, 0, 3);
        state.define("noisy_entry", 0, 0, 1);
        return state;
    }

    bool eval(const char* text, const while_it_lasts::StoryState& state)
    {
        std::string error;
        const auto condition = while_it_lasts::Condition::parse(text, error);
        EXPECT_TRUE(condition.has_value()) << text << ": " << error;
        return condition && condition->evaluate(state);
    }
} // namespace

TEST(Condition, EmptyIsTrue)
{
    const auto state = makeState();
    EXPECT_TRUE(eval("", state));
    EXPECT_TRUE(while_it_lasts::Condition{}.evaluate(state));
}

TEST(Condition, ComparesAndCombines)
{
    auto state = makeState();
    EXPECT_TRUE(eval("x_condition == 3", state));
    EXPECT_TRUE(eval("x_condition >= 2 && !noisy_entry", state));
    EXPECT_FALSE(eval("cig_count >= 1", state));
    EXPECT_TRUE(eval("cig_count >= 1 || x_condition > 2", state));
    EXPECT_TRUE(eval("not (noisy_entry or cig_count != 0)", state));

    state.set("noisy_entry", 1);
    EXPECT_TRUE(eval("noisy_entry", state));
    EXPECT_FALSE(eval("!noisy_entry", state));
}

TEST(Condition, ItemsAndSeen)
{
    auto state = makeState();
    EXPECT_FALSE(eval("item.key", state));
    state.give("key");
    EXPECT_TRUE(eval("item.key", state));

    EXPECT_TRUE(eval("seen.campsite.fire == 0", state));
    state.markSeen("campsite.fire");
    EXPECT_TRUE(eval("seen.campsite.fire > 0", state));
}

TEST(Condition, ReportsMistakes)
{
    std::string error;
    EXPECT_FALSE(while_it_lasts::Condition::parse("x_condition = 3", error));
    EXPECT_NE(error.find("=="), std::string::npos);
    EXPECT_FALSE(while_it_lasts::Condition::parse("(x_condition > 1", error));
    EXPECT_FALSE(while_it_lasts::Condition::parse("x_condition >", error));
    EXPECT_FALSE(while_it_lasts::Condition::parse("a $ b", error));
}

TEST(Condition, ListsItsNames)
{
    std::string error;
    const auto condition = while_it_lasts::Condition::parse("a > 1 && (b || !item.key)", error);
    ASSERT_TRUE(condition);
    EXPECT_EQ(condition->names(), (std::vector<std::string>{"a", "b", "item.key"}));
}

TEST(Effect, ChangesStateAndClamps)
{
    auto state = makeState();
    std::vector<while_it_lasts::Effect> effects;
    std::string error;
    for (const char* text : {"cig_count += 1", "x_condition -= 5", "noisy_entry = true", "give bandage", "goto summit 40"})
    {
        auto effect = while_it_lasts::Effect::parse(text, error);
        ASSERT_TRUE(effect) << text << ": " << error;
        effects.push_back(*effect);
    }

    std::vector<std::string> commands;
    while_it_lasts::applyEffects(effects, state, [&](const while_it_lasts::Effect& e) {
        commands.push_back(e.name + ":" + e.args.at(0) + ":" + e.args.at(1));
    });

    EXPECT_EQ(state.get("cig_count"), 1);
    EXPECT_EQ(state.get("x_condition"), 0); // clamped to its minimum
    EXPECT_EQ(state.get("noisy_entry"), 1);
    EXPECT_TRUE(state.hasItem("bandage"));
    EXPECT_EQ(commands, (std::vector<std::string>{"goto:summit:40"}));
}

TEST(Effect, ReportsMistakes)
{
    std::string error;
    EXPECT_FALSE(while_it_lasts::Effect::parse("cig_count += lots", error));
    EXPECT_FALSE(while_it_lasts::Effect::parse("2bad = 1", error));
    EXPECT_FALSE(while_it_lasts::Effect::parse("give", error));
}

TEST(StoryState, ResetsToStartValues)
{
    auto state = makeState();
    state.set("cig_count", 2);
    state.give("bat");
    state.markSeen("intro");
    state.reset();
    EXPECT_EQ(state.get("cig_count"), 0);
    EXPECT_EQ(state.get("x_condition"), 3);
    EXPECT_TRUE(state.items().empty());
    EXPECT_EQ(state.seen("intro"), 0);
}
