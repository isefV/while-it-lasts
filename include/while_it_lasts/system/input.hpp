#pragma once

/**
 * @file input.hpp
 * @brief The keyboard, as game actions. The game has no mouse input.
 *
 *     Move          A / D, Left / Right
 *     Interact      E, with the thing the player stands next to
 *     Advance text  E, Space, Enter
 *     Choose        Up / Down (W / S) or 1 to 4; E or Enter to pick
 *     Inventory     Tab or I
 *     Pause         Esc
 *     Fighting      F swing, R shoot, Q dodge
 *     Grab          G: take hold of X lying on the ground, or let go
 */

#include <raylib.h>

namespace while_it_lasts
{
    enum class Action
    {
        Left,
        Right,
        Up,
        Down,
        Interact,
        Advance,
        Inventory,
        Pause,
        Confirm,
        Attack,
        Shoot,
        Dodge,
        Grab,
        Fullscreen,
    };

    class Input
    {
    public:
        /// Reads the keyboard; call once per frame.
        void update();

        [[nodiscard]] bool pressed(Action action) const;
        [[nodiscard]] bool down(Action action) const;

        /// 1 to 4 when that number key was pressed, else 0.
        [[nodiscard]] int number() const { return number_; }

        /// Stops this frame's presses from reaching anything else.
        void consume() { consumed_ = true; }

        // Virtual input, for scripted walkthroughs: added to the real keys
        // until clearVirtual().
        void setVirtualDown(Action action, bool down);
        void pressVirtual(Action action);
        void pressVirtualNumber(int number) { virtualNumber_ = number; }
        void clearVirtual();

    private:
        static constexpr int ActionCount = static_cast<int>(Action::Fullscreen) + 1;

        bool virtualDown_[ActionCount] = {};
        bool virtualPressed_[ActionCount] = {};
        int virtualNumber_ = 0;

        int number_ = 0;
        bool consumed_ = false;
    };
} // namespace while_it_lasts
