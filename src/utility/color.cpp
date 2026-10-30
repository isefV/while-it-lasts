#include <while_it_lasts/utility/color.hpp>

#include <algorithm>

namespace while_it_lasts
{
    namespace
    {
        int hexDigit(char c)
        {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        }

        int hexByte(std::string_view text, std::size_t at)
        {
            const int hi = hexDigit(text[at]);
            const int lo = hexDigit(text[at + 1]);
            return (hi < 0 || lo < 0) ? -1 : hi * 16 + lo;
        }
    } // namespace

    std::optional<Color> parseColor(std::string_view text)
    {
        if (text.empty() || text[0] != '#' || (text.size() != 7 && text.size() != 9))
            return std::nullopt;

        const int r = hexByte(text, 1);
        const int g = hexByte(text, 3);
        const int b = hexByte(text, 5);
        const int a = text.size() == 9 ? hexByte(text, 7) : 255;
        if (r < 0 || g < 0 || b < 0 || a < 0) return std::nullopt;

        return Color{static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                     static_cast<unsigned char>(b), static_cast<unsigned char>(a)};
    }

    Color multiply(Color a, Color b)
    {
        return Color{static_cast<unsigned char>(a.r * b.r / 255), static_cast<unsigned char>(a.g * b.g / 255),
                     static_cast<unsigned char>(a.b * b.b / 255), static_cast<unsigned char>(a.a * b.a / 255)};
    }

    Color mix(Color a, Color b, float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        const auto channel = [t](unsigned char x, unsigned char y) {
            return static_cast<unsigned char>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t + 0.5f);
        };
        return Color{channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b), channel(a.a, b.a)};
    }
} // namespace while_it_lasts
