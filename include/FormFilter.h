#pragma once

#include <charconv>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <nlohmann/json.hpp>

struct FormFilter {
    std::optional<std::uint32_t> baseFormID;
    std::optional<std::uint32_t> referenceID;

    bool Matches(std::uint32_t baseID, std::uint32_t refID) const {
        return (!baseFormID || *baseFormID == baseID) &&
               (!referenceID || *referenceID == refID);
    }

    bool IsScoped() const {
        return baseFormID.has_value() || referenceID.has_value();
    }
};

namespace FormFilters {
    using Resolver = std::function<std::uint32_t(std::uint32_t, std::string_view)>;

    inline bool ParseHex(std::string_view text, std::uint32_t& result) {
        if (text.starts_with("0x") || text.starts_with("0X")) {
            text.remove_prefix(2);
        }
        if (text.empty()) {
            return false;
        }
        auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result, 16);
        return error == std::errc{} && end == text.data() + text.size() && result != 0;
    }

    inline bool ParseID(const nlohmann::json& value, std::uint32_t& result, const Resolver& resolve = {}) {
        if (value.is_number_unsigned()) {
            auto id = value.get<std::uint64_t>();
            if (id == 0 || id > std::numeric_limits<std::uint32_t>::max()) {
                return false;
            }
            result = static_cast<std::uint32_t>(id);
            return true;
        }
        if (value.is_number_integer()) {
            auto id = value.get<std::int64_t>();
            if (id <= 0 || id > std::numeric_limits<std::uint32_t>::max()) {
                return false;
            }
            result = static_cast<std::uint32_t>(id);
            return true;
        }
        if (!value.is_string()) {
            return false;
        }

        const auto& text = value.get_ref<const std::string&>();
        auto separator = text.find('|');
        if (separator == std::string::npos) {
            return ParseHex(text, result);
        }
        if (separator == 0 || !resolve) {
            return false;
        }

        std::uint32_t localID;
        if (!ParseHex(std::string_view(text).substr(separator + 1), localID) || localID > 0xFFFFFF) {
            return false;
        }
        result = resolve(localID, std::string_view(text).substr(0, separator));
        return result != 0;
    }

    inline bool ReadID(const nlohmann::json& entry, std::string_view field,
                      std::optional<std::uint32_t>& result, const Resolver& resolve = {}) {
        auto value = entry.find(std::string(field));
        if (value == entry.end()) {
            result.reset();
            return true;
        }

        std::uint32_t id;
        if (!ParseID(*value, id, resolve)) {
            return false;
        }
        result = id;
        return true;
    }
}
