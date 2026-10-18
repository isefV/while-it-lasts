#include <hollow/ui/bark.hpp>
#include <hollow/ui/text_renderer.hpp>
#include <hollow/ui/theme.hpp>

#include <algorithm>
#include <cmath>

namespace hollow
{
    namespace
    {
        constexpr float FadeTime = 0.25f;
        constexpr float MaxWidth = 140.0f;

        float duration(const BarkLine& line)
        {
            return 1.4f + 0.055f * static_cast<float>(utf8Count(line.text));
        }
    } // namespace

    void BarkPlayer::play(std::vector<BarkLine> lines)
    {
        lines_ = std::move(lines);
        index_ = 0;
        time_ = 0.0f;
    }

    void BarkPlayer::stop()
    {
        lines_.clear();
        index_ = 0;
    }

    void BarkPlayer::update(float dt)
    {
        if (!active()) return;
        time_ += dt;
        if (time_ >= duration(lines_[index_]))
        {
            ++index_;
            time_ = 0.0f;
        }
    }

    const BarkLine* BarkPlayer::current() const
    {
        return active() ? &lines_[index_] : nullptr;
    }

    void BarkPlayer::draw(const TextRenderer& text, Vector2 head, Color color) const
    {
        const BarkLine* line = current();
        if (!line) return;

        const float total = duration(*line);
        const float alpha = std::clamp(std::min(time_ / FadeTime, (total - time_) / FadeTime), 0.0f, 1.0f);
        const auto lines = wrapText(line->text, MaxWidth, text.measureFn());

        float y = head.y - 4.0f - static_cast<float>(lines.size()) * 10.0f;
        for (const auto& l : lines)
        {
            const float w = text.measure(l);
            const Rectangle box = layout::SceneBox;
            const float x = std::clamp(std::round(head.x - w / 2.0f), box.x + 2.0f, box.x + box.width - 2.0f - w);
            text.drawShadowed(l, Vector2{x, std::max(box.y + 2.0f, std::round(y))}, ColorAlpha(color, alpha));
            y += 10.0f;
        }
    }
} // namespace hollow
