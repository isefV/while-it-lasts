#pragma once

// Private to the library: reading the JSON data files with nlohmann/json.
// Public headers never include this, so nlohmann/json stays an internal
// dependency.

#include <while_it_lasts/utility/diagnostics.hpp>

#include <raylib.h>

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace while_it_lasts::detail
{
    // ordered_json keeps the order the writer used (conversations, rooms,
    // items in the inventory).
    using Json = nlohmann::ordered_json;

    /// Parses @p path. // and /* */ comments are allowed. A syntax error is
    /// reported as "<display>:<line>:<column>: ...".
    std::optional<Json> loadJsonFile(const std::string& path, const std::string& display, Diagnostics& diag);

    /// Parses JSON text (used by the tests), with the same error format.
    std::optional<Json> parseJson(std::string_view text, const std::string& display, Diagnostics& diag);

    /// "where.key" or "where[3]": the path to a value, for messages.
    std::string child(const std::string& where, std::string_view key);
    std::string child(const std::string& where, std::size_t index);

    /// Name of a JSON value's type, for messages ("a string", "an array").
    std::string typeName(const Json& value);

    // Typed reads of obj[key]. A missing key gives the fallback silently; a
    // value of the wrong type gives the fallback and an error naming where.
    std::string readString(const Json& obj, std::string_view key, const std::string& where, Diagnostics& diag,
                           std::string fallback = {});
    float readNumber(const Json& obj, std::string_view key, const std::string& where, Diagnostics& diag,
                     float fallback = 0.0f);
    int readInt(const Json& obj, std::string_view key, const std::string& where, Diagnostics& diag,
                int fallback = 0);
    bool readBool(const Json& obj, std::string_view key, const std::string& where, Diagnostics& diag,
                  bool fallback = false);
    Color readColor(const Json& obj, std::string_view key, const std::string& where, Diagnostics& diag,
                    Color fallback);
    std::vector<std::string> readStrings(const Json& obj, std::string_view key, const std::string& where,
                                         Diagnostics& diag);

    /// Reports keys of @p obj that are not in @p known (typos such as
    /// "dialog" for "dialogue"). Keys starting with "_" are comments.
    void checkKeys(const Json& obj, std::initializer_list<std::string_view> known, const std::string& where,
                   Diagnostics& diag);
} // namespace while_it_lasts::detail
