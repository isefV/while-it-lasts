#pragma once

/**
 * @file hollow.hpp
 * @brief Public API of hollow. Replace with your own.
 *
 * Headers declare; the code lives in src/. Classes keep their private state
 * behind a d-pointer (`d_`), so it can change without changing the ABI.
 */

#include <hollow/export.hpp>

#include <memory>
#include <string>
#include <string_view>

namespace hollow
{
    /// Adds two numbers.
    HOLLOW_API int add(int a, int b);

    /// Returns a greeting, computed on a worker thread.
    HOLLOW_API std::string greet(std::string_view name);

    /**
     * @brief Sums the values it is given.
     *
     * An example of an exported class with a d-pointer.
     */
    class HOLLOW_API Counter
    {
    public:
        Counter();
        ~Counter();

        Counter(const Counter&) = delete;
        Counter& operator=(const Counter&) = delete;
        Counter(Counter&&) noexcept;
        Counter& operator=(Counter&&) noexcept;

        /// Adds @p value to the total.
        void add(int value);

        /// The sum of all values added so far.
        [[nodiscard]] long long total() const noexcept;

        /// How many values were added.
        [[nodiscard]] int count() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> d_;
    };
} // namespace hollow
