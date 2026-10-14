#include <hollow/utility/random.hpp>

namespace hollow
{
    Random::Random(unsigned seed)
        : engine_(seed)
    {
    }

    int Random::range(int lo, int hi)
    {
        if (hi < lo) return lo;
        return std::uniform_int_distribution<int>(lo, hi)(engine_);
    }

    float Random::uniform(float lo, float hi)
    {
        if (hi <= lo) return lo;
        return std::uniform_real_distribution<float>(lo, hi)(engine_);
    }

    bool Random::chance(float p)
    {
        return uniform() < p;
    }

    int NoRepeatPicker::pick(int count, Random& random)
    {
        if (count <= 0) return -1;
        if (count == 1 || last_ < 0 || last_ >= count)
        {
            last_ = random.range(0, count - 1);
            return last_;
        }

        // Pick among the others: skip over the previous index.
        int index = random.range(0, count - 2);
        if (index >= last_) ++index;
        last_ = index;
        return index;
    }
} // namespace hollow
