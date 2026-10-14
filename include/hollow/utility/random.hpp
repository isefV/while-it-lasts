#pragma once

/**
 * @file random.hpp
 * @brief Random numbers, and picks that never repeat back to back.
 */

#include <random>

namespace hollow
{
    class Random
    {
    public:
        explicit Random(unsigned seed = std::random_device{}());

        /// A whole number in [lo, hi], both included.
        int range(int lo, int hi);

        /// A number in [lo, hi).
        float uniform(float lo = 0.0f, float hi = 1.0f);

        /// true with probability @p p (0 to 1).
        bool chance(float p);

    private:
        std::mt19937 engine_;
    };

    /**
     * @brief Picks an index in [0, count) that differs from the previous pick.
     *
     * For random short lines ("barks"): one picker per pool, so the same line
     * never plays twice in a row.
     */
    class NoRepeatPicker
    {
    public:
        int pick(int count, Random& random);
        void reset() noexcept { last_ = -1; }

    private:
        int last_ = -1;
    };
} // namespace hollow
