#include <while_it_lasts/utility/random.hpp>
#include <while_it_lasts/utility/text.hpp>

#include <gtest/gtest.h>

namespace
{
    // Every character is 6 pixels wide.
    float measure(std::string_view text)
    {
        return 6.0f * static_cast<float>(while_it_lasts::utf8Count(text));
    }
} // namespace

TEST(Text, ReplacesTokens)
{
    EXPECT_EQ(while_it_lasts::replaceTokens("{y}: hey, {x}. {unknown}", {{"x", "Sam"}, {"y", "Ash"}}), "Ash: hey, Sam. {unknown}");
}

TEST(Text, WrapsAtWords)
{
    const auto lines = while_it_lasts::wrapText("one two three four", 60.0f, measure);
    EXPECT_EQ(lines, (std::vector<std::string>{"one two", "three four"}));
}

TEST(Text, WrapsWithFirstLineIndentAndBreaks)
{
    const auto lines = while_it_lasts::wrapText("aa bb cc\ndd", 36.0f, measure, 18.0f);
    EXPECT_EQ(lines, (std::vector<std::string>{"aa", "bb cc", "dd"}));
}

TEST(Text, CutsWordsLongerThanALine)
{
    const auto lines = while_it_lasts::wrapText("abcdefghij", 24.0f, measure);
    EXPECT_EQ(lines, (std::vector<std::string>{"abcd", "efgh", "ij"}));
}

TEST(Text, CountsUtf8)
{
    EXPECT_EQ(while_it_lasts::utf8Count("caf\xC3\xA9"), 4u);
    EXPECT_EQ(while_it_lasts::utf8Prefix("caf\xC3\xA9!", 4), "caf\xC3\xA9");
}

TEST(Random, NeverRepeatsBackToBack)
{
    while_it_lasts::Random random(7);
    while_it_lasts::NoRepeatPicker picker;
    int last = -1;
    for (int i = 0; i < 200; ++i)
    {
        const int pick = picker.pick(3, random);
        ASSERT_GE(pick, 0);
        ASSERT_LT(pick, 3);
        ASSERT_NE(pick, last);
        last = pick;
    }
    EXPECT_EQ(picker.pick(1, random), 0);
}
