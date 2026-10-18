#include "revive.hpp"

#include "game.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>

using namespace hollow;

namespace
{
    constexpr float Period = 0.85f;     // seconds per beat
    constexpr float WindowWidth = 0.20f; // the part of the beat where E counts
    constexpr float FirstWindow = 0.40f; // the first beat: in the middle
    constexpr float EarliestWindow = 0.25f; // later beats: anywhere from here, so there is time to see it
    constexpr float WindowMove = 0.15f;  // and at least this far from the last place
    constexpr float PushBack = 6.0f;    // a good press holds the Reaper off a little
    constexpr float MissStep = 8.0f;    // a press off the beat lets it closer
    constexpr float ReachGap = 12.0f;   // it touches him at this distance
    constexpr float ReaperW = 33.0f; // the art is 32 x 80
    constexpr float ReaperH = 82.0f;
    const Color Dark{70, 76, 96, 255}; // the scene while he is gone
} // namespace

void Revive::start(Game& game, std::string then, int presses, float seconds)
{
    active_ = true;
    then_ = std::move(then);
    needed_ = std::max(1, presses);
    good_ = 0;
    phase_ = 0.0f;
    windowFrom_ = FirstWindow;
    pressedThisBeat_ = false;
    pulse_ = miss_ = 0.0f;
    done_ = -1.0f;
    touched_ = false;

    // Y kneels at X's side; the Reaper comes from the other side.
    Actor& x = game.companion();
    Actor& y = game.player();
    game.setCompanionMode(CompanionMode::Stay, x.x);
    x.pose = "lying";
    const int ySide = y.x <= x.x ? -1 : 1;
    y.stop();
    y.x = std::clamp(x.x + static_cast<float>(ySide) * 14.0f, game.room().walkMin, game.room().walkMax);
    y.facing = -ySide;
    y.pose = "kneel";

    reaperSide_ = -ySide;
    reaperX_ = reaperSide_ > 0 ? layout::SceneWidth + 16.0f : -16.0f;
    reaperSpeed_ = std::max(1.0f, std::fabs(reaperX_ - x.x) - ReachGap) / std::max(2.0f, seconds);
    reaperAlpha_ = 0.0f;
    closeness_ = 0.0f;

    tintBefore_ = game.tint();
    game.setTint(Dark, 1.2f);
    game.sound("sting");
}

bool Revive::pressNow() const
{
    return active_ && done_ < 0.0f && !pressedThisBeat_ && phase_ >= windowFrom_ + 0.02f &&
           phase_ <= windowFrom_ + WindowWidth - 0.02f;
}

void Revive::update(Game& game, float dt)
{
    if (!active_) return;
    const Actor& x = game.companion();
    reaperFrames_.update(dt, game.reaperSheet());
    pulse_ = std::max(0.0f, pulse_ - dt * 3.0f);
    miss_ = std::max(0.0f, miss_ - dt * 2.5f);

    // Over: he gasps (or it touched him), the Reaper fades, the conversation.
    if (done_ >= 0.0f)
    {
        done_ += dt;
        reaperAlpha_ = std::max(0.0f, reaperAlpha_ - dt * 0.7f);
        reaperX_ += static_cast<float>(reaperSide_) * 10.0f * dt;
        if (done_ >= 1.6f)
        {
            active_ = false;
            game.player().pose = "kneel";
            game.setTint(tintBefore_, 1.5f);
            game.startDialogue(then_);
        }
        return;
    }

    // The beat, and E on it.
    phase_ += dt / Period;
    if (phase_ >= 1.0f)
    {
        phase_ -= 1.0f;
        pressedThisBeat_ = false;

        // Each beat the place to press moves somewhere else on the bar.
        const float before = windowFrom_;
        for (int i = 0; i < 8 && std::fabs(windowFrom_ - before) < WindowMove; ++i)
            windowFrom_ = game.random().uniform(EarliestWindow, 1.0f - WindowWidth);
    }
    const Input& input = game.input();
    if (input.pressed(Action::Advance) || input.pressed(Action::Interact))
    {
        const bool onBeat = phase_ >= windowFrom_ && phase_ <= windowFrom_ + WindowWidth && !pressedThisBeat_;
        pressedThisBeat_ = true;
        if (onBeat)
        {
            ++good_;
            pulse_ = 1.0f;
            game.sound("heartbeat", 0.9f, 0.95f + 0.05f * static_cast<float>(good_ % 3));
            reaperX_ += static_cast<float>(reaperSide_) * PushBack;
        }
        else
        {
            miss_ = 1.0f;
            reaperX_ -= static_cast<float>(reaperSide_) * MissStep;
        }
    }

    // The Reaper walks in.
    reaperAlpha_ = std::min(0.92f, reaperAlpha_ + dt * 0.6f);
    reaperX_ -= static_cast<float>(reaperSide_) * reaperSpeed_ * dt;
    const float gap = std::fabs(reaperX_ - x.x);
    const float start = layout::SceneWidth * 0.5f;
    closeness_ = std::clamp(1.0f - (gap - ReachGap) / start, 0.0f, 1.0f);
    game.shake(0.1f, 0.6f + 2.2f * closeness_);

    if (good_ >= needed_)
    {
        done_ = 0.0f;
        game.sound("gasp");
        game.shake(0.3f, 1.5f);
    }
    else if (gap <= ReachGap)
    {
        done_ = 0.0f;
        touched_ = true;
        game.state().add("reaper_touched", 1);
        game.state().set("x_condition", 0);
        game.sound("sting");
        game.sound("gasp", 0.8f, 0.9f);
        game.shake(0.6f, 3.0f);
    }
}

