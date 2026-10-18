#include <hollow/objects/actor.hpp>
#include <hollow/utility/color.hpp>
#include <hollow/utility/math.hpp>

#include <algorithm>
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

        // Dragging and crouching keep their own animation while moving; a
        // swing of the bat plays out even while stepping.
        // A sheet without a frame for being dragged shows him lying.
        const bool movingPose = pose == "drag" || pose == "crouch";
        if (pose.starts_with("swing"))
            animator_.play(pose);
        else if (moved_)
            animator_.play(movingPose ? pose : "walk");
        else if (pose == "dragged" && sheet && !sheet->animations.contains("dragged"))
            animator_.play("lying");
        else
            animator_.play(pose);
        // A pose the sheet has no frames for shows the nearest it has, never a box.
        if (sheet && !sheet->animations.contains(animator_.current()))
            animator_.play(moved_ && sheet->animations.contains("walk") ? "walk" : "idle");

        // Dragging and crouching only move with the feet: standing still (or
        // in a conversation) holds the frame it stopped on.
        const std::string before = animator_.current();
        const int frameBefore = animator_.frame();
        if (moved_ || !movingPose) animator_.update(dt, sheet);

        footfall_ = false;
        if (moved_ && sheet && (animator_.frame() != frameBefore || animator_.current() != before))
            if (const auto it = sheet->animations.find(animator_.current()); it != sheet->animations.end())
                footfall_ = std::find(it->second.steps.begin(), it->second.steps.end(), animator_.frame()) !=
                            it->second.steps.end();
    }

    std::optional<Vector2> Actor::emitPoint() const
    {
        if (!sheet || !visible) return std::nullopt;
        const auto it = sheet->animations.find(animator_.current());
        if (it == sheet->animations.end() || !it->second.emit) return std::nullopt;
        // The art faces right; facing left mirrors it about the feet.
        const Animation& a = it->second;
        const float dx = a.emit->x - a.origin.x;
        return Vector2{x + (facing < 0 ? -dx : dx), ground + (a.emit->y - a.origin.y)};
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
        else if (pose == "lying" || pose == "dragged")
        {
            bw = h * 1.2f; // as long as the lying frames (48 for a 40 high character)
            bh = w * 0.7f;
        }
        else if (pose == "kneel")
        {
            bh = h * 0.65f;
        }
        else if (pose == "drag" || pose == "crouch")
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

        const bool lying = pose == "lying" || pose == "dragged";
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
