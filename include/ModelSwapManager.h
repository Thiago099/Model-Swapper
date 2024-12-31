#pragma once
#include "Model.h"
#include "Singleton.h"
class ModelSwapManager : public Singleton<ModelSwapManager> {
    models sources;
    void ApplyImpl(RE::TESObjectARMA* base, variantId id) const;
    void ApplyImpl(RE::TESObjectARMO* base, variantId id) const;
    void ApplyImpl(RE::TESObjectWEAP* base, variantId id) const;
    void ApplyImpl(RE::TESForm* base, variantId id) const;
    variantId ProcessImpl(RE::TESForm* base, RE::FormID id) const;
    variantId ProcessImpl(RE::TESObjectARMA* base, RE::FormID id) const;
    variantId ProcessImpl(RE::TESObjectARMO* base, RE::FormID id) const;
    variantId ProcessImpl(RE::TESObjectWEAP* base, RE::FormID id) const;

public:
    void Apply(RE::TESForm* base, variantId variant);
    const variantId Process(RE::TESForm* base, RefID id);
    void Register(std::string key, variants value);
};
