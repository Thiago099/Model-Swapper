#pragma once

#include "Application/Model.h"
#include "Lib/Singleton.h"

class ModelSwapManager : public Singleton<ModelSwapManager> {
    models sources;


    variantId PickRandomVariant(variants& source, const uint32_t seed);

    bool DoesTemporalOverrideStopTheReplacement(variant* item);

public:
    void Apply(RE::TESForm* base, variantId variant);
    const variantId Process(RE::TESForm* base, RefID id);
    void Register(std::string key, variants value);
    int32_t PickVariant(const char* str, const uint32_t seed);
    variant* GetVariant(const char* str, const uint32_t variant);
};
