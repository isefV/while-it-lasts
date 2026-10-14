#include <hollow/utility/text.hpp>

namespace hollow
{
    std::string replaceTokens(std::string_view text, const TokenMap& tokens)
    {
        std::string out;
        out.reserve(text.size());

        std::size_t i = 0;
        while (i < text.size())
        {
            if (text[i] == '{')
            {
                const std::size_t close = text.find('}', i + 1);
                if (close != std::string_view::npos)
                {
                    const auto key = text.substr(i + 1, close - i - 1);
                    if (const auto it = tokens.find(key); it != tokens.end())
                    {
                        out += it->second;
                        i = close + 1;
                        continue;
                    }
                }
            }
            out += text[i];
            ++i;
        }
        return out;
    }

    namespace
    {
        std::size_t utf8Length(unsigned char lead)
        {
            if (lead < 0x80) return 1;
            if ((lead >> 5) == 0x6) return 2;
            if ((lead >> 4) == 0xE) return 3;
            if ((lead >> 3) == 0x1E) return 4;
            return 1;
        }

        // Cuts a word that is wider than a line into pieces that fit.
        void breakLongWord(std::string_view word, float maxWidth, const MeasureFn& measure,
                           std::vector<std::string>& lines, std::string& current)
        {
            std::size_t i = 0;
            while (i < word.size())
            {
                const std::size_t len = utf8Length(static_cast<unsigned char>(word[i]));
                std::string next = current + std::string(word.substr(i, len));
                if (!current.empty() && measure(next) > maxWidth)
                {
                    lines.push_back(current);
                    current.clear();
                    continue;
                }
                current = std::move(next);
                i += len;
            }
        }
    } // namespace

    std::vector<std::string> wrapText(std::string_view text, float maxWidth, const MeasureFn& measure,
                                      float firstIndent)
    {
        std::vector<std::string> lines;

        std::size_t start = 0;
        while (start <= text.size())
        {
            std::size_t end = text.find('\n', start);
            if (end == std::string_view::npos) end = text.size();
            const std::string_view paragraph = text.substr(start, end - start);

            std::string current;
            for (const auto& word : splitWords(paragraph))
            {
                const float width = lines.empty() ? maxWidth - firstIndent : maxWidth;
                const std::string candidate = current.empty() ? word : current + " " + word;

                if (measure(candidate) <= width)
                {
                    current = candidate;
                    continue;
                }
                if (!current.empty())
                {
                    lines.push_back(current);
                    current.clear();
                }
                if (measure(word) <= maxWidth)
                    current = word;
                else
                    breakLongWord(word, maxWidth, measure, lines, current);
            }
            lines.push_back(current);

            start = end + 1;
        }

        // A trailing empty line comes only from a final '\n'.
        while (lines.size() > 1 && lines.back().empty())
            lines.pop_back();
        return lines;
    }

    std::size_t utf8Count(std::string_view text)
    {
        std::size_t count = 0;
        for (std::size_t i = 0; i < text.size(); i += utf8Length(static_cast<unsigned char>(text[i])))
            ++count;
        return count;
    }

    std::string_view utf8Prefix(std::string_view text, std::size_t count)
    {
        std::size_t i = 0;
        while (i < text.size() && count > 0)
        {
            i += utf8Length(static_cast<unsigned char>(text[i]));
            --count;
        }
        return text.substr(0, i < text.size() ? i : text.size());
    }

    std::string_view trim(std::string_view text)
    {
        const auto first = text.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos) return {};
        const auto last = text.find_last_not_of(" \t\r\n");
        return text.substr(first, last - first + 1);
    }

    std::vector<std::string> splitWords(std::string_view text)
    {
        std::vector<std::string> words;
        std::size_t i = 0;
        while (i < text.size())
        {
            while (i < text.size() && (text[i] == ' ' || text[i] == '\t' || text[i] == '\r')) ++i;
            const std::size_t start = i;
            while (i < text.size() && text[i] != ' ' && text[i] != '\t' && text[i] != '\r') ++i;
            if (i > start) words.emplace_back(text.substr(start, i - start));
        }
        return words;
    }
} // namespace hollow
