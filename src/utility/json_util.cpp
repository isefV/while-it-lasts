#include "json_util.hpp"

#include <hollow/utility/color.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace hollow::detail
{
    std::optional<Json> parseJson(std::string_view text, const std::string& display, Diagnostics& diag)
    {
        try
        {
            return Json::parse(text.begin(), text.end(), nullptr, true, /*ignore_comments=*/true);
        }
        catch (const Json::parse_error& e)
        {
            // e.byte counts from 1; turn it into a line and column.
            const std::size_t at = std::min<std::size_t>(e.byte > 0 ? e.byte - 1 : 0, text.size());
            int line = 1;
            int column = 1;
            for (std::size_t i = 0; i < at; ++i)
            {
                if (text[i] == '\n')
                {
                    ++line;
                    column = 1;
                }
                else
                {
                    ++column;
                }
            }

            // nlohmann's message repeats the position; keep only the reason.
            std::string reason = e.what();
            if (const auto colon = reason.find("syntax error"); colon != std::string::npos)
                reason = reason.substr(colon);

            diag.error(display + ":" + std::to_string(line) + ":" + std::to_string(column) + ": " + reason);
            return std::nullopt;
        }
    }

    std::optional<Json> loadJsonFile(const std::string& path, const std::string& display, Diagnostics& diag)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            diag.error(display + ": cannot open the file (" + path + ")");
            return std::nullopt;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return parseJson(buffer.str(), display, diag);
    }

    std::string child(const std::string& where, std::string_view key)
    {
        return where + "." + std::string(key);
    }

    std::string child(const std::string& where, std::size_t index)
    {
        return where + "[" + std::to_string(index) + "]";
    }

    std::string typeName(const Json& value)
    {
        switch (value.type())
        {
        case Json::value_t::null: return "null";
        case Json::value_t::object: return "an object";
        case Json::value_t::array: return "an array";
        case Json::value_t::string: return "a string";
        case Json::value_t::boolean: return "true/false";
        case Json::value_t::number_integer:
        case Json::value_t::number_unsigned:
        case Json::value_t::number_float: return "a number";
        default: return "an unknown value";
        }
    }

    namespace
    {
        const Json* find(const Json& obj, std::string_view key)
        {
            if (!obj.is_object()) return nullptr;
            const auto it = obj.find(std::string(key));
            return it == obj.end() ? nullptr : &*it;
        }

        void wrongType(const std::string& where, std::string_view key, std::string_view expected, const Json& value,
                       Diagnostics& diag)
        {
            diag.error(child(where, key) + ": expected " + std::string(expected) + ", found " + typeName(value));
        }
    } // namespace

    std::string readString(const Json& obj, std::string_view key, const std::string& where, Diagnostics& diag,
                           std::string fallback)
    {
        const Json* value = find(obj, key);
        if (!value) return fallback;
        if (!value->is_string())
        {
            wrongType(where, key, "a string", *value, diag);
            return fallback;
        }
        return value->get<std::string>();
    }

    float readNumber(const Json& obj, std::string_view key, const std::string& where, Diagnostics& diag,
                     float fallback)
    {
        const Json* value = find(obj, key);
        if (!value) return fallback;
        if (!value->is_number())
        {
            wrongType(where, key, "a number", *value, diag);
            return fallback;
        }
        return value->get<float>();
    }

    int readInt(const Json& obj, std::string_view key, const std::string& where, Diagnostics& diag, int fallback)
    {
        const Json* value = find(obj, key);
        if (!value) return fallback;
        if (value->is_boolean()) return value->get<bool>() ? 1 : 0;
        if (!value->is_number_integer())
        {
            wrongType(where, key, "a whole number", *value, diag);
            return fallback;
        }
        return value->get<int>();
    }

    bool readBool(const Json& obj, std::string_view key, const std::string& where, Diagnostics& diag, bool fallback)
    {
        const Json* value = find(obj, key);
        if (!value) return fallback;
        if (!value->is_boolean())
        {
            wrongType(where, key, "true or false", *value, diag);
            return fallback;
        }
        return value->get<bool>();
    }

    Color readColor(const Json& obj, std::string_view key, const std::string& where, Diagnostics& diag,
                    Color fallback)
    {
        const Json* value = find(obj, key);
        if (!value) return fallback;
        if (value->is_string())
        {
            if (const auto color = parseColor(value->get<std::string>())) return *color;
        }
        diag.error(child(where, key) + ": expected a colour such as \"#1b2230\", found " +
                   (value->is_string() ? "\"" + value->get<std::string>() + "\"" : typeName(*value)));
        return fallback;
    }

    std::vector<std::string> readStrings(const Json& obj, std::string_view key, const std::string& where,
                                         Diagnostics& diag)
    {
        std::vector<std::string> out;
        const Json* value = find(obj, key);
        if (!value) return out;
        if (value->is_string())
        {
            out.push_back(value->get<std::string>());
            return out;
        }
        if (!value->is_array())
        {
            wrongType(where, key, "a list of strings", *value, diag);
            return out;
        }
        for (std::size_t i = 0; i < value->size(); ++i)
        {
            const Json& item = (*value)[i];
            if (item.is_string())
                out.push_back(item.get<std::string>());
            else
                diag.error(child(child(where, key), i) + ": expected a string, found " + typeName(item));
        }
        return out;
    }

    void checkKeys(const Json& obj, std::initializer_list<std::string_view> known, const std::string& where,
                   Diagnostics& diag)
    {
        if (!obj.is_object()) return;
        for (const auto& [key, value] : obj.items())
        {
            if (!key.empty() && key[0] == '_') continue;
            if (std::find(known.begin(), known.end(), key) == known.end())
                diag.error(child(where, key) + ": unknown key \"" + key + "\"");
        }
    }
} // namespace hollow::detail
