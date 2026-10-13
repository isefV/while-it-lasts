#include <hollow/hollow.hpp>

#include <gtest/gtest.h>

#include <utility>

TEST(Library, Add)
{
    EXPECT_EQ(hollow::add(2, 3), 5);
    EXPECT_EQ(hollow::add(-2, 2), 0);
}

TEST(Library, Greet)
{
    EXPECT_EQ(hollow::greet("test"), "Hello, test!");
}

TEST(Counter, StartsEmpty)
{
    const hollow::Counter counter;

    EXPECT_EQ(counter.total(), 0);
    EXPECT_EQ(counter.count(), 0);
}

TEST(Counter, SumsValues)
{
    hollow::Counter counter;
    counter.add(4);
    counter.add(-1);
    counter.add(10);

    EXPECT_EQ(counter.total(), 13);
    EXPECT_EQ(counter.count(), 3);
}

TEST(Counter, Moves)
{
    hollow::Counter counter;
    counter.add(7);

    hollow::Counter moved = std::move(counter);

    EXPECT_EQ(moved.total(), 7);
}
