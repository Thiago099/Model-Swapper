#include "Manager.h"

void Manager::Register(std::string key, variants value) {
    Register(std::vector<std::string>{std::move(key)}, std::move(value));
}

void Manager::Register(std::vector<std::string> keys, variants value) {
    if (value.empty()) {
        return;
    }

    const bool scoped = value.front()->filter.IsScoped();

    if (keys.empty()) {
        // The empty key holds ID-only rules, independent of the current path.
        hasScopedRules = hasScopedRules || scoped;
        auto& idVariants = sources[""];
        idVariants.insert(idVariants.end(), value.begin(), value.end());
        return;
    }

    std::set<std::string> originalKeys;
    for (const auto& key : keys) {
        auto normalized = Str::processString(key);
        if (normalized.empty()) {
            return;
        }
        originalKeys.insert(std::move(normalized));
    }
    hasScopedRules = hasScopedRules || scoped;
    for (const auto& key : originalKeys) {
        auto& originalVariants = sources[key];
        originalVariants.insert(originalVariants.end(), value.begin(), value.end());
    }

    std::set<std::string> aliases;
    for (auto item : value) {
        if (item->key[0] == '\0' || !aliases.insert(item->key).second) {
            continue;
        }
        auto& aliasVariants = sources[item->key];
        if (scoped) {
            if (!originalKeys.contains(item->key)) {
                aliasVariants.insert(aliasVariants.end(), value.begin(), value.end());
            }
        } else {
            // Keep legacy alias replacement without discarding scoped rules.
            std::erase_if(aliasVariants, [](variant* existing) { return !existing->filter.IsScoped(); });
            aliasVariants.insert(aliasVariants.end(), value.begin(), value.end());
        }
    }
}

void Manager::Process(RE::TESBoundObject* base, RE::FormID id) {
    auto wrapper = Swap(base, id);
}

void Manager::Process(RE::TESObjectARMA* base, RE::FormID id) {
    auto wrapper = Swap(base, id, base ? base->GetFormID() : 0);
}

std::unique_ptr<AVObject> Manager::Swap(RE::TESBoundObject* base, RE::FormID id) {
    if (!base) {
        return nullptr;
    }
    auto wrapper = std::make_unique<AVModel>(base);
    wrapper->Match(sources, id, base->GetFormID());
    return wrapper;
}

std::unique_ptr<AVObject> Manager::Swap(RE::TESObjectARMA* base, RE::FormID id, RE::FormID baseFormID) {
    if (!base) {
        return nullptr;
    }
    auto wrapper = std::make_unique<AVObjectARMA>(base);
    wrapper->Match(sources, id, baseFormID);
    return wrapper;
}
