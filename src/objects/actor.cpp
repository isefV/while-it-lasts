#include <hollow/objects/actor.hpp>
#include <hollow/utility/color.hpp>
#include <hollow/utility/math.hpp>

#include <cmath>

namespace hollow
{
    void Actor::walkTo(float targetX)
    {
        target_ = targetX;
    }

    void Actor::stop()
    {
        target_.reset();
    }

    float Actor::speed() const
    {
        return (def ? def->speed : 48.0f) * speedScale;
    }

    void Actor::step(float dx)
    {
        if (dx == 0.0f) return;
        x += dx;
        facing = dx > 0.0f ? 1 : -1;
        steppedThisFrame_ = true;
    }

    void Actor::update(float dt)
    {
        moved_ = steppedThisFrame_;
        steppedThisFrame_ = false;

        if (target_)
        {
            const float before = x;
            x = approach(x, *target_, speed() * dt);
            if (x != before)
            {
                facing = x > before ? 1 : -1;
                moved_ = true;
            }
            if (std::fabs(x - *target_) < 0.01f) target_.reset();
        }

        // Dragging keeps its own animation while moving.
        if (moved_)
            animator_.play(pose == "drag" ? "drag" : "walk");
        else
            animator_.play(pose);
        animator_.update(dt, sheet);
    }

    Rectangle Actor::bounds() const
    {
        const float w = def ? def->width : 14.0f;
        const float h = def ? def->height : 40.0f;
        float bw = w;
        float bh = h;
        if (pose == "sit")
        {
            bw = w * 1.3f;
            bh = h * 0.6f;
        }
        else if (pose == "lying")
        {
            bw = h * 0.9f;
            bh = w * 0.7f;
        }
        else if (pose == "kneel")
        {
            bh = h * 0.65f;
        }
        else if (pose == "drag")
        {
            bh = h * 0.85f;
        }
        return Rectangle{std::round(x - bw / 2.0f), std::round(ground - bh), std::round(bw), std::round(bh)};
    }

    void Actor::draw(Color tint) const
    {
        if (!visible) return;
        if (sheet && drawSprite(*sheet, animator_, Vector2{x, ground}, facing < 0, tint)) return;

        // Placeholder: body, a lighter head band, and an eye on the side it faces.
        const Color body = multiply(def ? def->color : Color{150, 160, 170, 255}, tint);
        const Rectangle r = bounds();
        DrawRectangleRec(r, body);

        const bool lying = pose == "lying";
        const Color head = mix(body, Color{230, 235, 240, body.a}, 0.25f);
        if (lying)
        {
            const float hx = facing > 0 ? r.x + r.width - 8 : r.x;
            DrawRectangleRec(Rectangle{hx, r.y, 8, r.height}, head);
        }
        else
        {
            DrawRectangleRec(Rectangle{r.x, r.y, r.width, std::min(9.0f, r.height / 3.0f)}, head);
            const float eyeX = facing > 0 ? r.x + r.width - 3 : r.x + 2;
            DrawRectangleRec(Rectangle{eyeX, r.y + 3, 1, 2}, Color{12, 14, 18, body.a});
        }
    }
} // namespace hollow
