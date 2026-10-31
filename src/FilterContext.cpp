#include "FilterContext.h"

namespace FormFilters {
    FormFilterContext CaptureContext(const RE::TESObjectREFR* reference) {
        FormFilterContext context;
        if (!reference) {
            return context;
        }

        const auto location = reference->GetCurrentLocation();
        for (auto current = location; current; current = current->parentLoc) {
            const auto id = current->GetFormID();
            if (std::find(context.locations.begin(), context.locations.end(), id) != context.locations.end()) {
                break;
            }
            context.locations.push_back(id);
        }

        auto cell = reference->GetParentCell();
        if (!cell) {
            cell = reference->GetSaveParentCell();
        }
        if (cell) {
            context.cellID = cell->GetFormID();
            if (auto regions = cell->GetRegionList(false)) {
                for (auto region : *regions) {
                    if (region) {
                        context.regions.push_back(region->GetFormID());
                    }
                }
            }
        }

        if (auto worldspace = reference->GetWorldspace()) {
            context.worldspaces.push_back(worldspace->GetFormID());
            if (worldspace->parentWorld) {
                context.worldspaces.push_back(worldspace->parentWorld->GetFormID());
            }
        }

        // Use the reference API as BOS does, including its keyword helper behavior.
        // Only the current location contributes keywords; its parents do not.
        context.hasKeyword = [reference, location](std::uint32_t id) {
            const auto keyword = RE::TESForm::LookupByID<RE::BGSKeyword>(id);
            return keyword && ((location && location->HasKeyword(keyword)) || reference->HasKeyword(keyword));
        };
        return context;
    }

    bool ValidateContextIDs(const FormFilter& filter, std::string_view& invalidField) {
        for (const auto& field : ContextFields) {
            const auto& ids = filter.*(field.member);
            if (!ids) {
                continue;
            }
            for (const auto id : *ids) {
                const auto form = RE::TESForm::LookupByID(id);
                // BOS retains cell IDs that are not materialized at data load.
                // CaptureContext later supplies the actual, typed cell's ID.
                if (!form && field.type == ContextType::Cell) {
                    continue;
                }
                bool valid = false;
                if (form) {
                    switch (field.type) {
                    case ContextType::Location: valid = form->As<RE::BGSLocation>() != nullptr; break;
                    case ContextType::Region: valid = form->As<RE::TESRegion>() != nullptr; break;
                    case ContextType::Keyword: valid = form->As<RE::BGSKeyword>() != nullptr; break;
                    case ContextType::Cell: valid = form->As<RE::TESObjectCELL>() != nullptr; break;
                    case ContextType::Worldspace: valid = form->As<RE::TESWorldSpace>() != nullptr; break;
                    }
                }
                if (!valid) {
                    invalidField = field.name;
                    return false;
                }
            }
        }
        invalidField = {};
        return true;
    }
}
