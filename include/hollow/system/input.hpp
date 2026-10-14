#pragma once

/**
 * @file input.hpp
 * @brief Keyboard and mouse, as game actions.
 *
 *     Move          A / D, Left / Right
 *     Interact      E (or a click on a thing while standing close to it)
 *     Advance text  E, Space, Enter, left click
 *     Choose        Up / Down (W / S), 1 to 4, mouse; E, Enter or a click to pick
 *     Inventory     Tab or I
 *     Pause         Esc
 */

#include <raylib.h>

namespace hollow
{
    class Canvas;

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
        Fullscreen,
    };

    class Input
    {
    public:
        /// Reads the keyboard and mouse; call once per frame.
        void update(const Canvas& canvas);

        [[nodiscard]] bool pressed(Action action) const;
        [[nodiscard]] bool down(Action action) const;

        /// Mouse position on the canvas.
        [[nodiscard]] Vector2 mouse() const { return mouse_; }
        [[nodiscard]] bool mouseMoved() const { return mouseMoved_; }
        [[nodiscard]] bool clicked() const { return clicked_; }

        /// 1 to 4 when that number key was pressed, else 0.
        [[nodiscard]] int number() const { return number_; }

        /// Stops this frame's presses from reaching anything else.
        void consume() { consumed_ = true; }

        // Virtual input, for scripted walkthroughs: added to the real keys and
        // mouse until clearVirtual().
        void setVirtualDown(Action action, bool down);
        void pressVirtual(Action action);
        void clickVirtual(Vector2 canvasPosition);
        void pressVirtualNumber(int number) { virtualNumber_ = number; }
        void clearVirtual();

    private:
        static constexpr int ActionCount = static_cast<int>(Action::Fullscreen) + 1;

        bool virtualDown_[ActionCount] = {};
        bool virtualPressed_[ActionCount] = {};
        bool virtualClick_ = false;
        Vector2 virtualMouse_{};
        bool hasVirtualMouse_ = false;
        int virtualNumber_ = 0;

        Vector2 mouse_{};
        bool mouseMoved_ = false;
        bool clicked_ = false;
        int number_ = 0;
        bool consumed_ = false;
    };
} // namespace hollow
