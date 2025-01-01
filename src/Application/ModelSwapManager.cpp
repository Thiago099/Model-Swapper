#include "Application/ModelSwapManager.h"
#include "Adaptors/Serialization.h"
#include "Application/AVObjects.h"

void ModelSwapManager::Apply(RE::TESForm* base, variantId variant) {

    if (variant == -1) {
		return;
	}

    if (auto obj = base->As<RE::TESObjectARMO>()) {
        ApplyImpl(obj, variant);
    } else if (auto obj = base->As<RE::TESObjectARMA>()) {
        ApplyImpl(obj, variant);
    } else if (auto obj = base->As<RE::TESObjectWEAP>()) {
        ApplyImpl(obj, variant);
    } else if (auto model = base->As<RE::TESModel>()) {
        ApplyImpl(base, variant);
    }
}
const int32_t ModelSwapManager::Process(RE::TESForm* base, const RefID id) {
    int32_t result = -1;

    if (auto obj = base->As<RE::TESObjectARMO>()) {
        result = ProcessImpl(obj, id);
    } else if (auto obj = base->As<RE::TESObjectARMA>()) {
        result = ProcessImpl(obj, id);
    } else if (auto obj = base->As<RE::TESObjectWEAP>()) {
        result = ProcessImpl(obj, id);
    } else if (auto model = base->As<RE::TESModel>()) {
        result = ProcessImpl(base, id);
    }

    return result;
}

int32_t ModelSwapManager::ProcessImpl(RE::TESObjectARMA* base, const RE::FormID id) const {
    const auto wrapper = new AVObjectARMA(base);
    int32_t result = wrapper->GetVariant(sources, id);
    delete wrapper;
    return result;
}

int32_t ModelSwapManager::ProcessImpl(RE::TESObjectARMO* base, RE::FormID id) const {
    const auto wrapper = new AVObjectARMO(base);
    int32_t result = wrapper->GetVariant(sources, id);
    delete wrapper;
    return result;
}

int32_t ModelSwapManager::ProcessImpl(RE::TESObjectWEAP* base, RE::FormID id) const {
    const auto wrapper = new AVObjectWEAP(base);
    int32_t result = wrapper->GetVariant(sources, id);
    delete wrapper;
    return result;
}

int32_t ModelSwapManager::ProcessImpl(RE::TESForm* base, RE::FormID id) const {
    const auto wrapper = new AVModel(base);
    int32_t result = wrapper->GetVariant(sources, id);
    delete wrapper;
    return result;
}

void ModelSwapManager::ApplyImpl(RE::TESObjectARMA* base, variantId id) const {
    const auto wrapper = new AVObjectARMA(base);
    if (id != -1) {
        wrapper->Apply(sources, id);
    }
    delete wrapper;
}

void ModelSwapManager::ApplyImpl(RE::TESObjectARMO* base, variantId id) const {
    const auto wrapper = new AVObjectARMO(base);
    if (id != -1) {
        wrapper->Apply(sources, id);
    }
    delete wrapper;
}

void ModelSwapManager::ApplyImpl(RE::TESObjectWEAP* base, variantId id) const {
    const auto wrapper = new AVObjectWEAP(base);
    if (id != -1) {
        wrapper->Apply(sources, id);
    }
    delete wrapper;
}

void ModelSwapManager::ApplyImpl(RE::TESForm* base, variantId id) const {
    const auto wrapper = new AVModel(base);
    if (id != -1) {
        wrapper->Apply(sources, id);
    }
    delete wrapper;
}

void ModelSwapManager::Register(std::string key, variants value) {
    key = Str::processString(key);

    if (const auto it = sources.find(key); it != sources.end()) {
        it->second.insert(it->second.end(), value.begin(), value.end());
    } else {
        sources[key] = value;
    }

    for (const auto item : value) {
        sources[item->key] = value;
    }
}
