#include "Manager.h"

void Manager::Register(std::string key, variants value) {
    if (value.empty()) {
        return;
    }
    key = Str::processString(key);

    const bool scoped = value.front()->filter.IsScoped();
    hasScopedRules = hasScopedRules || scoped;

    auto it = sources.find(key);
    if (it != sources.end()) {
        it->second.insert(it->second.end(), value.begin(), value.end());
    } else {
        sources[key] = value;
    }

    std::set<std::string> aliases;
    for (auto item : value) {
        if (!aliases.insert(item->key).second) {
            continue;
        }
        auto& aliasVariants = sources[item->key];
        if (scoped) {
            if (item->key != key) {
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
