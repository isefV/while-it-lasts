#include <hollow/hollow.hpp>

#include <future>

namespace hollow
{
    int add(int a, int b)
    {
        return a + b;
    }

    std::string greet(std::string_view name)
    {
        // Uses Threads (a private dependency of the library).
        auto greeting = std::async(std::launch::async, [name] {
            return "Hello, " + std::string(name) + "!";
        });

        return greeting.get();
    }

    struct Counter::Impl
    {
        long long total = 0;
        int count = 0;
    };

    Counter::Counter()
        : d_(std::make_unique<Impl>())
    {
    }

    Counter::~Counter() = default;
    Counter::Counter(Counter&&) noexcept = default;
    Counter& Counter::operator=(Counter&&) noexcept = default;

    void Counter::add(int value)
    {
        d_->total += value;
        ++d_->count;
    }

    long long Counter::total() const noexcept
    {
        return d_->total;
    }

    int Counter::count() const noexcept
    {
        return d_->count;
    }
} // namespace hollow
