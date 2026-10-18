#include <hollow/objects/actor.hpp>

#include <gtest/gtest.h>

using namespace hollow;

namespace
{
    // A walk of four frames at 10 a second with feet landing on 1 and 3, and
    // a smoking frame whose cigarette tip is at (24, 10).
    SpriteSheet walker()
    {
        SpriteSheet sheet;
        Animation walk;
        walk.frames = 4;
        walk.fps = 10.0f;
        walk.steps = {1, 3};
        sheet.animations["walk"] = walk;
        sheet.animations["idle"] = Animation{};
        Animation smoke;
        smoke.origin = Vector2{16, 47};
        smoke.emit = Vector2{24, 10};
        sheet.animations["smoke"] = smoke;
        return sheet;
    }
} // namespace

TEST(Actor, FootfallsLandOnTheStepFrames)
{
    const SpriteSheet sheet = walker();
    Actor a;
    a.sheet = &sheet;

    // One second of walking: frames 0 1 2 3 0 1 2 3 0 1, five footfalls.
    int falls = 0;
    for (int i = 0; i < 100; ++i)
    {
        a.step(0.5f);
        a.update(0.01f);
        if (a.footfall()) ++falls;
    }
    EXPECT_EQ(falls, 5);

    // Standing still, nothing.
    falls = 0;
    for (int i = 0; i < 50; ++i)
    {
        a.update(0.01f);
        if (a.footfall()) ++falls;
    }
    EXPECT_EQ(falls, 0);
}

TEST(Actor, SmokeRisesFromTheTipFacingEitherWay)
{
    const SpriteSheet sheet = walker();
    Actor a;
    a.sheet = &sheet;
    a.x = 100.0f;
    a.ground = 150.0f;
    a.pose = "smoke";
    a.update(0.01f);

    auto tip = a.emitPoint();
    ASSERT_TRUE(tip.has_value());
    EXPECT_FLOAT_EQ(tip->x, 108.0f); // 24 - 16 to the right of the feet
    EXPECT_FLOAT_EQ(tip->y, 113.0f); // 10 - 47 above them

    a.facing = -1;
    tip = a.emitPoint();
    ASSERT_TRUE(tip.has_value());
    EXPECT_FLOAT_EQ(tip->x, 92.0f); // mirrored

    a.pose = "idle";
    a.update(0.01f);
    EXPECT_FALSE(a.emitPoint().has_value());
}
