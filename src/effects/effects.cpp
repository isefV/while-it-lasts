#include <hollow/effects/effects.hpp>
#include <hollow/utility/math.hpp>

#include <algorithm>
#include <cmath>

namespace hollow
{
    void Fade::to(float target, float seconds)
    {
        target_ = std::clamp(target, 0.0f, 1.0f);
        if (seconds <= 0.0f)
        {
            value_ = target_;
            return;
        }
        speed_ = std::max(0.0001f, std::fabs(target_ - value_) / seconds);
    }

    void Fade::set(float value)
    {
        value_ = target_ = std::clamp(value, 0.0f, 1.0f);
    }

    void Fade::update(float dt)
    {
        value_ = approach(value_, target_, speed_ * dt);
    }

    void Fade::draw(Rectangle area, Color color) const
    {
        if (value_ <= 0.0f) return;
        DrawRectangleRec(area, ColorAlpha(color, value_ * static_cast<float>(color.a) / 255.0f));
    }

    void Shake::start(float seconds, float strength)
    {
        time_ = std::max(time_, seconds);
        strength_ = std::max(strength_, strength);
    }

    void Shake::update(float dt, Random& random)
    {
        if (time_ <= 0.0f)
        {
            offset_ = Vector2{0, 0};
            strength_ = 0.0f;
            return;
        }
        time_ -= dt;
        const int s = static_cast<int>(std::ceil(strength_));
        offset_ = Vector2{static_cast<float>(random.range(-s, s)), static_cast<float>(random.range(-s, s))};
    }

    float Flicker::update(float dt, Random& random)
    {
        time_ += dt;
        jitter_ = approach(jitter_, random.uniform(-0.08f, 0.08f), dt * 0.6f);
        value_ = 1.0f + 0.06f * std::sin(time_ * 7.3f) + 0.04f * std::sin(time_ * 13.1f + 1.7f) + jitter_;
        return value_;
    }

    void drawGlow(Vector2 center, float radius, Color color, float strength)
    {
        if (strength <= 0.0f || radius <= 0.0f) return;
        BeginBlendMode(BLEND_ADDITIVE);
        DrawCircleGradient(center, radius, ColorAlpha(color, 0.35f * strength), ColorAlpha(color, 0.0f));
        DrawCircleGradient(center, radius * 0.45f, ColorAlpha(color, 0.25f * strength), ColorAlpha(color, 0.0f));
        EndBlendMode();
    }

    void Apparition::fadeTo(float alpha, float seconds)
    {
        alpha_.to(alpha, seconds);
    }

    void Apparition::moveTo(Vector2 position, float seconds)
    {
        from_ = Vector2{rect.x, rect.y};
        to_ = position;
        moveLength_ = seconds;
        moveTime_ = 0.0f;
        if (seconds <= 0.0f)
        {
            rect.x = position.x;
            rect.y = position.y;
        }
    }

    void Apparition::update(float dt)
    {
        alpha_.update(dt);
        if (moveLength_ > 0.0f && moveTime_ < moveLength_)
        {
            moveTime_ = std::min(moveTime_ + dt, moveLength_);
            const float t = smoothstep(moveTime_ / moveLength_);
            rect.x = lerp(from_.x, to_.x, t);
            rect.y = lerp(from_.y, to_.y, t);
        }
    }

    void Apparition::draw() const
    {
        const float a = alpha_.value();
        if (a <= 0.0f) return;
        const Rectangle r{std::round(rect.x), std::round(rect.y), rect.width, rect.height};
        if (texture)
        {
            const auto w = static_cast<float>(texture->width);
            const auto h = static_cast<float>(texture->height);
            DrawTexturePro(*texture, Rectangle{0, 0, w, h}, r, Vector2{0, 0}, 0.0f, ColorAlpha(WHITE, a));
            return;
        }
        // Placeholder: a tall shape with a narrower head.
        const Color c = ColorAlpha(color, a * static_cast<float>(color.a) / 255.0f);
        const float headW = std::round(r.width * 0.55f);
        const float headH = std::round(std::min(r.height * 0.18f, 14.0f));
        DrawRectangleRec(Rectangle{r.x + std::round((r.width - headW) / 2.0f), r.y, headW, headH}, c);
        DrawRectangleRec(Rectangle{r.x, r.y + headH, r.width, r.height - headH}, c);
    }

    void Particles::emit(Vector2 position, int count, const Style& style, Random& random)
    {
        for (int i = 0; i < count; ++i)
        {
            particles_.push_back(Particle{
                position,
                Vector2{random.uniform(style.velocityMin.x, style.velocityMax.x),
                        random.uniform(style.velocityMin.y, style.velocityMax.y)},
                style.life * random.uniform(0.6f, 1.0f),
                0.0f,
                style.size,
                style.color,
            });
        }
    }

    void Particles::update(float dt)
    {
        for (auto& p : particles_)
        {
            p.age += dt;
            p.position.x += p.velocity.x * dt;
            p.position.y += p.velocity.y * dt;
            p.velocity.x *= 0.98f;
        }
        std::erase_if(particles_, [](const Particle& p) { return p.age >= p.life; });
    }

    void Particles::draw() const
    {
        for (const auto& p : particles_)
        {
            const float a = 1.0f - p.age / p.life;
            DrawRectangleRec(Rectangle{std::round(p.position.x), std::round(p.position.y), p.size, p.size},
                             ColorAlpha(p.color, a));
        }
    }
} // namespace hollow
