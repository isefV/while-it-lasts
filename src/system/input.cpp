#include <hollow/system/input.hpp>

#include <span>

namespace hollow
{
    namespace
    {
        std::span<const int> keysFor(Action action)
        {
            static constexpr int left[] = {KEY_A, KEY_LEFT};
            static constexpr int right[] = {KEY_D, KEY_RIGHT};
            static constexpr int up[] = {KEY_W, KEY_UP};
            static constexpr int down[] = {KEY_S, KEY_DOWN};
            static constexpr int interact[] = {KEY_E};
            static constexpr int advance[] = {KEY_E, KEY_SPACE, KEY_ENTER, KEY_KP_ENTER};
            static constexpr int inventory[] = {KEY_TAB, KEY_I};
            static constexpr int pause[] = {KEY_ESCAPE};
            static constexpr int fullscreen[] = {KEY_F11};

            switch (action)
            {
            case Action::Left: return left;
            case Action::Right: return right;
            case Action::Up: return up;
            case Action::Down: return down;
            case Action::Interact: return interact;
            case Action::Advance:
            case Action::Confirm: return advance;
            case Action::Inventory: return inventory;
            case Action::Pause: return pause;
            case Action::Fullscreen: return fullscreen;
            }
            return {};
        }
    } // namespace

    void Input::update()
    {
        consumed_ = false;
        number_ = virtualNumber_;
        for (int n = 1; n <= 4; ++n)
            if (IsKeyPressed(KEY_ONE + n - 1) || IsKeyPressed(KEY_KP_1 + n - 1)) number_ = n;
    }

    void Input::setVirtualDown(Action action, bool down)
    {
        virtualDown_[static_cast<int>(action)] = down;
    }

    void Input::pressVirtual(Action action)
    {
        virtualPressed_[static_cast<int>(action)] = true;
    }

    void Input::clearVirtual()
    {
        for (int i = 0; i < ActionCount; ++i)
            virtualDown_[i] = virtualPressed_[i] = false;
        virtualNumber_ = 0;
    }

    bool Input::pressed(Action action) const
    {
        if (consumed_) return false;
        if (virtualPressed_[static_cast<int>(action)]) return true;
        for (const int key : keysFor(action))
            if (IsKeyPressed(key)) return true;
        return false;
    }

    bool Input::down(Action action) const
    {
        if (virtualDown_[static_cast<int>(action)]) return true;
        for (const int key : keysFor(action))
            if (IsKeyDown(key)) return true;
        return false;
    }
} // namespace hollow
