#include <hollow/system/input.hpp>
#include <hollow/ui/dialogue_box.hpp>
#include <hollow/ui/text_renderer.hpp>
#include <hollow/ui/widgets.hpp>

#include <algorithm>
#include <cmath>

namespace hollow
{
    namespace
    {
        constexpr Rectangle Box = layout::DialogueBox;
        constexpr float InnerX = Box.x + layout::Padding;
        constexpr float InnerY = Box.y + layout::Padding;
        constexpr float InnerW = Box.width - 2.0f * layout::Padding;
    } // namespace

    int DialogueBox::linesPerPage() const
    {
        return 4; // 4 lines of 10 px fit inside the 64 px box, 8 px padding
    }

    std::size_t DialogueBox::pageLength() const
    {
        std::size_t total = 0;
        const std::size_t first = page_ * static_cast<std::size_t>(linesPerPage());
        for (std::size_t i = first; i < std::min(lines_.size(), first + linesPerPage()); ++i)
            total += utf8Count(lines_[i]);
        return total;
    }

    void DialogueBox::layoutLine(const DialogueView& view, const Speaker& speaker, const TextRenderer& text)
    {
        prefix_.clear();
        if (view.style == LineStyle::Speech && !speaker.name.empty()) prefix_ = speaker.name + ": ";
        const float indent = prefix_.empty() ? 0.0f : text.measure(prefix_);
        lines_ = wrapText(view.text, InnerW, text.measureFn(), indent);
        prefixColor_ = speaker.nameColor;
        page_ = 0;
        revealed_ = 0.0f;
    }

    Rectangle DialogueBox::optionRect(int index) const
    {
        return Rectangle{InnerX - 2, InnerY + static_cast<float>(index) * layout::LineHeight - 1, InnerW + 4,
                         layout::LineHeight};
    }

    DialogueBox::Result DialogueBox::update(const DialogueView& view, const Speaker& speaker, const Input& input,
                                            float dt, const TextRenderer& text, const UiConfig& ui)
    {
        Result result;
        blink_ += dt;

        if (view.serial != serial_)
        {
            serial_ = view.serial;
            selected_ = 0;
            choice_ = view.kind == DialogueView::Kind::Choice;
            choiceAge_ = 0.0f;
            result.choiceShown = choice_;
            if (view.kind == DialogueView::Kind::Line)
            {
                layoutLine(view, speaker, text);
                switch (view.style)
                {
                case LineStyle::Speech: textColor_ = ui.text; break;
                case LineStyle::Thought: textColor_ = ui.thought; break;
                case LineStyle::Narration: textColor_ = ui.narration; break;
                }
            }
            return result; // a new entry never takes the press that brought it
        }

        if (view.kind == DialogueView::Kind::Line)
        {
            const std::size_t length = pageLength();
            revealed_ = std::min(static_cast<float>(length), revealed_ + ui.textSpeed * dt);

            if (input.pressed(Action::Advance))
            {
                const std::size_t pages =
                    (lines_.size() + static_cast<std::size_t>(linesPerPage()) - 1) / static_cast<std::size_t>(linesPerPage());
                if (revealed_ < static_cast<float>(length))
                    revealed_ = static_cast<float>(length);
                else if (page_ + 1 < pages)
                {
                    ++page_;
                    revealed_ = 0.0f;
                }
                else
                    result.advance = true;
            }
        }
        else if (view.kind == DialogueView::Kind::Choice)
        {
            // Locked for a moment: the keys that were reading lines do nothing.
            choiceAge_ += dt;
            if (choiceLocked()) return result;

            const int count = static_cast<int>(view.options.size());
            if (input.pressed(Action::Up)) selected_ = (selected_ + count - 1) % count;
            if (input.pressed(Action::Down)) selected_ = (selected_ + 1) % count;

            if (input.number() >= 1 && input.number() <= count) result.choice = input.number() - 1;
            if (input.pressed(Action::Confirm)) result.choice = selected_;
        }
        return result;
    }

    void DialogueBox::draw(const DialogueView& view, const TextRenderer& text, const UiConfig& ui, Assets& assets) const
    {
        drawPanel(assets, Box, ui);
        if (view.serial != serial_) return; // not laid out yet (one frame)

        if (view.kind == DialogueView::Kind::Line)
        {
            const std::size_t perPage = static_cast<std::size_t>(linesPerPage());
            const std::size_t first = page_ * perPage;
            auto remaining = static_cast<std::size_t>(revealed_);

            float y = InnerY;
            for (std::size_t i = first; i < std::min(lines_.size(), first + perPage); ++i)
            {
                float x = InnerX;
                if (i == 0 && !prefix_.empty())
                {
                    text.draw(prefix_, Vector2{x, y}, prefixColor_);
                    x += text.measure(prefix_);
                }
                const std::size_t count = std::min(remaining, utf8Count(lines_[i]));
                text.draw(utf8Prefix(lines_[i], count), Vector2{x, y}, textColor_);
                remaining -= count;
                y += layout::LineHeight;
            }

            // Everything shown: a blinking arrow says "press to go on".
            if (static_cast<std::size_t>(revealed_) >= pageLength() && std::fmod(blink_, 1.0f) < 0.6f)
                drawArrow(Vector2{Box.x + Box.width - 12, Box.y + Box.height - 9}, true, ui.dim);
        }
        else if (view.kind == DialogueView::Kind::Choice)
        {
            // A choice arrives: the frame flashes warm and fades, the options
            // come in one after another, and a warm line fills along the top
            // while it is locked. Then the selection lights up.
            const float lock = std::clamp(choiceAge_ / ChoiceLock, 0.0f, 1.0f);
            const bool locked = choiceLocked();
            if (locked)
            {
                DrawRectangleLinesEx(Box, 1.0f, ColorAlpha(ui.accent, 0.9f * (1.0f - lock)));
                DrawRectangleRec(Rectangle{Box.x + 2, Box.y + 2, std::round((Box.width - 4) * lock), 1},
                                 ColorAlpha(ui.accent, 0.8f));
            }
            const float lit = std::clamp((choiceAge_ - ChoiceLock) / 0.15f, 0.0f, 1.0f);

            for (int i = 0; i < static_cast<int>(view.options.size()) && i < layout::MaxOptions; ++i)
            {
                const Rectangle r = optionRect(i);
                const float in = std::clamp((choiceAge_ - 0.12f * static_cast<float>(i)) / 0.25f, 0.0f, 1.0f);
                const float slide = std::round((1.0f - in) * 6.0f);
                const bool active = i == selected_ && !locked;
                if (active)
                {
                    DrawRectangleRec(r, ColorAlpha(ui.frame, 0.35f * lit));
                    drawArrow(Vector2{r.x + 2, r.y + 2}, false, ColorAlpha(ui.highlight, lit));
                }
                const Color color = active ? ui.highlight : locked ? ui.narration : ui.text;
                text.draw(view.options[static_cast<std::size_t>(i)], Vector2{InnerX + 8 + slide, r.y + 1},
                          ColorAlpha(color, in));
            }
            if (view.timer > 0.0f)
            {
                // Time left: a line along the bottom of the box that shrinks.
                const float fraction = std::clamp(view.timeLeft / view.timer, 0.0f, 1.0f);
                const float w = std::round((Box.width - 4) * fraction);
                DrawRectangleRec(Rectangle{Box.x + 2, Box.y + Box.height - 4, w, 1},
                                 fraction < 0.3f ? ui.highlight : ui.dim);
            }
        }
    }
} // namespace hollow
