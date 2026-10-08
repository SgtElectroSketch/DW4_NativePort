#pragma once

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

namespace dw4::content::detail {

using Json = nlohmann::json;

inline std::uint64_t unsigned_value(const Json& value, const std::uint64_t maximum, const std::string& field,
                                    const std::filesystem::path& path)
{
    const auto invalid = [&]() {
        return std::runtime_error("data field '" + field + "' must be an integer between 0 and " +
                                  std::to_string(maximum) + ": " + path.string());
    };
    if (!value.is_number_integer()) {
        throw invalid();
    }
    if (!value.is_number_unsigned() && value.get<std::int64_t>() < 0) {
        throw invalid();
    }
    const auto number = value.get<std::uint64_t>();
    if (number > maximum) {
        throw invalid();
    }
    return number;
}

inline std::uint64_t unsigned_field(const Json& record, const char* key, const std::uint64_t maximum,
                                    const std::string& field, const std::filesystem::path& path)
{
    const auto entry = record.find(key);
    if (entry == record.end()) {
        throw std::runtime_error("required data field '" + field + "' is missing: " + path.string());
    }
    return unsigned_value(*entry, maximum, field, path);
}

} // namespace dw4::content::detail