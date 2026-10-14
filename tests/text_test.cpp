#include <hollow/utility/random.hpp>
#include <hollow/utility/text.hpp>

#include <gtest/gtest.h>

namespace
{
    // Every character is 6 pixels wide.
    float measure(std::string_view text)
    {
        return 6.0f * static_cast<float>(hollow::utf8Count(text));
    }
} // namespace

TEST(Text, ReplacesTokens)
{
    EXPECT_EQ(hollow::replaceTokens("{y}: hey, {x}. {unknown}", {{"x", "Sam"}, {"y", "Ash"}}), "Ash: hey, Sam. {unknown}");
}

TEST(Text, WrapsAtWords)
{
    const auto lines = hollow::wrapText("one two three four", 60.0f, measure);
    EXPECT_EQ(lines, (std::vector<std::string>{"one two", "three four"}));
}

TEST(Text, WrapsWithFirstLineIndentAndBreaks)
{
    const auto lines = hollow::wrapText("aa bb cc\ndd", 36.0f, measure, 18.0f);
    EXPECT_EQ(lines, (std::vector<std::string>{"aa", "bb cc", "dd"}));
}

TEST(Text, CutsWordsLongerThanALine)
{
    const auto lines = hollow::wrapText("abcdefghij", 24.0f, measure);
    EXPECT_EQ(lines, (std::vector<std::string>{"abcd", "efgh", "ij"}));
}

TEST(Text, CountsUtf8)
{
    EXPECT_EQ(hollow::utf8Count("caf\xC3\xA9"), 4u);
    EXPECT_EQ(hollow::utf8Prefix("caf\xC3\xA9!", 4), "caf\xC3\xA9");
}

TEST(Random, NeverRepeatsBackToBack)
{
    hollow::Random random(7);
    hollow::NoRepeatPicker picker;
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
