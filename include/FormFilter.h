#pragma once

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>

struct FormFilter {
    using IDs = std::optional<std::vector<std::uint32_t>>;

    IDs baseFormID;
    IDs referenceID;
    IDs excludeBaseFormID;
    IDs excludeReferenceID;

    bool Matches(std::uint32_t baseID, std::uint32_t refID) const {
        return (!baseFormID || Contains(baseFormID, baseID)) &&
               (!referenceID || Contains(referenceID, refID)) &&
               !Contains(excludeBaseFormID, baseID) &&
               !Contains(excludeReferenceID, refID);
    }

    bool IsScoped() const {
        return baseFormID.has_value() || referenceID.has_value() ||
               (excludeBaseFormID && !excludeBaseFormID->empty()) ||
               (excludeReferenceID && !excludeReferenceID->empty());
    }

private:
    static bool Contains(const IDs& ids, std::uint32_t id) {
        return ids && std::find(ids->begin(), ids->end(), id) != ids->end();
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

    inline bool ReadIDs(const nlohmann::json& entry, std::string_view field,
                       FormFilter::IDs& result, const Resolver& resolve = {}) {
        auto value = entry.find(std::string(field));
        if (value == entry.end()) {
            result.reset();
            return true;
        }

        std::vector<std::uint32_t> ids;
        if (value->is_array()) {
            ids.reserve(value->size());
            for (const auto& element : *value) {
                std::uint32_t id;
                if (!ParseID(element, id, resolve)) {
                    return false;
                }
                ids.push_back(id);
            }
        } else {
            std::uint32_t id;
            if (!ParseID(*value, id, resolve)) {
                return false;
            }
            ids.push_back(id);
        }
        result = std::move(ids);
        return true;
    }
}
