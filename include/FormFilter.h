#pragma once

#include <algorithm>
#include <array>
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

struct FormFilterContext {
    std::vector<std::uint32_t> locations;
    std::vector<std::uint32_t> regions;
    std::vector<std::uint32_t> worldspaces;
    std::uint32_t cellID = 0;
    std::function<bool(std::uint32_t)> hasKeyword;
};

struct FormFilter {
    using IDs = std::optional<std::vector<std::uint32_t>>;

    IDs baseFormID;
    IDs referenceID;
    IDs excludeBaseFormID;
    IDs excludeReferenceID;
    IDs locationID;
    IDs regionID;
    IDs keywordID;
    IDs cellID;
    IDs worldspaceID;
    IDs excludeLocationID;
    IDs excludeRegionID;
    IDs excludeKeywordID;
    IDs excludeCellID;
    IDs excludeWorldspaceID;

    bool Matches(std::uint32_t baseID, std::uint32_t refID,
                 const FormFilterContext* context = nullptr) const {
        if ((baseFormID && !Contains(baseFormID, baseID)) ||
            (referenceID && !Contains(referenceID, refID)) ||
            Contains(excludeBaseFormID, baseID) || Contains(excludeReferenceID, refID)) {
            return false;
        }
        if (!HasContextFilters()) {
            return true;
        }
        if (!context) {
            return false;
        }

        if (Intersects(excludeLocationID, context->locations) ||
            Intersects(excludeRegionID, context->regions) ||
            MatchesKeyword(excludeKeywordID, *context) ||
            Contains(excludeCellID, context->cellID) ||
            Intersects(excludeWorldspaceID, context->worldspaces)) {
            return false;
        }

        const bool hasInclusions = locationID || regionID || keywordID || cellID || worldspaceID;
        return !hasInclusions || Intersects(locationID, context->locations) ||
               Intersects(regionID, context->regions) || MatchesKeyword(keywordID, *context) ||
               Contains(cellID, context->cellID) || Intersects(worldspaceID, context->worldspaces);
    }

    bool HasContextFilters() const {
        return locationID || regionID || keywordID || cellID || worldspaceID ||
               Nonempty(excludeLocationID) || Nonempty(excludeRegionID) ||
               Nonempty(excludeKeywordID) || Nonempty(excludeCellID) || Nonempty(excludeWorldspaceID);
    }

    bool IsScoped() const {
        return baseFormID.has_value() || referenceID.has_value() ||
               (excludeBaseFormID && !excludeBaseFormID->empty()) ||
               (excludeReferenceID && !excludeReferenceID->empty()) || HasContextFilters();
    }

private:
    static bool Contains(const IDs& ids, std::uint32_t id) {
        return ids && std::find(ids->begin(), ids->end(), id) != ids->end();
    }

    static bool Nonempty(const IDs& ids) {
        return ids && !ids->empty();
    }

    static bool Intersects(const IDs& ids, const std::vector<std::uint32_t>& values) {
        return ids && std::any_of(values.begin(), values.end(), [&](auto id) { return Contains(ids, id); });
    }

    static bool MatchesKeyword(const IDs& ids, const FormFilterContext& context) {
        return ids && context.hasKeyword &&
               std::any_of(ids->begin(), ids->end(), context.hasKeyword);
    }
};

namespace FormFilters {
    using Resolver = std::function<std::uint32_t(std::uint32_t, std::string_view)>;

    enum class ContextType { Location, Region, Keyword, Cell, Worldspace };
    struct ContextField {
        std::string_view name;
        FormFilter::IDs FormFilter::* member;
        ContextType type;
    };
    inline constexpr std::array<ContextField, 10> ContextFields{{
        {"LocationID", &FormFilter::locationID, ContextType::Location},
        {"RegionID", &FormFilter::regionID, ContextType::Region},
        {"KeywordID", &FormFilter::keywordID, ContextType::Keyword},
        {"CellID", &FormFilter::cellID, ContextType::Cell},
        {"WorldspaceID", &FormFilter::worldspaceID, ContextType::Worldspace},
        {"ExcludeLocationID", &FormFilter::excludeLocationID, ContextType::Location},
        {"ExcludeRegionID", &FormFilter::excludeRegionID, ContextType::Region},
        {"ExcludeKeywordID", &FormFilter::excludeKeywordID, ContextType::Keyword},
        {"ExcludeCellID", &FormFilter::excludeCellID, ContextType::Cell},
        {"ExcludeWorldspaceID", &FormFilter::excludeWorldspaceID, ContextType::Worldspace}
    }};

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