void Revive::drawScene(Game& game) const
{
    if (!active_) return;
    const float w = layout::SceneWidth;
    const float h = layout::SceneHeight;

    // The edges close in as it comes.
    const float dark = 0.25f + 0.45f * closeness_;
    for (int i = 0; i < 10; ++i)
    {
        const float f = static_cast<float>(i);
        DrawRectangleLinesEx(Rectangle{f * 3, f * 3, w - f * 6, h - f * 6}, 3.0f, ColorAlpha(BLACK, dark * (1.0f - f / 10.0f)));
    }

    // The Reaper.
    if (reaperAlpha_ > 0.0f)
    {
        const Color c = ColorAlpha(Color{6, 6, 10, 255}, reaperAlpha_);
        const Rectangle r{std::round(reaperX_ - ReaperW / 2), layout::GroundY - ReaperH, ReaperW, ReaperH};
        const SpriteSheet* sheet = game.reaperSheet();
        if (!sheet || !drawSpriteIn(*sheet, reaperFrames_, r, reaperSide_ > 0, ColorAlpha(WHITE, reaperAlpha_)))
        {
            const float headW = std::round(r.width * 0.55f);
            DrawRectangleRec(Rectangle{r.x + std::round((r.width - headW) / 2), r.y, headW, 14}, c);
            DrawRectangleRec(Rectangle{r.x, r.y + 14, r.width, r.height - 14}, c);
            // A reaching arm, closer as it nears him.
            const float arm = 6.0f + 14.0f * closeness_;
            const float ax = reaperSide_ > 0 ? r.x - arm : r.x + r.width;
            DrawRectangleRec(Rectangle{ax, r.y + 30, arm, 3}, c);
        }
    }

    // The moment he comes back (or it touches him): a white breath.
    if (done_ >= 0.0f && done_ < 0.5f)
        DrawRectangle(0, 0, static_cast<int>(w), static_cast<int>(h),
                      ColorAlpha(touched_ ? Color{200, 205, 220, 255} : game.ui().highlight, 0.35f * (1.0f - done_ / 0.5f)));
}

void Revive::drawHud(Game& game) const
{
    if (!active_) return;
    const UiConfig& ui = game.ui();
    const TextRenderer& text = game.text();
    const Rectangle box = layout::DialogueBox;

    const std::string title = done_ >= 0.0f ? (touched_ ? "...it touched him." : "He's breathing.")
                                            : "He's not breathing. Press E on the beat.";
    text.draw(title, Vector2{box.x + layout::Padding, box.y + layout::Padding}, done_ >= 0.0f ? ui.text : ui.highlight);

    // The bar: the window (a new place each beat), the beat crossing it.
    const Rectangle bar{box.x + layout::Padding, box.y + 26, 200, 10};
    DrawRectangleLinesEx(bar, 1.0f, ui.frame);
    const Rectangle window{std::round(bar.x + bar.width * windowFrom_), bar.y + 1, std::round(bar.width * WindowWidth), bar.height - 2};
    DrawRectangleRec(window, ColorAlpha(ui.frame, 0.6f + 0.4f * pulse_));
    if (done_ < 0.0f)
    {
        const float mx = std::round(bar.x + bar.width * phase_);
        const float grow = 2.0f * pulse_;
        DrawRectangleRec(Rectangle{mx - 1 - grow / 2, bar.y - 2 - grow / 2, 3 + grow, bar.height + 4 + grow},
                         miss_ > 0.0f ? ColorAlpha(ui.dim, 1.0f) : ui.highlight);
    }

    // How many good presses so far.
    for (int i = 0; i < needed_; ++i)
        DrawRectangleRec(Rectangle{bar.x + bar.width + 10 + static_cast<float>(i) * 5, bar.y + 2, 3, 6},
                         i < good_ ? ui.highlight : ColorAlpha(ui.dim, 0.7f));

    text.draw("E / Space", Vector2{box.x + layout::Padding, box.y + box.height - layout::Padding - layout::LineHeight}, ui.dim);
}
