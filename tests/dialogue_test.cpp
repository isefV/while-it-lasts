#include <hollow/data/game_data.hpp>
#include <hollow/system/dialogue_runner.hpp>
#include <hollow/system/story_state.hpp>

#include <gtest/gtest.h>

namespace
{
    // A small dialogue file in the real format.
    constexpr const char* Sample = R"({
        // comments are allowed
        "intro": [
            { "narration": "The fire crackles." },
            { "y": "Hey, {x}." },
            { "thought": "He looks tired." },
            { "if": "cig_count >= 1", "x": "Got a light?" },
            { "choice": [
                { "text": "Take it.", "do": ["cig_count += 1", "sound lighter"], "next": "after" },
                { "text": "Secret.", "if": "noisy_entry" },
                { "text": "Leave it." }
            ] },
            { "x": "Suit yourself." }
        ],
        "after": [
            { "if": "cig_count >= 1",
              "then": [ { "y": "Later. Maybe." } ],
              "else": [ { "y": "Never." } ] },
            { "pause": 0.5 },
            { "goto": "end" }
        ],
        "end": [
            { "do": ["event done"] },
            { "x": "Bye." }
        ],
        "timed": [
            { "choice": [
                { "text": "Run!", "next": "end" },
                { "timeout": true, "do": ["x_condition -= 1"] }
            ], "timer": 2 },
            { "y": "Too slow." }
        ]
    })";

    struct Fixture
    {
        hollow::ConversationMap conversations;
        hollow::StoryState state;
        std::vector<std::string> commands;
        std::unique_ptr<hollow::DialogueRunner> runner;

        Fixture()
        {
            hollow::Diagnostics diag;
            hollow::parseDialogueText(Sample, "sample.json", conversations, diag);
            EXPECT_TRUE(diag.ok()) << (diag.errors.empty() ? "" : diag.errors.front());

            state.define("cig_count", 0, 0, 3);
            state.define("noisy_entry", 0, 0, 1);
            state.define("x_condition", 3, 0, 3);
            runner = std::make_unique<hollow::DialogueRunner>(conversations, state, [this](const hollow::Effect& e) {
                commands.push_back(e.name + (e.args.empty() ? "" : " " + e.args[0]));
            });
            runner->setTokens({{"x", "Sam"}, {"y", "Ash"}});
        }

        const hollow::DialogueView& view() const { return runner->view(); }
    };
} // namespace

TEST(Dialogue, PlaysLinesWithStylesAndNames)
{
    Fixture f;
    ASSERT_TRUE(f.runner->start("intro"));
    EXPECT_EQ(f.view().style, hollow::LineStyle::Narration);
    f.runner->advance();
    EXPECT_EQ(f.view().speaker, "y");
    EXPECT_EQ(f.view().text, "Hey, Sam.");
    f.runner->advance();
    EXPECT_EQ(f.view().style, hollow::LineStyle::Thought);
    f.runner->advance();

    // The conditional line is skipped; the hidden option is not shown.
    ASSERT_EQ(f.view().kind, hollow::DialogueView::Kind::Choice);
    EXPECT_EQ(f.view().options, (std::vector<std::string>{"Take it.", "Leave it."}));
}

TEST(Dialogue, ChoiceWithNextAppliesEffectsAndJumps)
{
    Fixture f;
    f.runner->start("intro");
    for (int i = 0; i < 3; ++i) f.runner->advance();
    f.runner->choose(0);

    EXPECT_EQ(f.state.get("cig_count"), 1);
    EXPECT_EQ(f.view().text, "Later. Maybe.");
    f.runner->advance();
    EXPECT_EQ(f.view().kind, hollow::DialogueView::Kind::Pause);
    f.runner->update(0.6f);
    EXPECT_EQ(f.view().text, "Bye.");
    EXPECT_EQ(f.commands, (std::vector<std::string>{"sound lighter", "event done"}));
    f.runner->advance();
    EXPECT_FALSE(f.runner->active());
    EXPECT_EQ(f.state.seen("after"), 1);
}

TEST(Dialogue, ChoiceWithoutNextContinuesBelow)
{
    Fixture f;
    f.runner->start("intro");
    for (int i = 0; i < 3; ++i) f.runner->advance();
    f.runner->choose(1); // "Leave it."
    EXPECT_EQ(f.view().text, "Suit yourself.");
}

TEST(Dialogue, TimedChoiceTakesTheTimeoutOption)
{
    Fixture f;
    f.runner->start("timed");
    EXPECT_EQ(f.view().options.size(), 1u); // the timeout option has no text
    f.runner->update(1.0f);
    EXPECT_EQ(f.view().kind, hollow::DialogueView::Kind::Choice);
    f.runner->update(1.5f);
    EXPECT_EQ(f.state.get("x_condition"), 2);
    EXPECT_EQ(f.view().text, "Too slow.");
}

TEST(Dialogue, ReportsMistakesWithTheirPlace)
{
    hollow::ConversationMap out;
    hollow::Diagnostics diag;
    hollow::parseDialogueText(R"({ "a": [ { "y": "hi", "x": "both?" }, { "if": "a = 1", "y": "x" } ] })", "bad.json",
                              out, diag);
    ASSERT_EQ(diag.errors.size(), 2u);
    EXPECT_NE(diag.errors[0].find("bad.json: a[0]"), std::string::npos);
    EXPECT_NE(diag.errors[1].find("bad.json: a[1].if"), std::string::npos);

    hollow::Diagnostics syntax;
    hollow::parseDialogueText("{\n  \"a\": [\n    { \"y\": \"hi\" },,\n  ]\n}", "broken.json", out, syntax);
    ASSERT_EQ(syntax.errors.size(), 1u);
    EXPECT_EQ(syntax.errors[0].rfind("broken.json:3:", 0), 0u) << syntax.errors[0];
}
