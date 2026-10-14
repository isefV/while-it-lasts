#pragma once

/**
 * @file math.hpp
 * @brief Small numeric helpers for movement and fades.
 */

#include <algorithm>
#include <cmath>

namespace hollow
{
    /// Moves @p current towards @p target by at most @p step.
    inline float approach(float current, float target, float step)
    {
        if (current < target) return std::min(current + step, target);
        return std::max(current - step, target);
    }

    inline float lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    /// 0 at @p t = 0, 1 at @p t = 1, slow at both ends.
    inline float smoothstep(float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
} // namespace hollow
